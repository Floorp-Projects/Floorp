/* $Id: QtContext.h,v 1.2 1998/10/20 02:40:45 cls%seawood.org Exp $
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

#ifndef _QTCONTEXT
#define _QTCONTEXT

#include <client.h>
#include <fe_proto.h>
#include <xp_file.h>
#include <qwidget.h>
#include "finding.h"

struct DialogPool;
class QMenuBar;


#if defined(XP_UNIX)
#define CONTEXT_DATA(context)	((QtContext*)((context)->fe.data))
#elif defined(XP_WIN)
#define CONTEXT_DATA(context)	((QtContext*)((context)->fe.cx))
#endif


class QtContext : public QObject {
    Q_OBJECT
public:
    static QtContext* qt( MWContext* c ) { return CONTEXT_DATA(c); }

    virtual ~QtContext();

    // Below are used by the FE_* functions.

    // MANY of them should be moved wholly into QtBrowserContext

    virtual bool securityDialog(int state, char *prefs_toggle);

    void setPasswordEnabled(bool usePW);

    void setPasswordAskPrefs(int askPW, int timeout);

    void setCipherPrefs(char *cipher);

    void alert(const char *message);
    virtual bool confirm( const char* message );
    static bool confirm( QtContext* cx, const char* message );
    void message(const char* message);

    char * promptMessageSubject();

    void rememberPopPassword(const char *password);

    int promptForFileName(const char *prompt_string,
		      const char *default_path,
		      bool file_must_exist_p,
		      bool directories_allowed_p,
		      ReadFileNameCallbackFunction fn,
		      void *closure);

    char * prompt(const char *message, const char *deflt);

    char * promptWithCaption(const char *caption,
		      const char *message, const char *deflt);

    char * promptPassword(const char *message);

    static MWContext * makeNewWindow(URL_Struct *url=0,
		 char       *window_name=0,
		 Chrome     *chrome=0,
		 MWContext  *parent=0);

    virtual void setRefreshURLTimer(uint32 secs, char *url);
    virtual void killRefreshURLTimer() {};

    void connectToRemoteHost(int url_type, char *hostname,
			char *port, char *username);

    int32 getContextID();

    void createEmbedWindow(NPEmbeddedApp *app);

    void saveEmbedWindow(NPEmbeddedApp *app);

    void restoreEmbedWindow(NPEmbeddedApp *app);

    void destroyEmbedWindow(NPEmbeddedApp *app);

    void setDrawable(CL_Drawable *drawable);

    void progress(const char * message);

    void setCallNetlibAllTheTime();

    void clearCallNetlibAllTheTime();

    void graphProgressInit(URL_Struct *	,
					  int32 content_length);

    void graphProgressDestroy(URL_Struct *	,
			int32			content_length,
			int32			total_bytes_read);

    void graphProgress(URL_Struct *	,
				  int32 ,
				  int32 bytes_since_last_time,
				  int32 );

    int  fileSortMethod();

    void layoutNewDocument(URL_Struct *url,
		      int32 *iWidth, int32 *iHeight,
		       int32 *mWidth, int32 *mHeight);

    virtual void setDocTitle(char *title);

    void finishedLayout();

    void beginPreSection();

    void endPreSection();

    char * translateISOText(int charset, char *ISO_Text);

    void getTextInfo(LO_TextStruct *text,
		LO_TextInfo *text_info);

    void getTextFrame(LO_TextStruct *text, int32 start,
                      int32 end, XP_Rect *frame);

    void getEmbedSize(LO_EmbedStruct *embed_struct,
		  NET_ReloadMethod force_reload);

    void getJavaAppSize(LO_JavaAppStruct *java_struct,
		    NET_ReloadMethod reloadMethod);

    void getFormElementInfo(LO_FormElementStruct *form);

    virtual void getFormElementValue(LO_FormElementStruct *form,
						 bool delete_p, bool submit_p);

    void resetFormElement(LO_FormElementStruct *form);

    void setFormElementToggle(LO_FormElementStruct *form,
			  bool state);

    void freeEmbedElement(LO_EmbedStruct *embed_struct);

    void freeJavaAppElement(struct LJAppletData *appletData);

    void hideJavaAppElement(struct LJAppletData *appletData);

    void freeEdgeElement(LO_EdgeStruct *edge);

    void formTextIsSubmit(LO_FormElementStruct *form);

    void setProgressBarPercent(int32 percent);

    virtual void setBackgroundColor(uint8 red, uint8 green, uint8 blue);

    void displaySubtext(int iLocation,
		   LO_TextStruct *text, int start_pos, int end_pos,
		   bool need_bg);

    void displayText(int iLocation, LO_TextStruct *text,
	bool need_bg);

    void displayEmbed(int iLocation, LO_EmbedStruct *embed_struct);

    void displayJavaApp(int iLocation, LO_JavaAppStruct *java_struct);

    void displayEdge(int loc, LO_EdgeStruct *edge);

    void displayTable(int loc, LO_TableStruct *ts);

    void displayCell(int loc, LO_CellStruct *cell);

    void displaySubDoc(int loc, LO_SubDocStruct *sd);

    void displayLineFeed(int iLocation, LO_LinefeedStruct *line_feed, bool need_bg);

    void displayHR(int iLocation, LO_HorizRuleStruct *hr);

    void displayBullet(int iLocation, LO_BulletStruct *bullet);

    virtual void displayFormElement(int iLocation,
					   LO_FormElementStruct *form);

    void displayBorder(int iLocation, int x, int y, int width,
                  int height, int bw, LO_Color *color, LO_LineStyle style);

    void displayFeedback(int iLocation, LO_Element *element);

    void eraseBackground(int iLocation, int32 x, int32 y,
                    uint32 width, uint32 height, LO_Color *bg);

    void clearView(int which);

    virtual void setDocDimension(int iLocation, int32 iWidth, int32 iLength);

    virtual void setDocPosition(int iLocation, int32 x, int32 y);

    void getDocPosition(int iLocation, int32 *iX, int32 *iY); // NOT virtual

    void enableClicking();

    void drawJavaApp(int iLocation, LO_JavaAppStruct *java_struct);

    void allConnectionsComplete();

    void releaseTextAttrFeData(LO_TextAttr *attr);

    void freeFormElement(LO_FormElementData *form_data);

    virtual int enableBackButton();

    virtual int enableForwardButton();

    virtual int disableBackButton();

    virtual int disableForwardButton();

    void updateStopState();

    void * freeGridWindow(bool save_history);

    void restructureGridWindow(int32 x, int32 y,
        int32 width, int32 height);

    void getFullWindowSize(int32 *width, int32 *height);

    void getEdgeMinSize(int32 *size_p);

    void loadGridCellFromHistory(void *hist,
			   NET_ReloadMethod force_reload);

    void shiftImage(LO_ImageStruct *lo_image);

    void scrollDocTo(int iLocation, int32 x, int32 y);

    void scrollDocBy(int iLocation, int32 deltax, int32 deltay);

    void backCommand();

    void forwardCommand();

    void homeCommand();

    void printCommand();

    void getWindowOffset(int32 *sx, int32 *sy);

    void getScreenSize(int32 *sx, int32 *sy);

    void getAvailScreenRect(int32 *sx, int32 *sy, int32 *left, int32 *top);

    void getPixelAndColorDepth(int32 *pixelDepth,
			 int32 *colorDepth);

    void setWindowLoading(URL_Struct *url,
	Net_GetUrlExitFunc **exit_func_p);

    void raiseWindow();

    void destroyWindow();

    void getDocAndWindowPosition(int32 *pX, int32 *pY,
    int32 *pWidth, int32 *pHeight );

    void displayTextCaret(int iLocation, LO_TextStruct* text,
		    int char_offset);

    void  displayImageCaret(LO_ImageStruct*        image,
		     ED_CaretObjectPosition caretPos);

    void displayGenericCaret(LO_Any * image,
                        ED_CaretObjectPosition caretPos );

    bool  getCaretPosition(LO_Position* where,
		    int32* caretX, int32* caretYLow, int32* caretYHigh
);

    void destroyCaret();

    void showCaret();

    void  documentChanged(int32 p_y, int32 p_height);

    void setNewDocumentProperties();

    void imageLoadDialog();

    void imageLoadDialogDestroy();

    void  saveDialogCreate(int nfiles, ED_SaveDialogType saveType);

    void  saveDialogSetFilename(char* filename);

    void  finishedSave(int status,
		char *pDestURL, int iFileNumber);

    void  saveDialogDestroy(int status, char* file_url);

    bool saveErrorContinueDialog(char*        filename,
			   ED_FileError error);

    void clearBackgroundImage();

    void editorDocumentLoaded();

    void displayAddRowOrColBorder(XP_Rect *pRect,
                            bool bErase);

    void  displayEntireTableOrCell(LO_Element* pLoElement);

    void focusInputElement(LO_Element *element);

    void blurInputElement(LO_Element *element);

    void selectInputElement(LO_Element *element);

    void changeInputElement(LO_Element *element);

    void submitInputElement(LO_Element *element);

    void clickInputElement(LO_Element *xref);

    virtual bool handleLayerEvent(CL_Layer *layer,
				  CL_Event *layer_event);

    bool handleEmbedEvent(LO_EmbedStruct *embed,
                           CL_Event *event);

    void updateChrome(Chrome *chrome);

    void queryChrome(Chrome * chrome);

    MWContext * makeGridWindow(void *hist_list, void *history,
	int x, int y, int width, int height,
	char *url_str, char *window_name, int scrolling,
	NET_ReloadMethod force_reload, bool no_edge);

    void clearDNSSelect(int socket);

    void setReadSelect(int fd);

    void clearReadSelect(int fd);

    void setConnectSelect(int fd);

    void clearConnectSelect(int fd);

    void setFileReadSelect(int fd);

    void clearFileReadSelect(int fd);

    char*  getTempFileFor(const char* fname,
			   XP_FileType ftype, XP_FileType* rettype);

    virtual int getURL( URL_Struct *url );

    // Below are used by the Qt FE, not by the FE_* functions.

    // return a QFont* for a given Netscape TextStruct.
    // This will use a cached result if possible
    QFont getFont(LO_TextAttr *text);

    virtual void perror( const char* message );

    virtual void urlDone(URL_Struct *, int) { }

    virtual QWidget* contentWidget() const { return 0; }
    virtual QWidget *topLevelWidget() const { return 0; }

    virtual int documentWidth() const {return 0; }
    virtual int documentHeight() const {return 0; }
    virtual void documentSetContentsPos( int, int ) {}
    virtual int scrollWidth() const { return 0; }
    virtual int scrollHeight() const { return 0; } 
    virtual int visibleWidth() const {return 0; }
    virtual int visibleHeight() const {return 0; }

    void updateRect(int x, int y, int w, int h);

    MWContext* mwContext() const { return context; }

    void	setTransparentPixel( const QColor & );

    virtual int documentXOffset() const;
    virtual int documentYOffset() const;

    void clearPainter();
    QPainter* painter() const
	{ if (internal_painter) return internal_painter;
	    else return forcePainter(); }


    void setDrawableOrigin(int  x_origin, int yorigin);
    int getXOrigin() { return internal_x_origin; }
    int getYOrigin() { return internal_y_origin; }

    bool isGridParent( MWContext* ) const;
    int getSynchronousURL ( QWidget* widget_to_grab,
			    const char *title,
			    URL_Struct *url,
			    int output_format,
			    void *call_data );
    int getSecondaryURL( URL_Struct *url_struct,
			 int output_format,
			 void *call_data, bool skip_get_url );
    void saveURL ( URL_Struct *url );

  DialogPool* dialogPool() const { return dialogpool; }

public slots:
    void cmdQuit();

signals:
    void messengerMessage(const char*, int);
    void messengerMessageClear();
    void progressStarting( int );
    void progressMade( int );
    void progressMade();
    void progressFinished();
    void progressReport( const char * );
    void canStop( bool );
    void urlChanged( const char * );

 protected:
    QtContext( MWContext* );
    virtual QPainter* makePainter();

    // re-call document[XY]Offset and
    void adjustCompositorSize();
    void adjustCompositorPosition();

    // access the context-specific variable via this pointer
    DialogPool* dialogpool;

    virtual void setupToolbarSignals(){};
    int bw;
    int scrolling_policy;   //  LO_SCROLL_YES, ..NO, .. AUTO, ... NEVER

 private:
    QPainter* forcePainter() const;
    QPainter* internal_painter;

    // for drawing.cpp ---------------------

    QColorGroup colorGroup(const QColor&);

    void displayTableBorder(LO_TableStruct *ts, QRect r, int bw);

    int internal_x_origin;
    int internal_y_origin;
    // end drawing.cpp ---------------------

    CL_Compositor* createCompositor(MWContext* context);
    MWContext* context;

public:
    // stuff for the threading or whatever
    int dont_free_context_memory;
    QWidget* synchronous_url_dialog;
    int synchronous_url_exit_status;
    unsigned char delete_response;
    bool destroyed;
    bool looping_images_p;     /* TRUE if images are looping. */
    int active_url_count;
    bool clicking_blocked;
    bool have_tried_progress;

    fe_FindData find_data;
};



