//	============================================================================
//	¥¥¥	QAP_Assist.h
//	============================================================================
//		QA Partner/Macintosh Driver Assistance Hook Header 
//		
//		Copyright © 1993-1997 Segue Software, Inc.
//		All Rights Reserved.
//		
//		QA PARTNER RELEASE VERSION 4.0 BETA 1
//		THIS IS A BETA RELEASE.  THIS SOFTWARE MAY HAVE BUGS.  THIS SOFTWARE MAY CHANGE BEFORE FINAL RELEASE.




#ifndef _MAC	//¥NETSCAPE: added these lines
#define _MAC
#include <LTableView.h>
#endif

#ifndef __QAP_ASSIST_H__
#define __QAP_ASSIST_H__

#include <LTableView.h>


//¥NETSCAPE: added this definition
//#define QAP_BUILD

// The internal name of the QA Partner 4.0 driver
#define QAP_DRIVER_NAME     "\p.QAP40"

// The PBControl code to send the driver to set the assist hook
#define QAP_SET_ASSIST_HOOK 200

// The return value you can use if you're not ready to assist the driver
#define QAP_CALL_ME_BACK    -1

// Special "index" values for kQAPGetListContents
#define QAP_INDEX_ALL       -1
#define QAP_INDEX_SELECTED  -2

// 	Prototype for the assist hook function which you must supply
#ifdef __cplusplus
extern "C" {
#endif
	pascal short QAP_AssistHook (short selector, long handle, void *buffer, short val, long l_appA5);
#ifdef __cplusplus
}
#endif

typedef pascal short (*QAPAssistHookPtr)(short, long, void *, short, long);

// Mixed-Mode junk for QAP_AssistHook
#if GENERATINGPOWERPC || defined(powerc) || defined(__powerc) || defined (_MPPC_)
	enum
	{
		uppQAPAssistHookInfo = kPascalStackBased
		| RESULT_SIZE(SIZE_CODE(sizeof(short)))
		| STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(short)))
		| STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(long)))
		| STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(void *)))
		| STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(short)))
		| STACK_ROUTINE_PARAMETER(5, SIZE_CODE(sizeof(long)))			
	};

	typedef UniversalProcPtr QAPAssistHookUPP;
	#define CallQAPAssistHook(userRoutine, selector, handle, buffer, val, appA5)		\
		CallUniversalProc((UniversalProcPtr)(userRoutine), uppQAPAssistHookInfo, (short)(selector), (long)(handle), (void *)(buffer), (short)(val), (long)(appA5))
	#define NewQAPAssistHook(userRoutine)		\
		(QAPAssistHookUPP) NewRoutineDescriptor((ProcPtr)(userRoutine), uppQAPAssistHookInfo, GetCurrentISA())
#else
	typedef QAPAssistHookPtr QAPAssistHookUPP;
	#define CallQAPAssistHook(userRoutine, selector, handle, buffer, val, appA5)		\
		(*(userRoutine))((short)(selector), (long)(handle), (void *)(buffer), (short)(val), (long)(appA5)) 
	#define NewQAPAssistHook(userRoutine)		\
		(QAPAssistHookUPP)(userRoutine)
#endif

// The selectors for the assist hook callback function
enum	
{
// 	selector                        handle          buffer                      val                     return
// 	--------                        ------          ---                         ---                     ------
	kQAPGetWindowContents,       // WindowPtr       wcinfo array ptr            max array elements      number of items returned
	kQAPGetTextInfo,             // from wcinfo     textinfo struct ptr         1                       0 if no error
	kQAPGetStaticText,           // from wcinfo     buffer ptr (Str255)         buffer size             0 if no erro
	kQAPGetListInfo,             // from wcinfo     listinfo struct ptr         1                       0 if no erro
	kQAPGetListContents,         // from wcinfo     buffer ptr                  index                   0 if no error
	kQAPGetCustomItemName,       // from wcinfo		buffer ptr (Str255)         0                       0 if no error
	kQAPGetCustomItemValue,		 // from wcinfo		buffer ptr (long *) 		0
	kQAPAppToForeground,         // NULL            NULL                        0                       0 if no error
	kQAPAppToBackground,         // NULL            NULL                        0                       0 if no error
	kQAPGetScrollbarInfo,
	kQAPWin32Service			 // HWND			ptr to paramblock			win32 Service Selector
 };
 
 // Selectors for win32 services
 
enum
{
	kQAP_CB_GetContents,
	kQAP_CB_GetAttr,
	kQAP_CB_GetInfo,
	kQAP_CB_GetItemText,
	kQAP_CB_GetText,
	kQAP_CB_IsSelected,
	kQAP_CB_SelectText,
	
	kQAP_LB_GetContents,
	kQAP_LB_GetAtPoint,
	kQAP_LB_GetAttr,
	kQAP_LB_GetInfo,
	kQAP_LB_GetItemInfo,
	kQAP_LB_GetItemText,
	kQAP_LB_IsSelected,

	kQAP_LV_GetContents,
	kQAP_LV_GetAttr,
	kQAP_LV_GetInfo,
	kQAP_LV_GetItemInfo,
	kQAP_LV_GetItemText,
	kQAP_LV_IsSelected,
	kQAP_LV_MakeVisible,
	
