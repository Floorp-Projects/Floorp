/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* */
/*
   PrefsDialog.h -- class definitions for FE preferences dialog windows
   Created: Linda Wei <lwei@netscape.com>, 17-Sep-96.
 */

// class XFE_PrefsDialog 
//       . class definition for the preferences dialog.
//       . containing instances of XFE_PrefsPage
// 
// class XFE_PrefsPage
//       . abstract base class of all preferences pages
// 


#ifndef _xfe_prefsdialog_h
#define _xfe_prefsdialog_h

#include "rosetta.h"
#include "outline.h"
#include "Dialog.h"
#include "xeditor.h"
#include "Outliner.h"
#include "Outlinable.h"
#include "PrefsData.h"
/* #ifdef MOZ_LDAP */ /* (rb) This file is required  */
#include "dirprefs.h"
/* #endif */
#ifdef MOZ_MAIL_NEWS
#include "PrefsDialogMServer.h"
#include "PrefsMailFolderDlg.h"
#endif

#define PREFS_SET_GLOBALPREF_TEXT(SLOT1,SLOT2) \
{ \
    char *s = 0; \
	if (fe_globalPrefs.SLOT1) XP_FREE(fe_globalPrefs.SLOT1); \
	s = fe_GetTextField(fep->SLOT2); \
	fe_globalPrefs.SLOT1 = s ? XP_STRDUP(s) : XP_STRDUP(""); \
	if (s) XtFree(s); \
}

#define PREFS_SET_GLOBALPREF_BOOLEAN(SLOT1,SLOT2) \
{ \
	Boolean    b; \
    XtVaGetValues(fep->SLOT1, XmNset, &b, 0); \
    fe_globalPrefs.SLOT2 = b; \
}

#define PREFS_CHECK_FILE(W,DESC,MSG,N) \
{ \
    char *text = 0;	\
    char *orig_text = 0;	\
    orig_text = fe_GetTextField (W); \
    text = fe_StringTrim (orig_text); \
    if (!text || !*text || stat (text, &st)) { \
        PR_snprintf (MSG, 200, \
					 XP_GetString(XFE_FILE_DOES_NOT_EXIST),	\
					 DESC, (text ? text : ""));	\
        MSG += strlen (MSG); \
	} \
	if (orig_text) XtFree(orig_text); \
}

#define PREFS_CHECK_DIR(W,DESC,MSG,N) \
{ \
    char *text = 0;	\
    char *orig_text = 0;	\
    orig_text = fe_GetTextField (W); \
    text = fe_StringTrim (orig_text); \
    if (!text || !*text) { \
		PR_snprintf (MSG, 200, \
					 XP_GetString(XFE_EMPTY_DIR), \
					 DESC);	\
        MSG += strlen (MSG); \
	} \
    else if (stat (text, &st) || (!S_ISDIR(st.st_mode))) { \
        PR_snprintf (MSG, 200, \
					 XP_GetString(XFE_DIR_DOES_NOT_EXIST),	\
					 DESC, text);	\
        MSG += strlen (MSG); \
	} \
	if (orig_text) XtFree(orig_text); \
}


#define PREFS_CHECK_HOST_1(TEXT,DESC,MSG,N)	\
{ \
    int d; char c; \
    PRHostEnt hpbuf; \
    char dbbuf[PR_NETDB_BUF_SIZE]; \
    if (TEXT && 4 == sscanf (TEXT, " %d.%d.%d.%d %c", \
							 &d, &d, &d, &d, &c)) \
        /* IP addresses are ok */ ;	 \
    else if (!TEXT || !*TEXT ||	\
			 (PR_GetHostByName(TEXT, dbbuf, sizeof(dbbuf), &hpbuf)) == PR_FAILURE) { \
		PR_snprintf (MSG, 200,\
					 XP_GetString(XFE_HOST_IS_UNKNOWN), \
					 DESC, (TEXT ? TEXT : "")); \
		MSG += strlen (MSG); \
	} \
}


#define PREFS_CHECK_HOST(W,DESC,MSG,N) \
{ \
    char *text = 0; \
    char *orig_text = 0;	\
    orig_text = fe_GetTextField (W); \
    text = fe_StringTrim (orig_text); \
    PREFS_CHECK_HOST_1 (text, DESC, MSG, N); \
	if (orig_text) XtFree(orig_text); \
}

