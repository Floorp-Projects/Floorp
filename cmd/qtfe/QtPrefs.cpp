/* $Id: QtPrefs.cpp,v 1.1 1998/09/25 18:01:29 ramiro%netscape.com Exp $
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

#include "QtPrefs.h"
#include "QtContext.h"


#include <qpainter.h>
#include <qapp.h>
#include <qlabel.h>
#include <qchkbox.h>
#include <qradiobt.h>
#include <qlined.h>
#include <qlistview.h>
#include <qlistbox.h>
#include <qcombo.h>
#include <qwidgetstack.h>
#include <qtvbox.h>
#include <qthbox.h>
#include <qtgrid.h>
#include <qpushbt.h>
#include <qtlabelled.h>
#include <qtbuttonrow.h>
#include <qlayout.h>
#include <qbttngrp.h>
#include <qvalidator.h>

#include "prefapi.h"


/*
  The QtPrefItem classes connect Qt widgets to Mozilla preferences
  (PREF_Get*Pref() and friends, defined in prefapi.h).

  Create your widget, feed it into the appropriate QtPrefItem constructor,
  and the rest should happen automatically

  Note that QtEnumPrefItem needs several radio buttons. They are
  not given in the constructor, but specified with addRadioButton().
 */



QtPrefItem::QtPrefItem( const char* k, QtPreferences *parent )
    :QObject( parent ), mykey( k )
{
    parent->add(this);
}


QtIntPrefItem::QtIntPrefItem( const char*k, QLineEdit *l, QtPreferences *p )
    :QtPrefItem( k, p ), lined(l)
{
    if ( !l ) {
	warning( "QtIntPrefItem without lined" );
	return;
    }
    QIntValidator *v = new QIntValidator( l );
    l->setValidator( v );
    read();
}
    //setSpinBox
int QtIntPrefItem::value() const
{
    return lined ? QString(lined->text()).toInt() : 0;
}


void QtIntPrefItem::setValue( int val )
{
    QString s;
    s.setNum( val );
    lined->setText( s );
}


void QtIntPrefItem::save()
{
    int status;
    status = PREF_SetIntPref( key(), value() );
}

void QtIntPrefItem::read()
{
    int32 newVal = 0;
    int status;
    status = PREF_GetIntPref( key(), &newVal );
    setValue( (int)newVal );
}



QtBoolPrefItem::QtBoolPrefItem( const char*k, QCheckBox *c, QtPreferences *p )
    :QtPrefItem( k, p ), chkbx(c)
{
    read();
}
    //setSpinBox
bool QtBoolPrefItem::value() const
{
    return chkbx ? chkbx->isChecked() : FALSE;
}

void QtBoolPrefItem::setValue( bool b )
{
    chkbx->setChecked( b );
}

void QtBoolPrefItem::save()
{
    int status;
    status = PREF_SetBoolPref( key(), value() );
}

void QtBoolPrefItem::read()
{
    XP_Bool newVal = FALSE;
    int status;
    status = PREF_GetBoolPref( key(), &newVal );
    setValue( newVal );
}



QtCharPrefItem::QtCharPrefItem( const char*k, QLineEdit *l, QtPreferences *p )
    :QtPrefItem( k, p ), lined(l)
{
    read();
}
    //setSpinBox
const char *QtCharPrefItem::value() const
{
    return lined ? lined->text() : 0;
}

void QtCharPrefItem::save()
{
    int status;
    status = PREF_SetCharPref( key(), value() );
}

void QtCharPrefItem::read()
{
    int len = 333; //#########
    char buf[333]; //######
    buf[0]=0;
    int status;
    status = PREF_GetCharPref( key(), buf, &len );
    setValue( buf );
}

void QtCharPrefItem::setValue( const char *txt )
{
    lined->setText( txt );
}



QtEnumPrefItem::QtEnumPrefItem( const char*k, QtPreferences *p )
    :QtPrefItem( k, p ), currentVal(0)
{
    btngrp = new QButtonGroup;
    btngrp->setExclusive( TRUE );
    connect( btngrp, SIGNAL(clicked(int)),
	     this, SLOT(setCurrent(int)) );
    read();
}