class QtMailContext : public QtContext {
/* A mail reader window */
public:
    QtMailContext(MWContext* cx) : QtContext(cx) { }
};

class QtNewsContext : public QtContext {
/* A news reader window */
public:
    QtNewsContext(MWContext* cx) : QtContext(cx) { }
};

class QtMailMsgContext : public QtContext {
/* A window to display a mail msg */
public:
    QtMailMsgContext(MWContext* cx) : QtContext(cx) { }
};

class QtNewsMsgContext : public QtContext {
/* A window to display a news msg */
public:
    QtNewsMsgContext(MWContext* cx) : QtContext(cx) { }
};

class QtMessageCompositionContext : public QtContext {
/* A news-or-mail message editing window */
public:
    QtMessageCompositionContext(MWContext* cx) : QtContext(cx) { }
};

class QtSaveToDiskContext : public QtContext {
/* The placeholder window for a download */
public:
    QtSaveToDiskContext(MWContext* cx) : QtContext(cx) { }
};

class QtTextContext : public QtContext {
/* non-window context for text conversion */
public:
    QtTextContext(MWContext* cx) : QtContext(cx) { }
};

class QtPostScriptContext : public QtContext {
/* non-window context for PS conversion */
public:
    QtPostScriptContext(MWContext* cx) : QtContext(cx) { }
};