#define PREFS_CHECK_PROXY(W1,W2,DESC,PORT_REQUIRED,MSG,N) \
{ \
    char *orig_text1 = 0;	\
    char *orig_text2 = 0;	\
    char *text = 0;\
    orig_text1 = fe_GetTextField (W1); \
    text = fe_StringTrim (orig_text1); \
    if (text && *text) { \
	    PREFS_CHECK_HOST_1 (text, DESC, MSG, N); \
        text = 0; \
        orig_text2 =  fe_GetTextField(W2); \
		text = fe_StringTrim (orig_text2); \
		if ((!text || !*text) && PORT_REQUIRED) { \
			PR_snprintf (MSG, 200, \
						 XP_GetString(XFE_NO_PORT_NUMBER), DESC); \
			MSG += strlen (MSG); \
		} \
	    if (orig_text2) XtFree(orig_text2); \
	} \
	if (orig_text1) XtFree(orig_text1); \
}

// Top level categories

#define CAT_APPEARANCE           "Appearance"
#define CAT_BROWSER              "Browser"
#define CAT_MAILNEWS             "Mail & Groups"
#define CAT_EDITOR               "Editor"
#ifdef PREFS_UNSUPPORTED
#define CAT_OFFLINE              "Offline"
#endif /* PREFS_UNSUPPORTED */
#define CAT_ADVANCED             "Advanced"

class XFE_PrefsDialog;
class XFE_PrefsPage;

struct PrefsCategory {
	char                  *name;
	char                  *categoryName;
	char                  *categoryDesc;
	Boolean                leaf;
	struct PrefsCategory  *parent;
	struct PrefsCategory  *children;
	int                    numChildren;
	Boolean                open;
	XFE_PrefsPage         *page;
};

// ************************* XFE_PrefsDialog  *************************

class XFE_PrefsDialog : public XFE_Dialog
{
public:

	// Constructors, Destructors

	XFE_PrefsDialog(Widget     parent,    
					char      *name,  
					MWContext *context,
					Boolean    modal = TRUE);

	virtual ~XFE_PrefsDialog();

	MWContext *getContext();            // Getting MWContext
	int getVisibleCategoryCount();      
	PrefsCategory *getCategoryByIndex(int);
	PrefsCategory *getCategoryByName(const char *);
	PrefsCategory *getCurrentCategory();
	void refreshCategories();
	Boolean isCurrentCategory(PrefsCategory *);
	Widget getOutline();
	Widget getPrefsParent();
	Widget getPageForm();
	Widget getDialogChrome();

	virtual void show();                // pop up dialog
    void setContext(MWContext *);       // Setting MWContext (for now) 		   
	void calcVisibleCategoryCount();    // Calculate category count
	void setCurrentCategory(PrefsCategory *);
	void openCategory(const char *);    // open a category
	void closeCategory(const char *);   // close a category
	void setDoInitInSetPage(Boolean);   // set the flag
	void saveChanges();                 // save all the changes
	Dimension calculateWidth();         // calculate the dialog width 
	Dimension calculateHeight();        // calculate the dialog height

	// Callbacks - main

	static void prefsMainCb_ok(Widget, XtPointer, XtPointer);
	static void prefsMainCb_cancel(Widget, XtPointer, XtPointer);
	static void prefsMainCb_help(Widget, XtPointer, XtPointer);
	static void prefsMainCb_destroy(Widget, XtPointer, XtPointer);

	// Callbacks - category outline

    static Boolean categoryDataFunc(Widget, void*, int, fe_OutlineDesc*, int);
	static void categoryClickFunc(Widget, void*, int, int, const char*,
								  int, int, Boolean, Boolean, int);
	static void categoryIconFunc(Widget, void*, int, int);

private:

	void createRegions();
	void initCategories();
	void deleteCategories();
	void newPage(XFE_PrefsPage *&, int);

	Widget                   m_wPageLabel;
	Widget                   m_wPageTitle;
	Widget                   m_wPageForm;
	Widget                   m_wOutline;
	MWContext               *m_context;
	int                      m_numPages;
	XFE_PrefsPage          **m_pages;
	char                    *m_columnWidths;
	fe_OutlineHeaderDesc    *m_outlineHeaders;
	int                      m_numCategories;
	PrefsCategory           *m_categories;
	int                      m_visibleCategoryCount;
	PrefsCategory           *m_currentCategory;
	Boolean                  m_initPageLabel;
};

// ************************* XFE_PrefsPage  *************************

class XFE_PrefsPage
{
public:

	// Constructors, Destructors

	XFE_PrefsPage(XFE_PrefsDialog  *dialog);
    XFE_PrefsPage(Widget w);
	virtual ~XFE_PrefsPage();

