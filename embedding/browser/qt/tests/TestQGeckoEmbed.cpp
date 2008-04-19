#include <qapplication.h>
#include "mainwindow.h"
#include "qgeckoembed.h"

#include <qdir.h>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QGeckoEmbed::initialize(QDir::homeDirPath().latin1(),
                            ".TestQGeckoEmbed");

    MyMainWindow *mainWindow = new MyMainWindow();
    app.setMainWidget(mainWindow);

    mainWindow->resize(700, 500);
    mainWindow->show();

    QString url;
    if (argc > 1)
        url = argv[1];
    else
        url = "http://www.kde.org";

    mainWindow->qecko->loadURL(url);

    return app.exec();
}