class QtBiffContext : public QtContext {
/* non-window context for background mail notification */
public:
    QtBiffContext(MWContext* cx) : QtContext(cx) { }
};

class QtJavaContext : public QtContext {
/* non-window context for Java */
public:
    QtJavaContext(MWContext* cx) : QtContext(cx) { }
};

class QtAddressBookContext : public QtContext {
/* Context for the addressbook */
public:
    QtAddressBookContext(MWContext* cx) : QtContext(cx) { }
};

class QtOleNetworkContext : public QtContext {
/* non-window context for the OLE network1 object */
public:
    QtOleNetworkContext(MWContext* cx) : QtContext(cx) { }
};

class QtPrintContext : public QtContext {
/* non-window context for printing */
public:
    QtPrintContext(MWContext* cx) : QtContext(cx) { }
};

class QtDialogContext : public QtContext {
/* non-browsing dialogs. view-source/security */
public:
    QtDialogContext(MWContext* cx) : QtContext(cx) { }
};

class QtMetaFileContext : public QtContext {
/* non-window context for Windows metafile support */
public:
    QtMetaFileContext(MWContext* cx) : QtContext(cx) { }
};

class QtEditorContext : public QtContext {
/* An Editor Window */
public:
    QtEditorContext(MWContext* cx) : QtContext(cx) { }
};

