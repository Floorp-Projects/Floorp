/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


/*
 * This file is included by client.h
 *
 * It can be included by hand after mcom.h
 *
 * All intermodule data structures (i.e. MWContext, etc) should be included
 *  in this file
 */
#ifndef _STRUCTS_H_
#define _STRUCTS_H_

#include "ntypes.h"
#include "xp_mcom.h"

#include "il_types.h"
#include "lo_ele.h"
#include "shistele.h"
#include "edttypes.h"

#ifdef JAVA
#include "prlong.h"
#include "prclist.h"
#endif /* JAVA */

/* ------------------------------------------------------------------------ */
/* ============= Typedefs for the global context structure ================ */

/* will come out of the ctxtfunc.h file eventually
 */
typedef struct _ContextFuncs ContextFuncs;
/*
 *   This stuff is front end specific.  Add whatever you need.
 */
#if defined(OSF1) && defined(__cplusplus)
    struct fe_ContextData;
#endif

#if defined(XP_MAC) && defined(__cplusplus)
class NetscapeContext;
class CHyperView;

class CNSContext;
class CHTMLView;
#endif

#if ( defined(XP_WIN) || defined(XP_OS2) ) && defined(__cplusplus)
class CAbstractCX;
class CEditView;
class CSaveProgress;
#endif

typedef struct FEstruct_ {
#ifndef MOZILLA_CLIENT
	void * generic_data;
#elif defined(XP_WIN) || defined(XP_OS2)
#ifdef __cplusplus
    	CAbstractCX *cx;
#else
	void *cx;
#endif
#elif defined(XP_TEXT)
	int doc_cols;
	int doc_lines;
	int cur_top_line;
	int num_anchors;
	int cur_anchor;

#elif defined(XP_UNIX)
    struct fe_ContextData *data;
#elif defined(XP_MAC)
#ifdef __cplusplus
    class NetscapeContext*		realContext;
    class CHyperView*		view;
    
    class CNSContext*		newContext;
    class CHTMLView*		newView;
#else
	void*					realContext;
	void*					view;

	void*					newContext;
	void*					newView;	
#endif

/*
** These members are only used by the EDITOR... However, if they
** are removed for non-editor builds the MWContext structure
** becomes skewed for java (and the rest of DIST)...
*/
#ifdef __cplusplus
    class CEditView*		editview;
    class CSaveProgress*	savedialog;
#else
	void*					editview;
	void*					savedialog;
#endif

#endif
#ifdef MOZILLA_CLIENT
  void*  webWidget; /* Really a nsIWebWidget */
#endif
} FEstruct;

#define FEUNITS_X(x,context)   ((int32) ((MWContext *)context)->convertPixX * (x))
#define FEUNITS_Y(y,context)   ((int32) ((MWContext *)context)->convertPixY * (y))

struct MessageCopyInfo;

/*
    This is a generic context structure.  There should be one context
    per document window.  The context will allow assorted modules to
    pull out things like the URL of the current document as well as
    giving them a place to hand their window specific stuff.

*/

#if defined (OSF1)
/* Forward declaration to make compiler happy on OSF1 */
struct MSG_SearchFrame;
#ifdef XP_CPLUSPLUS
class MSG_IMAPFolderInfoMail;
class MSG_Master;
class MSG_Pane;
class TImapServerState; 
#endif
#endif


#if defined(__cplusplus)
class nsITransferListener;
#endif

struct MWContext_ {
    MWContextType type;

    char       *url;   /* URL of current document */
    char       * name;	/* name of this context */
    History      hist;  /* Info needed for the window history module */
    FEstruct     fe;    /* Front end specific stuff */
    PRPackedBool          fancyFTP;  /* use fancy ftp ? */
    PRPackedBool          fancyNews; /* use fancy news ? */
    PRPackedBool          intrupt;   /* the user just interrupted things */
    PRPackedBool          graphProgress; /* should the user get visual feedback */
    PRPackedBool          waitingMode;  /* Has a transfer been initiated?  Once a */
                 /* transfer is started, the user cannot select another */
                 /* anchor until either the transfer is aborted, has */
                 /* started to layout, or has been recognized as a */
                 /* separate document handled through an external stream/viewer */
    PRPackedBool          reSize;   /* the user wants to resize the window once the */
                 /* current transfer is over */
    int          fileSort;  /* file sorting method */
    char       * save_as_name;	/* name to save current file as */
    char       * title;		/* title (if supplied) of current document */
    Bool       is_grid_cell;	/* Is this a grid cell */
    struct MWContext_ *grid_parent;	/* pointer to parent of grid cell */
    XP_List *	 grid_children;	/* grid children of this context */
    int      convertPixX;	/* convert from pixels to fe-specific coords */
    int      convertPixY;	/* convert from pixels to fe-specific coords */
    ContextFuncs * funcs;       /* function table of display routines */
    PrintSetup	*prSetup;	/* Info about print job */
    PrintInfo	*prInfo;	/* State information for printing process */