void QtEnumPrefItem::addRadioButton( QRadioButton *btn, int val )
{
    btngrp->insert( btn, val );
    btngrp->setButton( currentVal ); // the read() may already have happened
}

int QtEnumPrefItem::value() const
{
    return currentVal;
}

void QtEnumPrefItem::setValue( int val )
{
    currentVal = val;
    btngrp->setButton( val );
}

void QtEnumPrefItem::setCurrent( int val )
{
    currentVal = val;
}


void QtEnumPrefItem::save()
{
    int status;
    status = PREF_SetIntPref( key(), value() );
}
void QtEnumPrefItem::read()
{
    int32 newVal = 0;
    int status;
    status = PREF_GetIntPref( key(), &newVal );
    setValue( (int)newVal );
}



class StrongHeading : public QtHBox {
public:
    StrongHeading(const char* label, const char* desc, QWidget* parent=0, const char* name=0) :
	QtHBox(parent, name)
    {
	QLabel* l = new QLabel(label, this);
	QLabel* d = new QLabel(desc, this);
	QPalette p = palette();
	QColorGroup n = palette().normal();
	QColorGroup g(n.background(), n.foreground(), n.light(), n.dark(),
		      n.mid(), n.background(), n.base());
	p.setNormal( g );
	setPalette(p);
	l->setPalette(p);
	d->setPalette(p);
	l->setMargin(3);
	d->setMargin(2);

	QFont bold = *QApplication::font();
	bold.setBold(TRUE);
	bold.setPointSize(bold.pointSize()+2);
	l->setFont( bold );

	l->setFixedSize(l->sizeHint());
    }
};

inline static QWidget *fix( QWidget *w )
{
    w->setFixedSize( w->sizeHint() );
    return w;
}

static QWidget* advanced( QtPreferences *prf )
{
    QtVBox *vbox = new QtVBox;
    QtVBox *page = vbox;
    new StrongHeading( "Advanced",
		       "Change preferences that affect the entire product",
		       vbox );

    QtVBox *box;

    QtLabelled *frame = new QtLabelled( vbox );
    box = new QtVBox( frame );
    QCheckBox *cb;
    cb = new QCheckBox( "&Automatically load images and other data types\n"
			"(Otherwise, click the Images"
			" button to load when needed)", box );
    new QtBoolPrefItem( "general.always_load_images", cb, prf );
#ifdef JAVA
    cb = new QCheckBox( "Enable &Java", box );
#endif
    cb = new QCheckBox( "Enable Ja&vaScript", box );
    new QtBoolPrefItem( "javascript.enabled", cb, prf);
    cb = new QCheckBox( "Enable &Style Sheets", box );
    new QtBoolPrefItem( "browser.enable_style_sheets", cb, prf);
#if 0
    cb = new QCheckBox( "Enable Auto&Install", box );
    cb = new QCheckBox( "Send email address as anonymous &FTP password", box );
#endif

    box = new QtVBox( new QtLabelled( "Cookies", vbox ) );
    QtEnumPrefItem *cookie
	= new QtEnumPrefItem( "network.cookie.cookieBehavior", prf );
    cookie->addRadioButton( new QRadioButton( "Accept all &cookies", box ),
			    NET_Accept );
    cookie->addRadioButton( new QRadioButton(
	   "&Only accept cookies originating from the same server as\n"
	   "the page being viewed", box ), NET_DontAcceptForeign );
    cookie->addRadioButton( new QRadioButton(
		    "&Do not accept or send cookies", box ),
			    NET_DontUse);
    new QtBoolPrefItem( "network.cookie.warnAboutCookies",
		  new QCheckBox( "&Warn me before accepting a cookie", box ),
			prf );
    page->addStretch();
    return vbox;
}
/*
class DummyCategory : public QLabel {
public:
    DummyCategory( const char * s = 0 ) :
	QLabel(s?s:"Dummy")
    {
	setMinimumSize(100,100);
    }
};
*/