	// Manipulators

	virtual void create() = 0;
	virtual void init() = 0;
	virtual void map();
	virtual void unmap();
	virtual Boolean verify();
	virtual void install() = 0;
	virtual void save() = 0;

	void setCreated(Boolean);
	void setInitialized(Boolean);
	void setDoInitInMap(Boolean);
	void setChanged(Boolean);

	// Gets

	Boolean isCreated();
	Boolean isInitialized();
	Boolean doInitInMap();
	Boolean isChanged();
	MWContext *getContext();
	XFE_PrefsDialog *getPrefsDialog();
	Dimension getWidth();
	Dimension getHeight();

protected:

	// Data

	MWContext          *m_context;
	XFE_PrefsDialog    *m_prefsDialog;
	Widget              m_wPageForm;   /* in the prefs dialog */
	Widget              m_wPage;       /* top level form in this page */
	Boolean             m_created;
	Boolean             m_initialized;
	Boolean             m_doInitInMap;
	Boolean             m_changed;
};

// ************************* XFE_PrefsPageAppearance  *************************

class XFE_PrefsPageGeneralAppearance : public XFE_PrefsPage
{
public:

	// Constructors, Destructors

	XFE_PrefsPageGeneralAppearance(XFE_PrefsDialog  *dialog);
	virtual ~XFE_PrefsPageGeneralAppearance();

	// Manipulators

	virtual void create();
	virtual void init();
	virtual void install();
	virtual void save();

	// Gets

	PrefsDataGeneralAppearance *getData();

	// Callbacks - page GeneralAppearance

	static void cb_setToolbarAttr(Widget, XtPointer, XtPointer);

private:	

	// Data

	PrefsDataGeneralAppearance        *m_prefsDataGeneralAppearance;
	XP_Bool                            m_toolbar_needs_updating;
};

// ************************* XFE_PrefsPageGeneralFonts  *************************

class XFE_PrefsPageGeneralFonts : public XFE_PrefsPage
{
public:

	// Constructors, Destructors

	XFE_PrefsPageGeneralFonts(XFE_PrefsDialog  *dialog);
	virtual ~XFE_PrefsPageGeneralFonts();

	// Manipulators

	virtual void create();
	virtual void relayout(PrefsDataGeneralFonts *fep);
	virtual void init();
	virtual void install();
	virtual void save();

	// Gets

	PrefsDataGeneralFonts *getData();

	// Callbacks - page General/Fonts

	static void cb_charSet(Widget, XtPointer, XtPointer);
	static void cb_toggleUseFont(Widget, XtPointer, XtPointer);
	static void cb_allowScaling(Widget, XtPointer, XtPointer);

private:	

	// Data

	PrefsDataGeneralFonts        *m_prefsDataGeneralFonts;
};

// ************************* XFE_PrefsPageGeneralColors  *************************

class XFE_PrefsPageGeneralColors : public XFE_PrefsPage
{
public:

	// Constructors, Destructors

	XFE_PrefsPageGeneralColors(XFE_PrefsDialog  *dialog);
	virtual ~XFE_PrefsPageGeneralColors();

	// Manipulators

	virtual void create();
	virtual void init();
	virtual void install();
	virtual void save();

	// Gets

	PrefsDataGeneralColors *getData();

	// Callbacks - page General/Colors

	static void cb_activateColor(Widget, XtPointer, XtPointer);
	static void cb_defaultColor(Widget, XtPointer, XtPointer);

private:	

	// Data

	PrefsDataGeneralColors   *m_prefsDataGeneralColors;
	XP_Bool                   m_underlinelinks_changed;
	XP_Bool                   m_colors_changed;
};

// ************************* XFE_PrefsPageGeneralAdvanced  *************************

class XFE_PrefsPageGeneralAdvanced : public XFE_PrefsPage
{
public:

	// Constructors, Destructors

	XFE_PrefsPageGeneralAdvanced(XFE_PrefsDialog  *dialog);
	virtual ~XFE_PrefsPageGeneralAdvanced();

	// Manipulators

	virtual void create();
	virtual void init();
	virtual void install();
	virtual void save();

	// Gets

	PrefsDataGeneralAdvanced *getData();

	// Callbacks - page General/Advanced

	static void cb_toggleCookieState(Widget, XtPointer, XtPointer);

private:	

	// Data

	PrefsDataGeneralAdvanced        *m_prefsDataGeneralAdvanced;

	XP_Bool    m_toolbar_needs_updating;
};