    /* XXXM12N Stuff for the new, modular Image Library. *********************/
    IL_GroupContext *img_cx;    /* Created by Front Ends.  Passed into Image
                                   Library function calls.  */
    IL_ColorSpace *color_space;  /* Colorspace information for images.  This
                                    should become a part of the FE's display
                                    context when MWContext goes away. */
    IL_IRGB *transparent_pixel; /* Background color to be passed into
                                   IL_GetImage.  Set by Front Ends (?) */
    /*************************************************************************/

	int32		images;			/* # of distinct images on this page */

	/* ! do not use these !  */
	/* ! these are going away soon ! */
	/* instead see intl_csi.h for the i18n accessor functions */
	int16		do_not_use_win_csid;	/* code set ID of current window	*/
	int16		do_not_use_doc_csid;	/* code set ID of current document	*/
	int16		do_not_use_relayout;	/* tell conversion to treat relayout case */
	char		*do_not_use_mime_charset;	/* MIME charset from URL			*/

    struct MSG_CompositionFrame *msg_cframe; /* ditto. */
	struct MSG_SearchFrame *msg_searchFrame; /*state for search, for search contexts*/
	struct MSG_BiffFrame *biff_frame; /* Biff info for this context, if any. */
    struct BM_Frame *bmframe; /* Bookmarks info for this context, if any. */
    
    /* for now, add IMAP mail stuff here */
#ifdef XP_CPLUSPLUS
	class MSG_IMAPFolderInfoMail  *currentIMAPfolder;
	class MSG_Pane				  *imapURLPane;		/* used when updated folders */
    class MSG_Master              *mailMaster;
    class TNavigatorImapConnection   *imapConnection; 
#else
	struct MSG_IMAPFolderInfoMail *currentIMAPfolder;
	struct MSG_Pane				  *imapURLPane;		/* used when updated folders */
    struct MSG_Master	 		  *mailMaster;
    struct TNavigatorImapConnection *imapConnection; 
#endif    

	/* for now, add message copy info stuff here */
	struct MessageCopyInfo		  *msgCopyInfo;

	NPEmbeddedApp *pluginList;	/* plugins on this page */
	void *pluginReconnect; /* yet another full screen hack */

    struct MimeDisplayData *mime_data;	/* opaque data used by MIME message
										   parser (not Mail/News specific,
										   since MIME objects can show up
										   in Browser windows too.) */
										/* Also overloaded by progress module to hold private crap! */


    struct JSContext *mocha_context;	/* opaque handle to Mocha state */
    uint32  event_bit;			/* sum of all event capturing objects */
    XP_Bool js_drag_enabled;		/* indicates JS drag enabled */
    int8 js_dragging;			/* indicates which button has JS drag in process */
    XP_List * js_dependent_list;	/* lifetime-linked children of this context */
    MWContext *js_parent;
    int32 js_timeouts_pending;          /* Number of pending JavaScript timeouts */


    XP_Bool restricted_target;			/* TRUE if window is off-limits for opening links into 
						from mail or other window-grabbing functions.*/

	NPEmbeddedApp *pEmbeddedApp; /* yet another full screen hack */
	char * defaultStatus;		/* What string to show in the status area
								   whenever there's nothing better to show.
								   FE's should implement FE_Progress so that
								   if it is passed NULL or "" it will instead
								   display this string.  libmsg changes this
								   string all the time for mail and news
								   contexts. */
#if 0
	CL_Compositor *compositor;  /* The compositor associated with this context */
	XP_Bool blink_hidden;       /* State of blink layers */
	void *blink_timeout;        /* Timeout used for blink hiding/unhiding */
#endif /* LAYERS */

	int32      doc_id;			/* unique identifier for generated documents */
	int32      context_id;			/* unique identifier for context */