static QWidget *fontPage( QtPreferences *prf )
{
    QtVBox *page = new QtVBox;

    new StrongHeading( "Fonts", "Change the fonts in your display" , page );
    QtLabelled *frame = new QtLabelled( "Fonts and Encodings", page );
    QtVBox *box;
#if 0
    box = new QtVBox( frame );
    QtHBox *hbox = new QtHBox( box );
    new QLabel( "For the Encoding", hbox );
    QComboBox *combo = new QComboBox( FALSE, hbox );
    combo->insertItem( "Western (iso-8859-1)" );

    hbox = new QtHBox( box );
    new QLabel( "Variable Width Font", hbox );
    combo = new QComboBox( FALSE, hbox );
    combo->insertItem( "Times (Adobe)" );
    new QLabel( "Size:", hbox );
    combo = new QComboBox( FALSE, hbox );
    combo->insertItem( "12.0" );


    hbox = new QtHBox( box );
    new QLabel( "Fixed Width Font", hbox );
    combo = new QComboBox( FALSE, hbox );
    combo->insertItem( "Courier (Adobe)" );
    new QLabel( "Size:", hbox );
    combo = new QComboBox( FALSE, hbox );
    combo->insertItem( "10.0" );
#else
    new QLabel( "Sorry, not implemented", frame );
#endif

    frame = new QtLabelled( page );
    box = new QtVBox( frame );
    new QLabel( "Sometimes a document will provide its own fonts." );

    QtEnumPrefItem *docfonts
	= new QtEnumPrefItem( "browser.use_document_fonts", prf );
    docfonts->addRadioButton( new QRadioButton( "Use my default fonts, overriding document-specified fonts", box ), DOC_FONTS_NEVER );
    docfonts->addRadioButton( new QRadioButton( "Use document-specified fonts, but disable Dynamic Fonts", box ), DOC_FONTS_QUICK );
    docfonts->addRadioButton( new QRadioButton( "Use document-specified fonts, including Dynamic Fonts", box ), DOC_FONTS_ALWAYS );

    page->addStretch();
    return page;
}


static QWidget *colorPage( QtPreferences *prf )
{
    QtVBox *page = new QtVBox;

     new StrongHeading( "Colors", "Change the colors in your display" , page );

     QtLabelled *frame;
     QtVBox *box;
#if 0
    QtHBox *topbox = new QtHBox( page );

    frame = new QtLabelled( "Colors", topbox );
    QtGrid *grid = new QtGrid( 3, frame );
    new QLabel( "Text:", grid );
    new QPushButton( "dummy", grid );
    new QWidget(grid); //### grid->skip();
    new QLabel( "Background:", grid );
    new QPushButton( "dummy", grid );
    new QWidget(grid); //### grid->skip();
    new QWidget(grid); //### grid->skip();
    new QWidget(grid); //### grid->skip();
    new QPushButton( "Use Default", grid );

    frame = new QtLabelled( "Links", topbox );
    box = new QtVBox( frame );
    grid = new QtGrid( 2, box );
    new QLabel( "Unvisited Links:", grid );
    new QPushButton( "dummy", grid );
    new QLabel( "Visited Links:", grid );
    new QPushButton( "dummy", grid );
    new QCheckBox( "Underline links", box );

#else
    new QLabel( "Sorry, not implemented", page );
#endif

    frame = new QtLabelled( page );
    box = new QtVBox( frame );

    new QLabel( "Sometimes a document will provide its own colors and background", box );
    new QtBoolPrefItem( "browser.use_document_colors",
	     new QCheckBox( "Always use my colors, overriding document", box ),
	     prf);

    page->addStretch();
    return page;
}