// ************************* XFE_PrefsPageGeneralAppl  *************************

class XFE_PrefsPageGeneralAppl : public XFE_PrefsPage
{
public:

	// Constructors, Destructors

	XFE_PrefsPageGeneralAppl(XFE_PrefsDialog  *dialog);
	virtual ~XFE_PrefsPageGeneralAppl();

	// Manipulators

	virtual void create();
	virtual void init();
	virtual void install();
	virtual void save();
	virtual void unmap();
	void setModified(Boolean);

	// Gets

	PrefsDataGeneralAppl *getData();

	// Callbacks - page General/Applications

	static void cb_new(Widget, XtPointer, XtPointer);
	static void cb_edit(Widget, XtPointer, XtPointer);
	static void cb_delete(Widget, XtPointer, XtPointer);
	static void cb_browse(Widget, XtPointer, XtPointer);

private:	

	// Data

	PrefsDataGeneralAppl        *m_prefsDataGeneralAppl;
};

// ************************* XFE_PrefsPageGeneralCache  *************************

class XFE_PrefsPageGeneralCache : public XFE_PrefsPage
{
public:

	// Constructors, Destructors

	XFE_PrefsPageGeneralCache(XFE_PrefsDialog  *dialog);
	virtual ~XFE_PrefsPageGeneralCache();

	// Manipulators

	virtual void create();
	virtual void init();
	virtual void install();
	virtual void save();

	// Gets

	PrefsDataGeneralCache *getData();

	// Callbacks - page General/Cache

	static void cb_browse(Widget, XtPointer, XtPointer);
	static void cb_clearDisk(Widget, XtPointer, XtPointer);
	static void cb_clearMem(Widget, XtPointer, XtPointer);

private:	

	// Data

	PrefsDataGeneralCache        *m_prefsDataGeneralCache;
};

// ************************* XFE_PrefsPageGeneralProxies  *************************

class XFE_PrefsPageGeneralProxies : public XFE_PrefsPage
{
public:

	// Constructors, Destructors

	XFE_PrefsPageGeneralProxies(XFE_PrefsDialog  *dialog);
	virtual ~XFE_PrefsPageGeneralProxies();

	// Manipulators

	virtual void create();
	virtual void init();
	virtual void install();
	virtual void save();

	// Gets

	PrefsDataGeneralProxies *getData();

	// Callbacks - page General/Proxies

	static void cb_reloadProxies(Widget, XtPointer, XtPointer);
	static void cb_setProxies(Widget, XtPointer, XtPointer);
	static void cb_viewProxies(Widget, XtPointer, XtPointer);

private:	

	// Data

	PrefsDataGeneralProxies        *m_prefsDataGeneralProxies;
};

// ************************* XFE_PrefsPageBrowser  *************************

class XFE_PrefsPageBrowser : public XFE_PrefsPage
{
public:

	// Constructors, Destructors

	XFE_PrefsPageBrowser(XFE_PrefsDialog  *dialog);
	virtual ~XFE_PrefsPageBrowser();

	// Manipulators

	virtual void create();
	virtual void init();
	virtual void install();
	virtual void save();

	// Gets

	PrefsDataBrowser *getData();

	// Callbacks - page Browser

	static void cb_browse(Widget, XtPointer, XtPointer);
	static void cb_setStartupPage(Widget, XtPointer, XtPointer);
	static void cb_expireNow(Widget, XtPointer, XtPointer);
	static void cb_useCurrent(Widget, XtPointer, XtPointer);

private:	

	// Data

	PrefsDataBrowser  *m_prefsDataBrowser;
	XP_Bool            m_home_changed;
};

// ************************* XFE_PrefsPageBrowserLang  *************************

class XFE_PrefsPageBrowserLang : public XFE_PrefsPage, public XFE_Outlinable
{
public:

	// Constructors, Destructors

	XFE_PrefsPageBrowserLang(XFE_PrefsDialog  *dialog);
	virtual ~XFE_PrefsPageBrowserLang();

	// Manipulators

	virtual void create();
	virtual void init();
	virtual void install();
	virtual void save();

	void    insertLang(char *lang);
	void    insertLangAtPos(int pos, char *lang);
	void    deleteLangAtPos(int pos);
	void    swapLangs(int pos1, int pos2, int selPos);
	void    setSelectionPos(int pos);
	void    deselectPos(int pos);

	// Gets

	PrefsDataBrowserLang *getData();

	// Outlinable interface methods
	virtual void *ConvFromIndex(int index);
	virtual int ConvToIndex(void *item);

