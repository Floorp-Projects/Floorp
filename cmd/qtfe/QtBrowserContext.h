/* $Id: QtBrowserContext.h,v 1.2 1998/10/20 02:40:45 cls%seawood.org Exp $
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
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  Portions
 * created by Warwick Allison, Kalle Dalheimer, Eirik Eng, Matthias
 * Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav Tvete are
 * Copyright (C) 1998 Warwick Allison, Kalle Dalheimer, Eirik Eng,
 * Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  All Rights Reserved.
 */

#ifndef _QTBROWSERCONTEXT
#define _QTBROWSERCONTEXT

#include "QtContext.h"

#include <qprinter.h>

class QMainWindow;
class QMenuBar;
class QtBrowserScrollView;
class Toolbars;
class ContextMenu;
class QtFormElement;

class QtBrowserContext : public QtContext {
    /* A web browser window */
    Q_OBJECT
public:
    QtBrowserContext(MWContext*, Chrome *, QWidget* parent, const char* name=0, WFlags f=0,
		     int _scrolling_policy = LO_SCROLL_AUTO, int _bw = 0);

    virtual ~QtBrowserContext();

    virtual QWidget *contentWidget() const;
    virtual QWidget *topLevelWidget() const;

    virtual int documentWidth() const;
    virtual int documentHeight() const;
    virtual int scrollWidth() const;
    virtual int scrollHeight() const;
    virtual int visibleWidth() const;
    virtual int visibleHeight() const;

    void	setRefreshURLTimer(uint32 secs, char *url);
    void        killRefreshURLTimer();

    void	setBackgroundColor(uint8 red, uint8 green, uint8 blue);

    virtual bool securityDialog(int state, char *prefs_toggle);
    virtual void setDocTitle(char *title);
    virtual void setDocDimension(int iLocation, int32 iWidth, int32 iLength);
    virtual void setDocPosition(int iLocation, int32 x, int32 y);

    virtual int documentXOffset() const;
    virtual int documentYOffset() const;
    virtual void documentSetContentsPos( int x, int y );

    virtual void setLoadImagesButtonEnabled( bool );
    virtual void setStopButtonEnabled( bool );

    virtual int enableBackButton();
    virtual int enableForwardButton();
    virtual int disableBackButton();
    virtual int disableForwardButton();

    virtual void displayFormElement(int iLocation,
				       LO_FormElementStruct *form);
    virtual void getFormElementValue(LO_FormElementStruct *form,
						 bool delete_p, bool submit_p);

public slots:
    virtual void cmdOpenBrowser();
    virtual void cmdComposeMessage();
    virtual void cmdNewBlank();
    virtual void cmdNewTemplate();
    virtual void cmdNewWizard();
    virtual void cmdOpenPage();
    virtual void cmdSaveAs();
    virtual void cmdSaveFrameAs();
    virtual void cmdSendPage();
    virtual void cmdSendLink();
    virtual void cmdEditFrame();
    virtual void cmdEditPage();
    virtual void cmdUploadFile();
    virtual void cmdPrint();
    virtual void cmdClose();
    virtual void cmdUndo();
    virtual void cmdRedo();
    virtual void cmdCut();
    virtual void cmdCopy();
    virtual void cmdPaste();
    virtual void cmdSelectAll();
    virtual void cmdFindInObject();
    virtual void cmdFindAgain();
    virtual void cmdSearch();
    virtual void cmdSearchAddress();
    virtual void cmdEditPreferences();
    virtual void cmdToggleNavigationToolbar();
    virtual void cmdToggleLocationToolbar();
    virtual void cmdTogglePersonalToolbar();
    virtual void cmdIncreaseFont();
    virtual void cmdDecreaseFont();
    virtual void cmdReload();
    virtual void cmdSuperReload();
    virtual void cmdShowImages();
    virtual void cmdRefresh();
    virtual void cmdStopLoading();
    virtual void cmdViewPageSource();
    virtual void cmdViewPageInfo();
    virtual void cmdPageServices();
    virtual void cmdBack();
    virtual void cmdForward();
    virtual void cmdHome();
    virtual void cmdOpenNavCenter();
    virtual void cmdOpenOrBringUpBrowser();
    virtual void cmdOpenInbox();
    virtual void cmdOpenNewsgroups();
    virtual void cmdOpenEditor();
    virtual void cmdOpenConference();
    virtual void cmdCalendar();
    virtual void cmdHostOnDemand();
    virtual void cmdNetcaster();
    virtual void cmdToggleTaskbarShowing();
    virtual void cmdOpenFolders();
    virtual void cmdOpenAddressBook();
    virtual void cmdOpenHistory();
    virtual void cmdJavaConsole();
    virtual void cmdViewSecurity();
    virtual void cmdAboutNetscape();
    virtual void cmdAboutMozilla();
    virtual void cmdAboutQt();
    virtual void cmdGuide();

    virtual void cmdSecurityDialog1();
    virtual void cmdSecurityDialog2();
    virtual void cmdSecurityDialog3();
    virtual void cmdSecurityDialog4();
    virtual void cmdSecurityDialog5();
    virtual void cmdSecurityDialog6();
    virtual void cmdSecurityDialog7();

    virtual void setMessage( const char *message, int timeout = 3000 );
    virtual int getURL(URL_Struct *url);

    // these were fe_* functions
    void browserGetURL( const char* address );
    void editorEdit( Chrome*, const char* address );

    void scrollUp();
    void scrollDown();
    void pageUp();
    void pageDown();
    
protected:
    virtual QPainter* makePainter();

    virtual bool handleLayerEvent(CL_Layer *layer,
				  CL_Event *layer_event);
    bool initImageCallbacks();
    virtual void setupToolbarSignals();


private slots:
    void	refreshURLNow();
    void	viewMoved(int x, int y);
    void	contextMenuDied();

private:
    bool eventFilter( QObject *o, QEvent *e );

    // this method is not implemented in QtBrowserContext.cpp, but in
    // menus.cpp
    void populateMenuBar( QMenuBar* );

    // This method is not implemented in QtBrowserContext.cpp, but in
    // printing.cpp
    void print( URL_Struct* url, bool print_to_file, const char* filename );

    QMainWindow	*browserWidget;
    QtBrowserScrollView *scrollView;
    QTimer      *refreshURLTimer;
    QString	refreshURL;
    QPrinter printer;
    Toolbars * toolbars;

    QString currentMessage;
    QTimer * messageFlickerTimer;
    ContextMenu * contextMenu;

    // for forms
    void moveSubWidget( QWidget* w, int x, int y );
    void showSubWidget( QWidget* w, bool );
};


#endif