static QWidget *appsPage( QtPreferences *prf )
{
    QtVBox *page = new QtVBox;

    new StrongHeading( "Applications",
		       "Specify helper applications for different file types",
		       page );
#if 0
    QtLabelled *frame = new QtLabelled( page );
    QtVBox *box = new QtVBox( frame );
    QListView *lv = new QListView( box );
    lv->addColumn( "Description", 250 );
    lv->addColumn( "Handled By", 200 );
    new QListViewItem( lv, "GIF Image", "Netscape" );
    new QListViewItem( lv, "Hypertext Markup Language", "Netscape" );
    new QListViewItem( lv, "Plain text", "Netscape" );
    new QListViewItem( lv, "Perl Program", "Unknown:Prompt User" );
    new QListViewItem( lv, "Lkjsfdlkdsjgf", "Unknown:Prompt User" );
    new QListViewItem( lv, "Asdfafsafs", "Unknown:Prompt User" );
    new QListViewItem( lv, "Xzcmnv", "Unknown:Prompt User" );
    new QListViewItem( lv, "Aqwreqrpoierw", "Unknown:Prompt User" );
    new QListViewItem( lv, "mnzxcewewq", "Unknown:Prompt User" );
    new QListViewItem( lv, "Oiuycxsxc Xocuy", "Unknown:Prompt User" );
    new QListViewItem( lv, "JPG Image", "Netscape" );
    new QListViewItem( lv, "XBM Image", "Netscape" );

    QtButtonRow *br = new QtButtonRow( box );
    new QPushButton( "New...", br );
    new QPushButton( "Edit...", br );
    new QPushButton( "Delete", br );

    QtHBox *hbox = new QtHBox( box );
    new QLabel( "Download files to:", hbox );
    new QLineEdit( hbox );
    new QPushButton( "Choose...", hbox );
#else
    new QLabel( "Sorry, not implemented", page );
#endif

    page->addStretch();
    return page;
}


static QWidget *appearPage( QtPreferences *prf )
{
    QtVBox *page = new QtVBox;

    new StrongHeading( "Appearance",
		       "Change the appearance of the display" , page );
    QtLabelled *frame;
    QtVBox *box;
#if 0	//### since only navigator is currently implemented
    frame = new QtLabelled( "On startup, launch", page );
    box = new QtVBox( frame );
    new QCheckBox( "&Navigator", box );
    new QCheckBox( "&Messenger Mailbox", box );
    new QCheckBox( "Collabra &Discussions", box );
    new QCheckBox( "Page &Composer", box );
    new QCheckBox( "N&etcaster", box );
#endif
    frame = new QtLabelled( "Show Toolbar As", page );
    box = new QtVBox( frame );
    QtEnumPrefItem *toolb
	= new QtEnumPrefItem( "browser.chrome.toolbar_style", prf );
    toolb->addRadioButton( new QRadioButton( "&Pictures and Text", box ),
			   BROWSER_TOOLBAR_ICONS_AND_TEXT );
    toolb->addRadioButton( new QRadioButton( "Pictures &Only", box ),
			   BROWSER_TOOLBAR_ICONS_ONLY );
    toolb->addRadioButton( new QRadioButton( "&Text Only", box ),
			   BROWSER_TOOLBAR_TEXT_ONLY );
    page->addStretch();
    return page;
}

static QWidget *navigatorPage( QtPreferences *prf )
{
    QtVBox *page = new QtVBox;

    new StrongHeading( "Navigator",   "Specify the home page location",
			    page );
    QtLabelled *frame = new QtLabelled( "Browser starts with", page );
    QtVBox *box = new QtVBox( frame );
    QtEnumPrefItem *startup
	= new QtEnumPrefItem( "browser.startup.page", prf );
    startup->addRadioButton( new QRadioButton( "Blank page", box ),
			     BROWSER_STARTUP_BLANK );
    startup->addRadioButton( new QRadioButton( "Home page", box ),
			     BROWSER_STARTUP_HOME );
    startup->addRadioButton( new QRadioButton( "Last page visited", box ),
			     BROWSER_STARTUP_LAST );

    frame = new QtLabelled( "Home Page", page );
    box = new QtVBox( frame );
    new QLabel( "Clicking the Home button will take you to this page",
		box );
    QtHBox *hbox = new QtHBox( box );
    new QLabel( "Location:", hbox );
    QLineEdit *home = new QLineEdit( hbox );
    new QtCharPrefItem( "browser.startup.homepage", home, prf );
    hbox = new QtHBox( box );
    new QPushButton( "Use Current Page" );
    new QPushButton ( "Choose" );

    frame = new QtLabelled( "History", page );
    hbox = new QtHBox( frame );
    new QLabel( "History expires after", hbox );
    new QtIntPrefItem( "browser.link_expiration", new QLineEdit( hbox ), prf );
    new QLabel( "days", hbox );
    new QPushButton( "Clear History", hbox );

    page->addStretch();
    return page;
}


