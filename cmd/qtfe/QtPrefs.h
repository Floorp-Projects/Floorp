/* $Id: QtPrefs.h,v 1.1 1998/09/25 18:01:30 ramiro%netscape.com Exp $
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Paul Olav
 * Tvete.  Further developed by Warwick Allison, Kalle Dalheimer,
 * Eirik Eng, Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and
 * Paul Olav Tvete.  Copyright (C) 1998 Warwick Allison, Kalle
 * Dalheimer, Eirik Eng, Matthias Ettrich, Arnt Gulbrandsen, Haavard
 * Nord and Paul Olav Tvete.  All Rights Reserved.
 */

#ifndef _QTPREFS
#define _QTPREFS

#include <qdialog.h>
#include <qlistview.h>
#include <qlist.h>

class QWidgetStack;
class QtPrefPageItem;

class QLineEdit;
class QRadioButton;
class QButtonGroup;
class QCheckBox;

class QtContext;

class QtPreferences;

/*
From xfe/prefs.h; there must be a platform-independent
definition somewhere(???###)
*/
/* prefs version */

#define PREFS_CURRENT_VERSION "1.0"

/* browser startup page */
#define BROWSER_STARTUP_BLANK  0
#define BROWSER_STARTUP_HOME   1
#define BROWSER_STARTUP_LAST   2

/* mail server type */
#define MAIL_SERVER_POP3       0
#define MAIL_SERVER_IMAP       1
#define MAIL_SERVER_MOVEMAIL   2
#define MAIL_SERVER_INBOX      3

/* toolbar style */
#define BROWSER_TOOLBAR_ICONS_ONLY     0
#define BROWSER_TOOLBAR_TEXT_ONLY      1
#define BROWSER_TOOLBAR_ICONS_AND_TEXT 2

/* news keep method */
#define KEEP_ALL_NEWS          0
#define KEEP_NEWS_BY_AGE       1
#define KEEP_NEWS_BY_COUNT     2

/* offline startup mode */
#define OFFLINE_STARTUP_ONLINE      0
#define OFFLINE_STARTUP_OFFLINE     1
#define OFFLINE_STARTUP_ASKME       2

/* offline news download increments */
#define OFFLINE_NEWS_DL_ALL         0
#define OFFLINE_NEWS_DL_UNREAD_ONLY 1

/* offline news download increments */
#define OFFLINE_NEWS_DL_YESTERDAY    0
#define OFFLINE_NEWS_DL_1_WK_AGO     1
#define OFFLINE_NEWS_DL_2_WKS_AGO    2
#define OFFLINE_NEWS_DL_1_MONTH_AGO  3
#define OFFLINE_NEWS_DL_6_MONTHS_AGO 4
#define OFFLINE_NEWS_DL_1_YEAR_AGO   5

/* use document fonts */
#define DOC_FONTS_NEVER        0
#define DOC_FONTS_QUICK        1
#define DOC_FONTS_ALWAYS       2

/* help file sites */
#define HELPFILE_SITE_NETSCAPE  0
#define HELPFILE_SITE_INSTALLED 1
#define HELPFILE_SITE_CUSTOM    2

/* default link expiration for 'never expired' option */
#define LINK_NEVER_EXPIRE_DAYS  180

/* default mail html action */
#define HTML_ACTION_ASK        0
#define HTML_ACTION_TEXT       1
#define HTML_ACTION_HTML       2
#define HTML_ACTION_BOTH       3




class QtPrefItem : public QObject {
    Q_OBJECT
public:
    QtPrefItem( const char *k, QtPreferences *parent = 0 );
    virtual void save() = 0;
    virtual void read() = 0;
    const char *key() { return mykey; }
    bool isModified() const { return modified; }
protected slots:
    void setModified( bool b = TRUE ) { modified = b; }
private:
    QString mykey;
    bool modified;
};

class QtIntPrefItem : public QtPrefItem {
public:
    QtIntPrefItem( const char*, QLineEdit*, QtPreferences *parent = 0 );
    //setSpinBox
    int value() const;
    void setValue( int );
    void save();
    void read();
private:
    QLineEdit *lined;
};



class QtCharPrefItem : public QtPrefItem {
public:
    QtCharPrefItem( const char*, QLineEdit*, QtPreferences *parent = 0 );
    //QSpinBox
    const char *value() const;
    void setValue( const char* );
    void save();
    void read();
private:
    QLineEdit *lined;
};


class QtBoolPrefItem : public QtPrefItem {
public:
    QtBoolPrefItem( const char*, QCheckBox*, QtPreferences *parent = 0 );
    bool value() const;
    void setValue( bool );
    void save();
    void read();
private:
    QCheckBox *chkbx;
};

class QtEnumPrefItem : public QtPrefItem {
    Q_OBJECT
public:
    QtEnumPrefItem( const char*, QtPreferences *parent = 0 );
    void addRadioButton( QRadioButton*, int val );
    int value() const;
    void setValue( int );
    void save();
    void read();
private slots:
    void setCurrent( int );
private:
    QButtonGroup *btngrp;
    int currentVal;
};


class QtPreferences : public QDialog {
    Q_OBJECT
public:
    QtPreferences(QWidget* parent=0, const char* name=0, int f=0);

    void add( QtPrefItem* );
    void readPrefs();
public slots:
    void apply();
    void cancel();
private slots:
    void manualProxyPage();
private:
    void add( QtPrefPageItem*, QWidget*);
    QWidgetStack* categories;
    QList<QtPrefItem> *prefs;
};

class QtPrefPageItem : public QObject, public QListViewItem {
    Q_OBJECT
public:
    QtPrefPageItem( QListView* view, const char* label );
    QtPrefPageItem( QListViewItem* group, const char* label );
    int id() const { return i; }

signals:
    void activated(int id);

protected:
    void activate();

private:
    int i;
    static int next_id;
};

class QtPrefs {
public:
    static void editPreferences( QtContext * );
};

#endif