  	void       *pHelpInfo;		/* pointer to additional help information;
  								   see ns/lib/libnet/mkhelp.c [EA] */
	
#ifdef JAVA
	/* See ns/sun-java/netscape/net/netStubs.c for the next 2 items: */

	/*
    ** This mysterious list is used in two ways: (1) If you're a real
    ** window context, it's a list of all dummy java contexts that were
    ** created for java's network connections. (2) If you're a dummy java
    ** context, it's where you're linked into the list of connections for
    ** the real context:
    */
	PRCList		javaContexts;

	/*
    ** Second, if you're a dummy java context, you'll need a pointer to
    ** the stream data so that you can shut down the netlib connection:
    */
	struct nsn_JavaStreamData*	javaContextStreamData;

	/*
    ** Stuff for GraphProgress. See lj_embed.c
    */
	Bool		displayingMeteors;
	int64		timeOfFirstMeteorShower;
	int16		numberOfSimultaneousShowers;

#endif /* JAVA */

	/*
    ** Put the editor stuff at the end so that people can still use the
    ** the Java DLL from the 2.0 dist build with Navigator Gold.
    */
    PRPackedBool is_editor;       /* differentiates between Editor and Browser modes */
    PRPackedBool is_new_document; /* quick access to new doc state (unsaved-no file yet)*/
    PRPackedBool display_paragraph_marks; /* True if we should display paragraph and hard-return symbols. */
    PRPackedBool display_table_borders; /* True if we should display dotted lines around tables with invisible borders. */
    PRPackedBool edit_view_source_hack;
    PRPackedBool edit_loading_url;  /* Hack to let us run the net while in a modal dialog */
    PRPackedBool edit_saving_url;   /*  " */
	PRPackedBool edit_has_backup;   /* Editor has made a session backup */
	PRPackedBool bIsComposeWindow;  /* Editor is a compose window */

	/*
	 * Webfonts that were loaded by this context
	 */
	void *webfontsList;

	/* web font stuff */
	/* On Windows,  they are initialized to 0 in cxdc.cpp	*/
	int16		WebFontDownLoadCount;	/* # of download for this doc( one download can have multiple fonts) */
	int16		MissedFontFace;			/* have we missed any font? */

	/*  number of pixels per point-size */
	double      XpixelsPerPoint;
	double		YpixelsPerPoint;
    
    Bool bJavaScriptCalling;
	/* Allow JavaScript in certain internally generated contexts even
	 * when the "enable javascript" pref is turned off.
	 * This flag will also be inherited by child grid cells.
	 */
	PRPackedBool forceJSEnabled;

	/* For increase and decrease font */
	double		fontScalingPercentage;

    int INTL_tag;     /* used to tell that we have a valid INTL_CSIInfo */
    INTL_CharSetInfo INTL_CSIInfo; /* new home of private i18n data */

    /* the current tab focused data */
    LO_TabFocusData *tab_focus_data;

    void *ncast_channel_context;
  /* if the window is displaying an XML file, keep a pointer to the XML file structure here */
    void*   xmlfile;
    Bool anonymous;
    URL_Struct*   modular_data;
    PRInt32       ref_count;

    /* This gets set to `true' when layout encounters an image with no
       width or height: layout will proceed to place the image, but
       the FE will have to do a reflow once all the netlib connections
       terminate for the page to be correctly displayed. The idea is
       to get visible content to the user ASAP, even if it means that
       stuff looks funny for a couple seconds. */
    PRPackedBool  requires_reflow;

#if defined(__cplusplus)
    nsITransferListener* progressManager;
#else
    void* progressManager;
#endif /* __cplusplus */
};


/* This tells libmime.a whether it has the mime_data slot in MWContext
   (which should always be true eventually, but having this #define here
   makes life easier for me today.)  -- jwz */
#define HAVE_MIME_DATA_SLOT


/* this is avialible even in non GOLD builds. */
#define EDT_IS_EDITOR(context)       (context != NULL && context->is_editor)
#define EDT_DISPLAY_PARAGRAPH_MARKS(context) (context && context->is_editor && context->display_paragraph_marks)
#define EDT_DISPLAY_TABLE_BORDERS(context) (context && context->is_editor && context->display_table_borders)
#define EDT_RELAYOUT_FLAG (0x2)
#define EDT_IN_RELAYOUT(context)       (context != NULL && ((context->is_editor & EDT_RELAYOUT_FLAG) != 0))