/*
pref("browser.cache.disk_cache_size",       7680);
pref("browser.cache.memory_cache_size",     1024);
pref("browser.cache.disk_cache_ssl",        false);
pref("browser.cache.check_doc_frequency",   0);
localDefPref("browser.cache.directory",         "");
localDefPref("browser.cache.wfe.directory", null);
*/

static QWidget *cachePage( QtPreferences *prf )
{
    QtVBox *page = new QtVBox;

    new StrongHeading( "Cache",   "Designate the size of the cache",
			    page );
    QtLabelled *frame = new QtLabelled( 0, page );
    QtVBox *box = new QtVBox( frame );

    new QLabel( "The cache is used to keep local copies of frequently accessed docu-\n\
ments and thus reduce time connected to the network. The Reload\n\
button will always compare the cache document to the network\n\
document and show the most recent one. To load pages and images\n\
from the network instead of the cache, press the Shift key and click\n\
the reload button.", box );

    QtHBox *hbox = new QtHBox( box );
    new QLabel( "Disk cache:", hbox );
    new QtIntPrefItem( "browser.cache.disk_cache_size",
		       new QLineEdit( hbox ), prf );
    new QPushButton( "Clear Disk Cache", hbox );

    hbox = new QtHBox( box );
    new QLabel( "Memory cache:", hbox );
    new QtIntPrefItem( "browser.cache.memory_cache_size",
		       new QLineEdit( hbox ), prf );
    new QPushButton( "Clear Memory Cache", hbox );

    hbox = new QtHBox( box );
    new QLabel( "Cache Folder:", hbox );
    new QtCharPrefItem( "browser.cache.directory",
		       new QLineEdit( hbox ), prf );
    new QPushButton( "Choose...", hbox );

    new QLabel( "Document in cache is compared to document on network", box );
    //    QtVBox *box = new QtVBox( frame );

    QtEnumPrefItem *freq
	= new QtEnumPrefItem( "browser.cache.check_doc_frequency", prf );
    //#### These numbers are not defined in prefs.h
    freq->addRadioButton( new QRadioButton( "Every time", box ), 1 );
    freq->addRadioButton( new QRadioButton( "Once per session", box ), 0 );
    freq->addRadioButton( new QRadioButton( "Never", box ), 2 );

    page->addStretch();
    return page;
}



class QtLayDlg : public QDialog
{
public:
    QtLayDlg( QWidget *parent = 0, const char *name = 0 );
    QtVBox *mainBox() { return vbox; }
private:
    QtVBox *vbox;
};


QtLayDlg::QtLayDlg( QWidget *parent, const char *name )
    :QDialog( parent, name, TRUE )
{
    QVBoxLayout *gm = new QVBoxLayout( this, 5 );
    vbox = new QtVBox( this );
    gm->addWidget( vbox );

    QHBoxLayout *btn = new QHBoxLayout;
    gm->addLayout( btn );
    QPushButton *ok = new QPushButton( "OK", this );
    ok->setDefault(TRUE);
    btn->addWidget( ok );

    btn->addStretch();

    QPushButton *cancel = new QPushButton( "Cancel", this );
    cancel->setAutoDefault(TRUE);
    btn->addWidget( cancel );

    QSize s = ok->sizeHint();
    s = s.expandedTo( cancel->sizeHint() );
    ok->setFixedSize( s );
    cancel->setFixedSize( s );
    //###use QtButtonRow when it supports addStretch

    connect( ok, SIGNAL(clicked()), SLOT(accept()) );
    connect( cancel, SIGNAL(clicked()), SLOT(reject()) );
}