	virtual char *getColumnName(int column);
	virtual char *getColumnHeaderText(int column);
	virtual fe_icon *getColumnHeaderIcon(int column);
	virtual EOutlinerTextStyle getColumnHeaderStyle(int column);
	virtual void *acquireLineData(int line);
	virtual void getTreeInfo(XP_Bool *expandable, XP_Bool *is_expanded, int *depth, 
							 OutlinerAncestorInfo **ancestor);
	virtual EOutlinerTextStyle getColumnStyle(int column);
	virtual char *getColumnText(int column);
	virtual fe_icon *getColumnIcon(int column);
	virtual void releaseLineData();

	virtual void Buttonfunc(const OutlineButtonFuncData *data);
	virtual void Flippyfunc(const OutlineFlippyFuncData *data);

	virtual XFE_Outliner *getOutliner();
	// Get tooltipString & docString; 
	// returned string shall be freed by the callee
	// row < 0 indicates heading row; otherwise it is a content row
	// (starting from 0)
	//
	virtual char *getCellTipString(int /* row */, int /* column */) {return NULL;}
	virtual char *getCellDocString(int /* row */, int /* column */) {return NULL;}

	// Callbacks - page BrowserLang

	static void cb_add(Widget, XtPointer, XtPointer);
	static void cb_delete(Widget, XtPointer, XtPointer);
	static void cb_promote(Widget, XtPointer, XtPointer);
	static void cb_demote(Widget, XtPointer, XtPointer);

private:	

	// Data

	static const int OUTLINER_COLUMN_ORDER;
	static const int OUTLINER_COLUMN_LANG;
	static const int OUTLINER_COLUMN_MAX_LENGTH;
	static const int OUTLINER_INIT_POS;

	PrefsDataBrowserLang   *m_prefsDataBrowserLang;
	int                     m_rowIndex;
};

#ifdef MOZ_MAIL_NEWS

// ************************* XFE_PrefsPageMailNews  *************************

class XFE_PrefsPageMailNews : public XFE_PrefsPage
{
public:

	// Constructors, Destructors

	XFE_PrefsPageMailNews(XFE_PrefsDialog  *dialog);
	virtual ~XFE_PrefsPageMailNews();

	// Manipulators

	virtual void create();
	virtual void init();
	virtual void install();
	virtual void save();

	// Gets

	PrefsDataMailNews *getData();

	// Callbacks - page MailNews

	static void cb_toggleMsgFont(Widget, XtPointer, XtPointer);
	static void cb_quotedTextColor(Widget, XtPointer, XtPointer);

private:	

	// Data

	PrefsDataMailNews        *m_prefsDataMailNews;
	XP_Bool                   m_refresh_needed;
};
#endif

// ************************* XFE_PrefsPageMailNewsIdentity  *************************

class XFE_PrefsPageMailNewsIdentity : public XFE_PrefsPage
{
public:

	// Constructors, Destructors

	XFE_PrefsPageMailNewsIdentity(XFE_PrefsDialog  *dialog);
	virtual ~XFE_PrefsPageMailNewsIdentity();

	// Manipulators

	virtual void create();
	virtual void init();
	virtual void install();
	virtual void save();
	virtual Boolean verify();

	// Gets

	PrefsDataMailNewsIdentity *getData();

	// Callbacks - page Mail News/Identity

#ifdef MOZ_MAIL_NEWS
	static void cb_browse(Widget, XtPointer, XtPointer);
	static void cb_toggleAttachCard(Widget, XtPointer, XtPointer);
#endif

private:	

	// Data

	PrefsDataMailNewsIdentity        *m_prefsDataMailNewsIdentity;
};

#ifdef MOZ_MAIL_NEWS

// Incoming Mail Servers Data
class XFE_PrefsIncomingMServer
{
public:

    XFE_PrefsIncomingMServer(Widget incomingServerBox, XFE_PrefsPage *page);
    virtual ~XFE_PrefsIncomingMServer();
    XFE_PrefsPage *getPage() { return prefsPage; };
    void refresh_incoming();
    XP_Bool has_pop();
    XP_Bool has_imap();
    XP_Bool has_many_imap();
    
private:
    void select_incoming();
    
    static void cb_select_incoming(Widget w, XtPointer closure,
                                   XtPointer callData);
    static void cb_new_incoming(Widget w, XtPointer closure,
                                XtPointer callData);
    static void cb_edit_incoming(Widget w, XtPointer closure,
                                 XtPointer callData);
    static void cb_delete_incoming(Widget w, XtPointer closure,
                                   XtPointer callData);
    static void cb_refresh(Widget w, XtPointer closure, XtPointer callData);
    