#ifdef JAVA

/*
** This macro is used to recover the MWContext* from the javaContexts
** list pointer:
*/
#define MWCONTEXT_PTR(context) \
    ((MWContext*) ((char*) (context) - offsetof(MWContext,javaContexts)))

#endif /* JAVA */

/* ------------------------------------------------------------------------ */
/* ====================== NASTY UGLY SHORT TERM HACKS ===================== */

#define XP_CONTEXTID(ctxt)		((ctxt)->context_id)
#define XP_DOCID(ctxt)			((ctxt)->doc_id)
#define XP_SET_DOCID(ctxt, id)	((ctxt)->doc_id = (id))

/* ------------------------------------------------------------------------ */
/* ============= Typedefs for the parser module structures ================ */

/*
 * I *think* (but am unsure) that these should be forked off into a
 *   parser specific client level include file
 *
 */

typedef int8 TagType;

struct PA_Tag_struct {
    TagType type;
    PRPackedBool is_end;
    uint16 newline_count;
#if defined(XP_WIN) || defined(XP_OS2)
    union {                 /* use an anonymous union for debugging purposes*/
    	PA_Block data;
        char* data_str;
    };
#else
    PA_Block data;
#endif
    int32 data_len;
    int32 true_len;
    void *lo_data;
    struct PA_Tag_struct *next;
    ED_Element *edit_element;
};

typedef struct _TagList {
	PA_Tag *tagList; 
    PA_Tag *lastTag; 
} TagList;

#define PA_HAS_PDATA( tag ) (tag->pVoid != 0 )

#ifdef XP_UNIX
typedef    char *PAAllocate (intn byte_cnt);
typedef    void  PAFree (char *ptr);
#else
typedef    void *PAAllocate (unsigned int byte_cnt);
typedef    void  PAFree (void *ptr);
#endif
typedef    intn  PA_OutputFunction (void *data_object, PA_Tag *tags, intn status);

struct _PA_Functions {
    PAAllocate  *mem_allocate;
    PAFree      *mem_free;
    PA_OutputFunction   *PA_ParsedTag;
};

typedef struct PA_InitData_struct {
    PA_OutputFunction   *output_func;
} PA_InitData;

/* Structure that defines the characteristics of a new window.
 * Each entry should be structured so that 0 should be the
 * default normal value.  Currently all 0 values
 * bring up a chromeless MWContextBrowser type window of
 * arbitrary size.
 */
struct _Chrome {
    MWContextType type;			/* Must be set to the correct type you want,
								 * if doesn't exist, define one!!!
								 */
	Bool show_button_bar;        /* TRUE to display button bar */
	Bool show_url_bar;           /* TRUE to show URL entry area */
 	Bool show_directory_buttons; /* TRUE to show directory buttons */
	Bool show_bottom_status_bar; /* TRUE to show bottom status bar */
	Bool show_menu;				 /* TRUE to show menu bar */
	Bool show_security_bar;		 /* TRUE to show security bar */
	Bool hide_title_bar;		 /* TRUE to hide title bar and window controls */
	int32 w_hint, h_hint;		 /* hints for width and height */
	int32 outw_hint, outh_hint;	 /* hints for outer window width and height */
	int32 l_hint, t_hint;		 /* hints for left and top window positioning */
	Bool topmost;				 /* TRUE for window alwaysOnTop */
	Bool bottommost;			 /* TRUE for 'desktop' window */
	Bool z_lock;				 /* TRUE for window which cannot move within z-order */
	Bool is_modal;				 /* TRUE to make window be modal */
	Bool show_scrollbar;		 /* TRUE to show scrollbars on window */
	Bool location_is_chrome;	 /* TRUE if top or left is specified */
	Bool allow_resize;			 /* TRUE to allow resize of windows */
	Bool allow_close;			 /* TRUE to allow window to be closed */
	Bool copy_history;			 /* TRUE to copy history of prototype context*/
	Bool dependent;				 /* TRUE if this window is to be closed with its parent*/
	Bool disable_commands;			/* TRUE if user has set hot-keys / menus off */
	Bool restricted_target;			/* TRUE if window is off-limits for opening links into 
						    from mail or other window-grabbing functions.*/
	void (* close_callback)(void *close_arg); /* called on window close */
	void *close_arg;			 /* passed to close_callback */
};


#endif /* _STRUCTS_H_ */