static QtLayDlg *manProxyDlg( QtPreferences *prf )
{

    QtLayDlg *dlg = new QtLayDlg( prf );

    QtVBox *page = dlg->mainBox();

    new QLabel(
"You may configure a proxy and port number for each of the internet\n"
"protocols that Mozilla supports.", page );

    QtGrid *grid = new QtGrid( 4, QtGrid::Horizontal, page );

    new QLabel( "FTP Proxy", grid );
    new QtCharPrefItem( "network.proxy.ftp", new QLineEdit( grid ), prf );
    new QLabel( "Port:", grid );
    new QtIntPrefItem( "network.proxy.ftp_port", new QLineEdit( grid ), prf );

    new QLabel( "Gopher Proxy", grid );
    new QtCharPrefItem( "network.proxy.gopher", new QLineEdit( grid ), prf );
    new QLabel( "Port:", grid );
    new QtIntPrefItem( "network.proxy.gopher_port", new QLineEdit( grid ), prf );

    new QLabel( "HTTP Proxy", grid );
    new QtCharPrefItem( "network.proxy.http", new QLineEdit( grid ), prf );
    new QLabel( "Port:", grid );
    new QtIntPrefItem( "network.proxy.http_port", new QLineEdit( grid ), prf );

    new QLabel( "Security Proxy", grid );
    new QtCharPrefItem( "network.proxy.ssl", new QLineEdit( grid ), prf );
    new QLabel( "Port:", grid );
    new QtIntPrefItem( "network.proxy.ssl_port", new QLineEdit( grid ), prf );

    new QLabel( "WAIS Proxy", grid );
    new QtCharPrefItem( "network.proxy.wais", new QLineEdit( grid ), prf );
    new QLabel( "Port:", grid );
    new QtIntPrefItem( "network.proxy.wais_port", new QLineEdit( grid ), prf );


/*
   94:localDefPref("browser.socksfile_location",      "");
  166:pref("network.hosts.socks_conf",            "");
  */

    new QLabel(
"You may provide a list of domains that Mozilla should access directly,\n"
"rather than via the proxy:", page );

    grid = new QtGrid( 4, QtGrid::Horizontal, page );

    new QLabel( "No Proxy for:", grid );
    new QtCharPrefItem( "network.proxy.no_proxies_on", new QLineEdit( grid ), prf );
    new QWidget; //#skip
    new QWidget; //#skip

    new QLabel( "SOCKS Host:", grid );
    new QtCharPrefItem( "network.hosts.socks_server", new QLineEdit( grid ), prf );
    new QLabel( "Port:", grid );
    new QtIntPrefItem( "network.hosts.socks_serverport", new QLineEdit( grid ), prf );

#if 0
    QtButtonRow *bottom = new QtButtonRow( page );
    new QPushButton( "OK", bottom );
    //###############    bottom->addStretch();
    new QPushButton( "Cancel", bottom );
#endif

    //    page->pack();
    //    dlg->resize(0,0);
    dlg->setCaption( "QtMozilla: View Manual Proxy Configuration" );

    dlg->hide();

    return dlg;
}



