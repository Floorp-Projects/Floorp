#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <qmainwindow.h>

class QGeckoEmbed;

class MyMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MyMainWindow();

public slots:
    void fileOpen();
    void startURIOpen(const QString &, bool &);
public:
    QGeckoEmbed *qecko;
};

#endif