    XFE_PrefsMServerDialog *m_mserver_dialog;
    XFE_PrefsPage* prefsPage;
    void newIncoming();
    void editIncoming();
    void deleteIncoming();
    Widget m_incoming;
    Widget m_button_form;
    Widget m_new_button;
    Widget m_edit_button;
    Widget m_delete_button;
    
};

// Outgoing Server Data
class XFE_PrefsOutgoingServer
{
public:
    XFE_PrefsOutgoingServer(Widget outgoingServerBox);
    Widget get_server_name() { return m_servername_text; };
    Widget get_user_name() { return m_username_text; };
    HG02621
 private:
    
    Widget m_outgoing;
    
    // Labels
    Widget m_servername_text;           // Text field
    Widget m_username_text;         // Text field
    
    HG19282
};

// Local mail directory data
class XFE_PrefsLocalMailDir
{
public:
    XFE_PrefsLocalMailDir(Widget localMailDirBox,  XFE_PrefsPage *page);
    static void cb_choose(Widget w, XtPointer closure,
                          XtPointer callData);
    Widget get_local_dir_text() { return m_local_dir_text; };
    XFE_PrefsPage *getPage() { return prefsPage; };
    
private:
    XFE_PrefsPage *prefsPage;
    Widget m_local_dir_text;
};


 // News Servers Data
class XFE_PrefsNewsServer
{
public:
    XFE_PrefsNewsServer(Widget ServerBox, XFE_PrefsPage *page);
    XFE_PrefsPage *getPage() { return prefsPage; };
    
private:
    static void cb_option(Widget, XtPointer, XtPointer);
    static void cb_new_news_server(Widget w, XtPointer closure,
                                   XtPointer callData);
    static void cb_edit_news_server(Widget w, XtPointer closure,
                                    XtPointer callData);
    static void cb_delete_news_server(Widget w, XtPointer closure,
                                      XtPointer callData);
    
    XFE_PrefsPage *prefsPage;
    Widget news_servers;
    Widget button_form;
    Widget new_button;
    Widget edit_button;
    Widget delete_button;
};
  
// ************************* XFE_PrefsPageMailNewsMserver  *************************

class XFE_PrefsPageMailNewsMserver : public XFE_PrefsPage
{
public:

	// Constructors, Destructors

	XFE_PrefsPageMailNewsMserver(XFE_PrefsDialog  *dialog);
	virtual ~XFE_PrefsPageMailNewsMserver();

	// Manipulators

	virtual void create();
	virtual void init();
	virtual void install();
	virtual void save();
	virtual Boolean verify();

	// Callbacks - page Mail News/Mail Server

	static void cb_browseApplication(Widget, XtPointer, XtPointer);
	static void cb_more(Widget, XtPointer, XtPointer);
	static void cb_toggleServerType(Widget, XtPointer, XtPointer);
	static void cb_toggleApplication(Widget, XtPointer, XtPointer);

private:	
   XFE_PrefsIncomingMServer *xfe_incoming;
   XFE_PrefsOutgoingServer *xfe_outgoing;
   XFE_PrefsLocalMailDir   *xfe_local_mail;

 
	// Data

};

// ************************* XFE_PrefsPageMailNewsCopies ******************

class XFE_PrefsPageMailNewsCopies : public XFE_PrefsPage
{
public:
  
    XFE_PrefsPageMailNewsCopies(XFE_PrefsDialog *dialog);
    virtual ~XFE_PrefsPageMailNewsCopies();
    
    virtual void create();
    
    MSG_FolderInfo *GetFolderInfoFromPath(char *path,
                                          MSG_FolderInfo *parent=NULL);
    virtual void init();
    virtual void install();
    virtual void save();
    virtual Boolean verify();
    
private:
    MSG_FolderInfo *m_mailFcc, *m_newsFcc, *m_draftsFcc, *m_templatesFcc;
    MSG_Master *m_master;
    XFE_PrefsMailFolderDialog *m_chooseDialog;
    
    MSG_FolderInfo *chooseFcc(int l10n_name, MSG_FolderInfo *selected_folder);
    XmString VerboseFolderName(MSG_FolderInfo *,int folder_string_id=0);
    MSG_FolderInfo *GetSpecialFolder(char *name, int l10n_name);
    