static QWidget *proxyPage( QtPreferences *prf )
{
    QtVBox *page = new QtVBox;

    new StrongHeading( "Proxies",   "Configure proxies to access the Internet",
			    page );
    QtLabelled *frame = new QtLabelled( 0, page );
    QtVBox *box = new QtVBox( frame );

    new QLabel( "A network proxy is used to provide additional security between your\n\
computer and the Internet (usually along with a firewall) and/or to\n\
increase performance between networks by reducing redundant traffic\n\
via caching. Your system administrator can provide you with proper\n\
proxy settings.", box );


    QtEnumPrefItem *prox = new QtEnumPrefItem( "network.proxy.type", prf );
    QRadioButton *rb;
    rb = new QRadioButton( "Direct connection to the internet", box );
    prox->addRadioButton( rb, PROXY_STYLE_NONE );

    QtHBox *hbox = new QtHBox( box );
    rb = new QRadioButton( "Manual proxy configuration", hbox );
    prox->addRadioButton( rb,PROXY_STYLE_MANUAL );
    QPushButton * manBut = new QPushButton( "View...", hbox );
    QObject::connect( manBut, SIGNAL(clicked()),
		      prf, SLOT(manualProxyPage()) );
    QObject::connect( rb, SIGNAL(toggled(bool)),
		      manBut, SLOT(setEnabled(bool)) );
    manBut->setEnabled( rb->isOn() );

    rb = new QRadioButton( "Automatic proxy configuration", box );
    prox->addRadioButton( rb, PROXY_STYLE_AUTOMATIC );

    hbox = new QtHBox( box );
    new QLabel( "Configuration location (URL)", hbox );
    new QtCharPrefItem( "network.proxy.autoconfig_url", //###???
			new QLineEdit( hbox ), prf );
    new QPushButton( "&Reload", box );

    page->addStretch();
    return page;
}



static QWidget *identityPage( QtPreferences *prf )
{
    QtVBox *page = new QtVBox;

    new StrongHeading( "Identity",
		       "Set your name, email address, and signature file",
		       page );
    QtLabelled *frame = new QtLabelled( 0, page );
    QtVBox *box = new QtVBox( frame );

    new QLabel( "The information below is needed before you can send mail. If you do\n\
not know the information requested, please contact your system\n\
administrator or Internet Service Provider.", box );

    new QLabel( "Your name:", box );
    new QtCharPrefItem( "mail.identity.username", new QLineEdit(box), prf );

    new QLabel( "Email Address:", box );
    new QtCharPrefItem( "mail.identity.useremail", new QLineEdit(box), prf );

    new QLabel( "Organization:", box );
    new QtCharPrefItem( "mail.identity.organization",
			new QLineEdit(box), prf );

    page->addStretch();
    return page;
}

static QWidget *languagePage( QtPreferences *prf )
{
    QtVBox *page = new QtVBox;

    new StrongHeading( "Languages",
		       "View web pages in different languages",
		       page );
#if 0
    QtLabelled *frame = new QtLabelled( 0, page );
    QtVBox *box = new QtVBox( frame );
    new QLabel( "Choose in order of preference the language(s) in which you prefer to\n\
view web pages. Web pages are sometimes available in serveral\n\
languages. Navigator presents the pages in the available language\n\
you most prefer.", box );
#else
    new QLabel( "Sorry, not implemented", page );
#endif
    page->addStretch();

    return page;
}


