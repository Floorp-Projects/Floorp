#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <qmainwindow.h>

class QGeckoEmbed;
class QLineEdit;

class MyMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MyMainWindow();

public slots:
    void fileOpen();
    void startURIOpen(const QString &, bool &);
    void changeLocation();
    void goBack();
    void goForward();
    void stop();

public:
    QGeckoEmbed *qecko;

private slots:
    void slotProgress(int, int);
    void slotProgress(const QString &, int, int);

private:
    QLineEdit *location;
};

#endif