    static void cb_chooseMailFcc(Widget, XtPointer closure,XtPointer);
    static void cb_chooseNewsFcc(Widget, XtPointer closure, XtPointer);
    static void cb_chooseDraftsFcc(Widget, XtPointer closure, XtPointer);
    static void cb_chooseTemplatesFcc(Widget, XtPointer closure, XtPointer);
    
    static void cb_gotMailFcc(Widget, XtPointer, XtPointer);
    static void cb_gotNewsFcc(Widget, XtPointer, XtPointer);
    static void cb_gotDraftsFcc(Widget, XtPointer, XtPointer);
    
    static void cb_gotTemplatesFcc(Widget, XtPointer, XtPointer);
    
         Widget createMailFrame(Widget parent, Widget attachTo);
         Widget createNewsFrame(Widget parent, Widget attachTo);
         Widget createDTFrame(Widget parent, Widget attachTo);

     Widget m_mail_self_toggle;
     Widget m_mail_other_toggle;
     Widget m_mail_other_text;
     Widget m_mail_fcc_toggle;
     Widget m_mail_choose_button;

     Widget m_news_self_toggle;
     Widget m_news_other_toggle;
     Widget m_news_other_text;
     Widget m_news_fcc_toggle;
     Widget m_news_choose_button;

     Widget m_drafts_save_label;
     Widget m_drafts_choose_button;

     Widget m_templates_save_label;
     Widget m_templates_choose_button;

 };

 // ************************* XFE_PrefsPageMailNewsHTML ******************


class XFE_PrefsPageMailNewsHTML : public XFE_PrefsPage
{
public:
    
    XFE_PrefsPageMailNewsHTML(XFE_PrefsDialog *dialog);
    virtual ~XFE_PrefsPageMailNewsHTML();
    
    virtual void create();
    virtual void init();
    virtual void install();
    virtual void save();
    virtual Boolean verify();
    
private:
    Widget createUseHTMLFrame(Widget parent, Widget attachTo);
    Widget createNoHTMLFrame(Widget parent, Widget attachTo);
    Widget m_usehtml_toggle;
    Widget m_nohtml_ask_toggle;
    Widget m_nohtml_text_toggle;
    Widget m_nohtml_html_toggle;
    Widget m_nohtml_both_toggle;
    
};



// ************************* XFE_PrefsPageMailNewsReceipts ******************


class XFE_PrefsPageMailNewsReceipts : public XFE_PrefsPage
{
public:
    
    XFE_PrefsPageMailNewsReceipts(XFE_PrefsDialog *dialog);
    virtual ~XFE_PrefsPageMailNewsReceipts();
    
    virtual void create();
    virtual void init();
    virtual void install();
    virtual void save();
    virtual Boolean verify();
    
    
private:
    Widget createRequestReceiptsFrame(Widget parent, Widget attachTo);
    Widget createReceiptsArriveFrame(Widget parent, Widget attachTo);
    Widget createReceiveReceiptsFrame(Widget parent, Widget attachTo);
    Widget m_dsn_toggle;
    Widget m_mdn_toggle;
    Widget m_both_toggle;
    Widget m_inbox_toggle;
    Widget m_sentmail_toggle;
    Widget m_never_toggle;
    Widget m_some_toggle;
};

#endif /* MOZ_MAIL_NEWS */

#ifdef EDITOR

// ************************* XFE_PrefsPageEditor  *************************

class XFE_PrefsPageEditor : public XFE_PrefsPage
{
public:

	// Constructors, Destructors

	XFE_PrefsPageEditor(XFE_PrefsDialog  *dialog);
	virtual ~XFE_PrefsPageEditor();

	// Manipulators

	virtual void create();
	virtual void init();
	virtual void install();
	virtual void save();
	virtual Boolean verify();

	// Gets

	PrefsDataEditor *getData();

	// Callbacks - page Editor

	static void cb_changed(Widget, XtPointer, XtPointer);
	static void cb_browseToTextField(Widget, XtPointer, XtPointer);
	static void cb_restoreTemplate(Widget, XtPointer, XtPointer);
	static void cb_autosaveToggle(Widget, XtPointer, XtPointer);
	static void cb_browseTemplate(Widget, XtPointer, XtPointer);

private:	

	// Data

	PrefsDataEditor        *m_prefsDataEditor;
};

// ************************* XFE_PrefsPageEditorAppearance  *************************

class XFE_PrefsPageEditorAppearance : public XFE_PrefsPage
{
public:

	// Constructors, Destructors

	XFE_PrefsPageEditorAppearance(XFE_PrefsDialog  *dialog);
	virtual ~XFE_PrefsPageEditorAppearance();

