#include "include/editor.h"
#include <QWebFrame>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QDir>
#include <QCoreApplication>

Editor::Editor(QWidget *parent) :
    QWidget(parent), ready(false), m_fileName("")
{

#ifdef QT_DEBUG
    QWebSettings::globalSettings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);
#endif

    this->jsToCppProxy = new JsToCppProxy();
    connect(this->jsToCppProxy,
            SIGNAL(messageReceived(QString,QVariant)),
            this,
            SLOT(on_proxyMessageReceived(QString,QVariant)));

    QString editorPath = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/" + "editor/index.html");

    this->webView = new QWebView();
    this->webView->setUrl(QUrl("file://" + editorPath));
    this->webView->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(this->webView);
    this->setLayout(layout);

    connect(this->webView->page()->mainFrame(), SIGNAL(javaScriptWindowObjectCleared()), this, SLOT(on_javaScriptWindowObjectCleared()));
}

Editor::~Editor()
{
    delete webView;
    delete jsToCppProxy;
}

void Editor::on_javaScriptWindowObjectCleared()
{
    this->webView->page()->mainFrame()->addToJavaScriptWindowObject("cpp_ui_driver", this->jsToCppProxy);
}

void Editor::on_proxyMessageReceived(QString msg, QVariant data)
{
    emit messageReceived(msg, data);

    if(msg == "J_EVT_READY") {
        this->ready = true;
        while(!this->outMessagesQueue.isEmpty()) {
            this->sendMessage(this->outMessagesQueue.dequeue(),
                              this->outMessagesDataQueue.dequeue());
        }
    } else if(msg == "J_EVT_CONTENT_CHANGED")
        emit contentChanged();
    else if(msg == "J_EVT_CLEAN_CHANGED")
        emit cleanChanged(data.toBool());
}

void Editor::setFocus()
{
    this->webView->setFocus();
}

void Editor::setFileName(QString filename)
{
    m_fileName = filename;
}

QString Editor::fileName()
{
    return m_fileName;
}

bool Editor::isClean()
{
    return this->sendMessageWithResult("C_FUN_IS_CLEAN", 0).toBool();
}

QString Editor::jsStringEscape(QString str) {
    return str.replace("\\", "\\\\")
            .replace("'", "\\'")
            .replace("\"", "\\\"")
            .replace("\n", "\\n")
            .replace("\r", "\\r")
            .replace("\t", "\\t")
            .replace("\b", "\\b");
}

void Editor::sendMessage(QString msg, QVariant data)
{
    if (!ready) {
        this->outMessagesQueue.enqueue(msg);
        this->outMessagesDataQueue.enqueue(data);
        return;
    }

    this->sendMessageWithResult(msg, data);
}

QVariant Editor::sendMessageWithResult(QString msg, QVariant data)
{
    while (!ready) {
        // FIXME Avoid active wait
    }

    QString funCall = "UiDriver.messageReceived('" +
            jsStringEscape(msg) + "');";

    this->jsToCppProxy->setMsgData(data);

    return this->webView->page()->mainFrame()->evaluateJavaScript(funCall);
}