	kQAP_PGL_GetContents,
	kQAP_PGL_GetInfo,
	kQAP_PGL_GetItemRect,
	kQAP_PGL_GetItemText,
	kQAP_PGL_IsSelected,

	kQAP_SCL_GetLayout,
	kQAP_SCL_GetValues,
	
	kQAP_TV_GetContents,
	kQAP_TV_GetAtPoint,
	kQAP_TV_GetAttr,
	kQAP_TV_GetInfo,
	kQAP_TV_GetItemInfo,
	kQAP_TV_GetItemText,
	kQAP_TV_IsSelected,
	
	kQAP_UD_GetLayout,
	kQAP_UD_GetValues
};
 
enum	
{
	// Window type codes for the WCINFO 'type' field
	WT_COMPLETE = 0,	// marker for end of window item list (QAP will not add other items)
	WT_INCOMPLETE,		// marker for end of assist item list (QAP will add other items too)
	WT_CONTROL,			// Apple Control Manager control
	WT_TEXT_FIELD,		// Apple TextEdit Manager Text Field
	WT_LIST_BOX,		// Apple List Manager List Box
	WT_ASSIST_ITEM,		// Custom control (type indicated in 'cls' field)
	WT_IGNORE,			// Internal use
	
	// Window class codes for the WCINFO 'cls' field (only used for WT_ASSIST_ITEM items)
	
	WC_STATIC_TEXT = 256,
	WC_TEXT_FIELD,
	WC_LIST_BOX,
	WC_POPUP_LIST,
	WC_OWNER_DRAW,
	WC_ICON,
	WC_PICTURE,
	WC_CUSTOM,
	WC_PUSH_BUTTON,
	WC_CHECK_BOX,
	WC_RADIO_BUTTON,
	WC_COMBO_BOX,
	WC_LIST_VIEW,
	WC_NULL,
	WC_PAGE_LIST,
	WC_TRACK_BAR,
	WC_TREE_VIEW,
	WC_UP_DOWN,
	WC_SCROLL_BAR,
	WC_WINDOW			// Internal use
};
 
// Values for the 'flags' field of wcinfo 
#define WCF_DISABLED	0x01
#define WCF_NOCONTROL	0x02

 
#if PRAGMA_ALIGN_SUPPORTED
#   pragma options align = power
#endif
 
// The structure to be filled in for each control when the assist hook
// is called with the kQAPGetWindowContents selector

#define MAC_NAME_SIZE	32

typedef struct wcinfo
{
	Handle handle;
	Rect rect;
	short type;
	short cls;
	short flags;
	short pad;		
	char str[MAC_NAME_SIZE];
} WCINFO, *PWCINFO, **PPWCINFO;

// The structure to be filled for the given item when the assist hook
// is called with the kQAPGetTextInfo selector

typedef struct textinfo
{
	Handle handle;        // handle to the text if you want me to unlock it
	TEHandle hTE;         // handle to the TE associated with the text (for dialog items)
	Ptr ptr;              // pointer to the actual text (locked if a handle, please)
	short len;            // length of the actual text
	Boolean hasFocus;     // TRUE if typing right now would go to the text field
	char state;           // value for HSetState if <handle> is not NULL
	char buf[256];  	  // buffer to copy text into if you need a fixed place
} TEXTINFO, *PTEXTINFO, **PPTEXTINFO;

// The structure to be filled for the given item when the assist hook
// is called with the kQAPGetListInfo selector

typedef struct listinfo
{
	short itemCount;       // total number of items in the listbox
	short topIndex;        // 1-based index of top visible item in list
	short itemHeight;      // height of each list item, in pixels
	short visibleCount;    // count of visible list items
	ControlHandle vScroll; // ListBox's vertical scrollbar, or NULL
	Boolean isMultiSel;    // TRUE if cmd-click selects multiple items
	Boolean isExtendSel;   // TRUE if shift-click extends the selection range
	Boolean hasText;       // TRUE if the list items are textual
	Boolean reserved;       
} QAPLISTINFO, *PQAPLISTINFO, **PPQAPLISTINFO;

typedef struct scrollbarinfo
{
    long lPos;
    long lMin;
    long lMax;
    long lPageSize;
    long lIncrement;
    long lVertical;
    long lHasThumb;
    Point ptIncrArrow;
    Point ptDecrArrow;
    Point ptIncrPage;
    Point ptDecrPage;
    Point ptMinThumb;
    Point ptMaxThumb;	
} QAPSCROLLBARINFO, *PQAPSCROLLBARINFO, **PPQAPSCROLLBARINFO;

#if PRAGMA_ALIGN_SUPPORTED
#   pragma options align = reset
#endif

//¥NETSCAPE --- begin
class CQAPartnerTableMixin
{
public:
					CQAPartnerTableMixin(LTableView *);
			virtual	~CQAPartnerTableMixin();

	virtual void	QapGetListInfo (PQAPLISTINFO pInfo) = 0;
	virtual Ptr		QapAddCellToBuf(Ptr pBuf, Ptr pLimit, const STableCell& sTblCell) = 0;
	virtual short	QapGetListContents(Ptr pBuf, short index);

protected:
	LTableView *	mTableView;
};
//¥NETSCAPE --- end

#endif // __QAP_ASSIST_H__