QtPreferences::QtPreferences(QWidget* parent, const char* name, int f) :
    QDialog(parent, name, TRUE, f)
{
    prefs = new QList<QtPrefItem>;

    QGridLayout* grid = new QGridLayout(this,1,1,5);
    QtVBox*       vbox = new QtVBox(this);
    grid->addWidget(vbox,0,0);
    grid->activate();

    QtHBox*        hbox = new QtHBox(vbox);
    QListView*    selector = new QListView(hbox);
    categories = new QWidgetStack(hbox);
    QtButtonRow*   buttons = new QtButtonRow(vbox);
    QPushButton*  ok = new QPushButton("OK",buttons);
    QPushButton*  cancel = new QPushButton("Cancel",buttons);

    ok->setDefault(TRUE);
    connect(ok, SIGNAL(clicked()), this, SLOT(accept()));
    connect(ok, SIGNAL(clicked()), this, SLOT(apply()));
    connect(cancel, SIGNAL(clicked()), this, SLOT(reject()));
    connect(cancel, SIGNAL(clicked()), this, SLOT(cancel()));

    QFontMetrics fm=fontMetrics();
    int w = fm.width("New Page Colors ")+selector->treeStepSize()*2;
    selector->addColumn( "Category", w );
    selector->setMaximumWidth( w );
#if 0     //workaround for list view behaviour
    QListViewItem *selRoot = new QListViewItem( selector );
    selRoot->setOpen( TRUE );
#else
    selector->setRootIsDecorated( TRUE );
    QListView *selRoot = selector;
#endif
    selector->setFocusPolicy( QWidget::StrongFocus );
    QtPrefPageItem *group;

    add(group = new QtPrefPageItem(selRoot, "Appearance"),
	appearPage( this ) );
    add(new QtPrefPageItem(group, "Font"), fontPage( this ) );
    add(new QtPrefPageItem(group, "Colors"), colorPage( this ) );
    add(group = new QtPrefPageItem(selRoot, "Navigator"),
	navigatorPage( this ) );
    add(new QtPrefPageItem(group, "Languages"), languagePage( this ) );
    add(new QtPrefPageItem(group, "Applications"), appsPage( this ) );
    add(new QtPrefPageItem(selRoot, "Identity"), identityPage( this ) );

    add(group = new QtPrefPageItem(selRoot, "Advanced"), advanced( this ));
    add(new QtPrefPageItem(group, "Cache"), cachePage( this ) );
    add(new QtPrefPageItem(group, "Proxies"), proxyPage( this ) );


//    add(new QtPrefPageItem(group, "Disk Space"), new DummyCategory);
//     add(group = new QtPrefPageItem(selRoot, "Mail & Groups"), new DummyCategory);
//     add(new QtPrefPageItem(group, "Messages"), new DummyCategory);
//     add(new QtPrefPageItem(group, "Mail Server"), new DummyCategory);
//     add(new QtPrefPageItem(group, "Groups Server"), new DummyCategory);
//     add(new QtPrefPageItem(group, "Directory"), new DummyCategory);
//     add(group = new QtPrefPageItem(selRoot, "Composer"), new DummyCategory);
//     add(new QtPrefPageItem(group, "New Page Colors"), new DummyCategory);
//     add(new QtPrefPageItem(group, "Publish"), new DummyCategory);

    setCaption("QtMozilla: Preferences");
}


/*!
  Starts the manual proxy configuration dialog
*/

void QtPreferences::manualProxyPage()
{
    static QDialog *dlg = 0;
    if ( !dlg )
	dlg = manProxyDlg( this );
    dlg->show();
}


void QtPreferences::add( QtPrefPageItem* item, QWidget* stack_item)
{
    categories->addWidget(stack_item, item->id());
    connect( item, SIGNAL(activated(int)), categories, SLOT(raiseWidget(int)) );
}


/*!
  Adds a new preferences item to the list of those who has to be
  saved when the OK button is pressed
*/

void QtPreferences::add( QtPrefItem *pr )
{
    prefs->append( pr );
}

void QtPreferences::apply()
{
    QListIterator<QtPrefItem> it( *prefs );
    QtPrefItem *pr;
    while ( (pr = it.current() ) ) {
	++it;
	pr->save();
    }
}

void QtPreferences::cancel()
{
    //readPrefs(); //### is this the right behaviour?
    //we need to reset the stuff so it is correct the next time the
    //dialog pops up, but that should be done just before show()

}



void QtPreferences::readPrefs()
{
    QListIterator<QtPrefItem> it( *prefs );
    QtPrefItem *pr;
    while ( (pr = it.current() ) ) {
	++it;
	pr->read();
    }
}

QtPrefPageItem::QtPrefPageItem( QListView* view, const char* label ) :
    QListViewItem( view, label, 0 ),
    i(next_id++)
{
    setExpandable(TRUE);
}

QtPrefPageItem::QtPrefPageItem( QListViewItem* group, const char* label ) :
    QListViewItem( group, label, 0 ),
    i(next_id++)
{
    setExpandable(FALSE);
}

void QtPrefPageItem::activate()
{
    emit activated(i);
}

int QtPrefPageItem::next_id = 0;


/*!
Edits prefs
*/

void QtPrefs::editPreferences( QtContext *cntxt )
{
    static QtPreferences *prf = 0;
    if ( !prf )
	prf = new QtPreferences( cntxt->topLevelWidget() );
    prf->show();
    prf->raise();
}