class QtSearchContext : public QtContext {
/* a window for modeless search dialog */
public:
    QtSearchContext(MWContext* cx) : QtContext(cx) { }
};

class QtSearchLdapContext : public QtContext {
/* a window for modeless LDAP search dialog */
public:
    QtSearchLdapContext(MWContext* cx) : QtContext(cx) { }
};

class QtHTMLHelpContext : public QtContext {
/* HTML Help context to load map files */
public:
    QtHTMLHelpContext(MWContext* cx) : QtContext(cx) { }
};

class QtMailFiltersContext : public QtContext {
/* Mail filters context */
public:
    QtMailFiltersContext(MWContext* cx) : QtContext(cx) { }
};


class QtMailNewsProgressContext : public QtContext {
/* a progress pane for mail/news URLs */
public:
    QtMailNewsProgressContext(MWContext* cx) : QtContext(cx) { }
};

class QtPaneContext : public QtContext {
/* Misc browser pane/window in weird parts of the UI, eg. navigation center */
public:
    QtPaneContext(MWContext* cx) : QtContext(cx) { }
};

class QtRDFSlaveContext : public QtContext {
/* Slave context for RDF network loads */
public:
    QtRDFSlaveContext(MWContext* cx) : QtContext(cx) { }
};

class QtProgressModuleContext : public QtContext {
/* Progress module (PW_ functions) */
public:
    QtProgressModuleContext(MWContext* cx) : QtContext(cx) { }
};

class QtIconContext : public QtContext {
/* Context for loading images as icons */
public:
    QtIconContext(MWContext* cx) : QtContext(cx) { }
};

extern "C" {
  void url_exit(URL_Struct *url, int status, MWContext *context);
  void imageGroupObserver(XP_Observable observable, XP_ObservableMsg message,
			  void *message_data, void *closure);

}
#endif