	// Manipulators

	virtual void create();
	virtual void init();
	virtual void install();
	virtual void save();

	// Gets

	PrefsDataEditorAppearance *getData();

	// Callbacks - page Editor/Appearance

private:	

	// Data

	PrefsDataEditorAppearance        *m_prefsDataEditorAppearance;
};

// ************************* XFE_PrefsPageEditorPublish  *************************

class XFE_PrefsPageEditorPublish : public XFE_PrefsPage
{
public:

	// Constructors, Destructors

	XFE_PrefsPageEditorPublish(XFE_PrefsDialog  *dialog);
	virtual ~XFE_PrefsPageEditorPublish();

	// Manipulators

	virtual void create();
	virtual void init();
	virtual void install();
	virtual void save();

	// Gets

	PrefsDataEditorPublish *getData();

	// Callbacks - page Editor/Publish

	static void cb_changed(Widget, XtPointer, XtPointer);
	static void cb_passwordChanged(Widget, XtPointer, XtPointer);

private:	

	// Data

	PrefsDataEditorPublish        *m_prefsDataEditorPublish;
};

#endif /* EDITOR */

#ifdef PREFS_UNSUPPORTED
// ************************* XFE_PrefsPageOffline  *************************

class XFE_PrefsPageOffline : public XFE_PrefsPage
{
public:

	// Constructors, Destructors

	XFE_PrefsPageOffline(XFE_PrefsDialog  *dialog);
	virtual ~XFE_PrefsPageOffline();

	// Manipulators

	virtual void create();
	virtual void init();
	virtual void install();
	virtual void save();

	// Gets

	PrefsDataOffline *getData();

	// Callbacks - page Offline

	static void cb_toggleStartup(Widget, XtPointer, XtPointer);

private:	

	// Data

	PrefsDataOffline        *m_prefsDataOffline;
};

#ifdef MOZ_MAIL_NEWS

// ************************* XFE_PrefsPageOfflineNews  *************************

class XFE_PrefsPageOfflineNews : public XFE_PrefsPage
{
public:

	// Constructors, Destructors

	XFE_PrefsPageOfflineNews(XFE_PrefsDialog  *dialog);
	virtual ~XFE_PrefsPageOfflineNews();

	// Manipulators

	virtual void create();
	virtual void init();
	virtual void install();
	virtual void save();

	// Gets

	PrefsDataOfflineNews *getData();

	// Callbacks - page OfflineNews

	static void cb_toggleDownLoadByDate(Widget, XtPointer, XtPointer);

private:	

	// Data

	PrefsDataOfflineNews        *m_prefsDataOfflineNews;
};

#endif /* MOZ_MAIL_NEWS */

#endif /* PREFS_UNSUPPORTED */

// ************************* XFE_PrefsPageDiskSpace  *************************

class XFE_PrefsPageDiskSpace : public XFE_PrefsPage
{
public:

	// Constructors, Destructors

	XFE_PrefsPageDiskSpace(XFE_PrefsDialog  *dialog);
	virtual ~XFE_PrefsPageDiskSpace();

	// Manipulators

	virtual void create();
	virtual void init();
	virtual void install();
	virtual void save();

	// Gets

	PrefsDataDiskSpace *getData();

	// Callbacks - page DiskSpace

	// static void cb_more(Widget, XtPointer, XtPointer);
	static void cb_toggleKeepNews(Widget, XtPointer, XtPointer);

private:	

	// Data

	PrefsDataDiskSpace        *m_prefsDataDiskSpace;
};

#ifdef PREFS_UNSUPPORTED

// ************************* XFE_PrefsPageHelpFiles  *************************

class XFE_PrefsPageHelpFiles : public XFE_PrefsPage
{
public:

	// Constructors, Destructors

	XFE_PrefsPageHelpFiles(XFE_PrefsDialog  *dialog);
	virtual ~XFE_PrefsPageHelpFiles();

	// Manipulators

	virtual void create();
	virtual void init();
	virtual void install();
	virtual void save();

	// Gets

	PrefsDataHelpFiles *getData();

	// Callbacks - page HelpFiles

	static void cb_toggleHelpSite(Widget, XtPointer, XtPointer);
	static void cb_browse(Widget, XtPointer, XtPointer);

private:	

	// Data

	PrefsDataHelpFiles        *m_prefsDataHelpFiles;
};
#endif

extern "C" void
fe_showEditorPreferences(XFE_Component *topLevel, MWContext *context);

#endif /* _xfe_prefsdialog_h */
