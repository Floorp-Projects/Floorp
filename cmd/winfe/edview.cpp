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

// edview.cpp : implementation of the CNetscapeEditView class
//
// Primary Command handling for Editor window
//
// Created 9/11/95 by CLM
//

#include "stdafx.h"
#ifdef EDITOR
#include "edview.h"
#include "edprops.h"
#include "edt.h"
#include "mainfrm.h"
#include "edframe.h"
#include "edres2.h"
#include "property.h"
#include "edttypes.h"
#include "edtable.h"
#include "prefapi.h"
#include "libi18n.h"
#include "intl_csi.h"
#include "compfrm.h"
#include "edtclass.h"
#include "abdefn.h"
 
#ifdef _IME_COMPOSITION
#define CLEARBIT(A, N)	A&=~N
#define SETBIT(A, N)	A|=N

    #ifdef XP_WIN16
        #include "ime16.h"
    #else
        #include "intlwin.h"
    #endif //XP_WIN16 else XP_WIN32
#endif
#ifdef _DEBUG
#ifdef XP_WIN16
#if 0
char * g_buffers[1000];
int g_numbuffers=0;

void
addToBuffer(const char *p_str)
{
    g_buffers[g_numbuffers++]=XP_STRDUP(p_str);
}

void addToBufferLONG(LONG p_int)
{
    char t_def[255];
    ltoa(p_int,t_def,10);
    g_buffers[g_numbuffers++]=(char *)XP_STRDUP(t_def);
    
}

void
addToBuffer(LPARAM p_result)
{
    const char *t_char;
    switch (p_result)
    {
    case IME_RS_DISKERROR: t_char="IME_RS_DISK_ERROR";break;
    case IME_RS_ERROR: t_char="IME_RS_ERRPR";break;
    case IME_RS_ILLEGAL: t_char="IME_RS_ILLEGAL";break;
    case IME_RS_INVALID: t_char="IME_RS_INVALID";break;
    case IME_RS_NEST: t_char="IME_RS_NEST";break;
    case IME_RS_NOIME: t_char="IME_RS_NOIME";break;
    case IME_RS_NOROOM: t_char="IME_RS_NOROOM";break;
    case IME_RS_NOTFOUND: t_char="IME_RS_NOTFOUND";break;
    case IME_RS_SYSTEMMODAL: t_char="IME_RS_SYSTEMMODAL";break;
    case IME_RS_TOOLONG: t_char="IME_RS_TOOLONG";break;
    case 0:t_char="SUCCESS";break;
    default:
        {
            return;
        }
    }
    g_buffers[g_numbuffers++]=(char *)t_char;
}
#endif //0
#endif //XP_WIN16
#endif //_DEBUG

#ifdef XP_WIN32
#include "shlobj.h"
#endif

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif


// Index to start of TrueType fonts in a menu
#define ID_FORMAT_TRUETYPE_BASE ID_FORMAT_FONTFACE_BASE+2


#ifdef _IMAGE_CONVERT
#include "libcnv.h"
#include "jinclude.h"
#include "jpeglib.h"

UINT CNetscapeEditView::m_converrmsg[NUM_CONVERR]={
    IDS_CONVERR0,//"Canceled",
    IDS_CONVERR1,//"Ok",
    IDS_CONVERR2,//"Invalid Source",
    IDS_CONVERR3,//"Invalid Destination",
    IDS_CONVERR4,//"Invalid File Header",
    IDS_CONVERR5,//"Invalid Image Header",
    IDS_CONVERR6,//"Invalid Bit Depth",
    IDS_CONVERR7,//"Bad Read",
    IDS_CONVERR8,//"Out of Memory",
    IDS_CONVERR9,//"JPEG Creation Error",
    IDS_CONVERR10,//"JPEG Compression Error",
    IDS_CONVERR11,//"Bad Number of Bit Planes in Source",
    IDS_CONVERR12,//"Writing to file failed",
    IDS_CONVERR13,//"Invalid parameters to convert",
    IDS_CONVERR14,//"Unknown Error"
};



struct PluginHookStruct //this is used for the callbacks from plugins you must
{                       //free the outputimagecontext yourself
    MWContext *m_pMWContext;
    CONVERT_IMGCONTEXT *m_outputimagecontext;
};

void FE_ImageConvertPluginCallback(EDT_ImageEncoderStatus status, void* hook);

#define DEFAULT_IMG_CONVERT_QUALITY 75
#define EDTMAXCOLORVALUE 256


#endif //_IMAGE_CONVERT

// editor plugin info. this is the structure of the array elements stored in m_pPluginInfo.
typedef struct _PluginInfo
{
    uint32  CategoryId;
    uint32  PluginId;
}   PluginInfo;

// implemented in edframe.cpp.  I changed this to be WFE_FindMenu in winproto.h SP 4/3/97
//int FindMenu(CMenu *pMenu, CString& menuItemName);

// We do this alot
#define GET_MWCONTEXT  (GetContext() == NULL ? NULL : GetContext()->GetContext())

// This is TRUE only if we have a context and are not an Editor or 
//   we are an editor that has a good buffer and is not blocked 
#define CAN_INTERACT  (GetContext() != NULL && GetContext()->GetContext() != NULL \
                       && !GetContext()->GetContext()->waitingMode \
                       && (!EDT_IS_EDITOR(GetContext()->GetContext()) \
                           || (EDT_HaveEditBuffer(GetContext()->GetContext()) && !EDT_IsBlocked(GetContext()->GetContext()))))

extern char *EDT_NEW_DOC_URL;
extern char *EDT_NEW_DOC_NAME;

// check if a given menu id is one of the character encoding menus
BOOL IsEncodingMenu(UINT nID)
{
    return (nID >= ID_OPTIONS_ENCODING_1 && nID <= ID_OPTIONS_ENCODING_70 );
}


/////////////////////////////////////////////////////////////////////////////
// CNetscapeEditView

#undef new
IMPLEMENT_DYNCREATE(CNetscapeEditView, CNetscapeView)
#define new DEBUG_NEW

// These were initially in CEditFrame:
BEGIN_MESSAGE_MAP(CNetscapeEditView, CNetscapeView)
    //{{AFX_MSG_MAP(CNetscapeEditView)
    ON_WM_SETFOCUS()
    ON_WM_KILLFOCUS()
    ON_WM_CREATE()
    ON_WM_CLOSE()
    ON_WM_KEYDOWN()
    ON_WM_CHAR()
    ON_WM_TIMER()
	ON_WM_LBUTTONDBLCLK()
    ON_WM_RBUTTONDOWN()
    ON_WM_LBUTTONUP()
	ON_CBN_SELENDOK(ID_COMBO_PARA, OnSelendokParagraphCombo)
	ON_CBN_SELENDOK(ID_COMBO_FONTFACE, OnSelendokFontFaceCombo)
	ON_CBN_SELENDOK(ID_COMBO_FONTSIZE, OnSelendokFontSizeCombo)
    ON_CBN_SELENDCANCEL(ID_COMBO_PARA, OnCancelComboBox)
    ON_COMMAND(ID_OTHER_FONTFACE, OnSetLocalFontFace)
    ON_COMMAND(ID_REMOVE_FONTFACE, OnRemoveFontFace)
    ON_CBN_SELENDCANCEL(ID_COMBO_FONTFACE, OnCancelComboBox)
	ON_CBN_SELENDCANCEL(ID_COMBO_FONTSIZE, OnCancelComboBox)
	ON_COMMAND(ID_FORMAT_INCREASE_FONTSIZE, OnIncreaseFontSize)
	ON_COMMAND(ID_FORMAT_DECREASE_FONTSIZE, OnDecreaseFontSize)
    ON_COMMAND(ID_SETFOCUS_PARA_STYLE, OnSetFocusParagraphStyle)
	ON_COMMAND(ID_SETFOCUS_FONTFACE, OnSetFocusFontFace)
	ON_COMMAND(ID_SETFOCUS_FONTSIZE, OnSetFocusFontSize)
	ON_COMMAND(ID_GET_COLOR, OnGetFontColor)
    ON_COMMAND(ID_OPEN_NAV_WINDOW, OnOpenNavigatorWindow)
    ON_COMMAND(ID_EDT_FILE_SAVEAS, OnFileSaveAs)    // This replaces ID_NETSCAPE_SAVE_AS functionality
    ON_COMMAND(ID_EDT_FILE_SAVE, OnFileSave)        // This is missing from browser - we add it to the File Menu
    ON_COMMAND(ID_EDT_FINDREPLACE, OnEditFindReplace)
    ON_COMMAND(ID_PROPS_LINK, OnLinkProperties)
    ON_COMMAND(ID_INSERT_LINK, OnInsertLink)
    ON_COMMAND(ID_MAKE_LINK, OnMakeLink)
    ON_COMMAND(ID_REMOVE_LINKS, OnRemoveLinks)
    ON_COMMAND(ID_PROPS_TARGET, OnTargetProperties)
    ON_COMMAND(ID_INSERT_TARGET, OnInsertTarget)
    ON_COMMAND(ID_INSERT_IMAGE, OnInsertImage)
    ON_COMMAND(ID_INSERT_HRULE, OnInsertHRule)
    ON_COMMAND(ID_INSERT_TAG, OnInsertTag)
    ON_COMMAND(ID_PROPS_TAG, OnTagProperties)
    ON_COMMAND(ID_PROPS_IMAGE, OnImageProperties)
    ON_UPDATE_COMMAND_UI(ID_PROPS_IMAGE, OnUpdateImageProperties)
    ON_COMMAND(ID_PROPS_HRULE, OnHRuleProperties)
    ON_COMMAND(ID_PROPS_TEXT, OnTextProperties)
    ON_COMMAND(ID_PROPS_PARAGRAPH, OnParagraphProperties)
    ON_COMMAND(ID_PROPS_CHARACTER, OnCharacterProperties)
    ON_COMMAND(ID_PROPS_DOCUMENT, OnDocumentProperties)
    ON_COMMAND(ID_PROPS_DOC_COLOR, OnDocColorProperties)
    ON_UPDATE_COMMAND_UI(ID_PROPS_DOCUMENT, HaveEditContext)
    ON_COMMAND(ID_FORMAT_CHAR_NO_TEXT_STYLES, OnCharacterNoTextStyles)
    ON_COMMAND(ID_FORMAT_CHAR_NONE, OnCharacterNone)
    ON_COMMAND(ID_FORMAT_CHAR_NONE, OnCharacterNoTextStyles)
    ON_COMMAND(ID_FORMAT_CHAR_BOLD, OnCharacterBold)
    ON_COMMAND(IDC_FONT_FIXED_WIDTH, OnCharacterFixedWidth)
    ON_COMMAND(ID_FORMAT_CHAR_ITALIC, OnCharacterItalic)
    ON_COMMAND(ID_FORMAT_CHAR_NOBREAKS, OnCharacterNoBreaks)
    ON_COMMAND(ID_FORMAT_CHAR_UNDERLINE, OnCharacterUnderline)
    ON_COMMAND(ID_FORMAT_CHAR_SUPER, OnCharacterSuper)
    ON_COMMAND(ID_FORMAT_CHAR_SUB, OnCharacterSub)
    ON_COMMAND(ID_FORMAT_CHAR_STRIKEOUT, OnCharacterStrikeout)
    ON_COMMAND(ID_FORMAT_CHAR_BLINK, OnCharacterBlink)
    ON_COMMAND(ID_FORMAT_INDENT, OnFormatIndent)
    ON_COMMAND(ID_FORMAT_OUTDENT, OnFormatOutdent)
    ON_COMMAND(ID_ALIGN_POPUP, OnAlignPopup)
    ON_COMMAND(ID_INSERT_POPUP, OnInsertObjectPopup)
    ON_COMMAND(ID_ALIGN_CENTER, OnAlignCenter)
    ON_COMMAND(ID_ALIGN_LEFT, OnAlignLeft)
    ON_COMMAND(ID_ALIGN_RIGHT, OnAlignRight)
    ON_COMMAND(ID_PROPS_LOCAL,OnLocalProperties)
    ON_COMMAND(ID_INSERT_LINE_BREAK,OnInsertLineBreak)
    ON_COMMAND(ID_INSERT_BREAK_LEFT,OnInsertBreakLeft)
    ON_COMMAND(ID_INSERT_BREAK_RIGHT,OnInsertBreakRight)
    ON_COMMAND(ID_INSERT_BREAK_BOTH,OnInsertBreakBoth)
    ON_COMMAND(ID_INSERT_NONBREAK_SPACE,OnInsertNonbreakingSpace)
    ON_COMMAND(ID_REMOVE_LIST, OnRemoveList)
    ON_COMMAND(ID_UNUM_LIST, OnUnumList)
    ON_COMMAND(ID_NUM_LIST, OnNumList)
    ON_COMMAND(ID_BLOCK_QUOTE, OnBlockQuote)
    ON_COMMAND(ID_EDIT_SELECTALL, OnSelectAll)
    ON_COMMAND(ID_EDIT_UNDO, OnUndo)
	ON_COMMAND(	ID_FILE_PUBLISH, OnPublish)
    ON_COMMAND(ID_EDIT_DISPLAY_PARAGRAPH_MARKS, OnDisplayParagraphMarks)
    ON_COMMAND(ID_ALIGN_TABLE_LEFT, OnAlignTableLeft)
    ON_COMMAND(ID_ALIGN_TABLE_CENTER, OnAlignTableCenter)
    ON_COMMAND(ID_ALIGN_TABLE_RIGHT, OnAlignTableRight)
    ON_COMMAND(ID_SELECT_TABLE, OnSelectTable)
    ON_COMMAND(ID_SELECT_TABLE_ROW, OnSelectTableRow)
    ON_COMMAND(ID_SELECT_TABLE_COLUMN, OnSelectTableColumn)
    ON_COMMAND(ID_SELECT_TABLE_CELL, OnSelectTableCell)
    ON_COMMAND(ID_SELECT_TABLE_ALL_CELLS, OnSelectTableAllCells)
    ON_COMMAND(ID_MERGE_TABLE_CELLS, OnMergeTableCells)
    ON_COMMAND(ID_SPLIT_TABLE_CELL, OnSplitTableCell)
    ON_COMMAND(ID_TABLE_TEXT_CONVERT, OnTableTextConvert)
    ON_COMMAND(ID_INSERT_TABLE,OnInsertTable)
    ON_COMMAND(ID_INSERT_TABLE_OR_PROPS,OnInsertTableOrTableProps)
    ON_COMMAND(ID_INSERT_TABLE_ROW,OnInsertTableRow)
    ON_COMMAND(ID_INSERT_TABLE_ROW_ABOVE,OnInsertTableRowAbove)
    ON_COMMAND(ID_INSERT_TABLE_COLUMN,OnInsertTableColumn)
    ON_COMMAND(ID_INSERT_TABLE_COLUMN_BEFORE,OnInsertTableColumnBefore)
    ON_COMMAND(ID_INSERT_TABLE_CELL,OnInsertTableCell)
    ON_COMMAND(ID_INSERT_TABLE_CELL_BEFORE,OnInsertTableCellBefore)
    ON_COMMAND(ID_INSERT_TABLE_CAPTION,OnInsertTableCaption)
    ON_COMMAND(ID_DELETE_TABLE,OnDeleteTable)
    ON_COMMAND(ID_DELETE_TABLE_ROW,OnDeleteTableRow)
    ON_COMMAND(ID_DELETE_TABLE_COLUMN,OnDeleteTableColumn)
    ON_COMMAND(ID_DELETE_TABLE_CELL,OnDeleteTableCell)
    ON_COMMAND(ID_DELETE_TABLE_CAPTION,OnDeleteTableCaption)
    ON_COMMAND(ID_TOGGLE_TABLE_BORDER,OnToggleTableBorder)
    ON_COMMAND(ID_TOGGLE_HEADER_CELL,OnToggleHeaderCell)
    ON_COMMAND(ID_PROPS_TABLE,OnPropsTable)
    ON_COMMAND(ID_PROPS_TABLE_ROW,OnPropsTableRow)
    ON_COMMAND(ID_PROPS_TABLE_COLUMN,OnPropsTableColumn)
    ON_COMMAND(ID_PROPS_TABLE_CELL,OnPropsTableCell)
    ON_COMMAND(ID_DISPLAY_TABLES, OnDisplayTables)
    // Browser handles this in CAbstractCX, but editor will do stuff here
    ON_COMMAND(ID_EDIT_CUT, OnEditCut)
    ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
    ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
    ON_COMMAND(ID_COPY_STYLE, OnCopyStyle)
    ON_COMMAND(ID_EDIT_DELETE, OnEditDelete)
    ON_COMMAND(ID_EDIT_FINDAGAIN, OnEditFindAgain)
    ON_COMMAND( ID_EDITOR_PAGEUP, OnPageUp )
    ON_COMMAND( ID_EDITOR_PAGEDOWN, OnPageDown )
    ON_COMMAND( ID_EDITOR_SELECTPAGEUP, OnSelectPageUp )
    ON_COMMAND( ID_EDITOR_SELECTPAGEDOWN, OnSelectPageDown )
    ON_COMMAND( ID_FILE_EDITSOURCE, OnEditSource)
    ON_COMMAND( ID_GO_PUBLISH_LOCATION, OnGoToDefaultPublishLocation)
    ON_COMMAND( ID_MAKE_IMAGE_BACKGROUND, OnSetImageAsBackground)
    ON_CBN_DROPDOWN(ID_COMBO_FONTSIZE, OnFontSizeDropDown)
    ON_COMMAND(ID_OPT_EDITBAR_TOGGLE, OnEditBarToggle)
    ON_COMMAND(ID_OPT_CHARBAR_TOGGLE, OnCharacterBarToggle)
    ON_UPDATE_COMMAND_UI(ID_OPT_EDITBAR_TOGGLE, OnUpdateEditBarToggle)
    ON_UPDATE_COMMAND_UI(ID_OPT_CHARBAR_TOGGLE, OnUpdateCharacterBarToggle)
    ON_COMMAND(ID_SELECT_NEXT_NONTEXT, OnSelectNextNonTextObject)
	//}}AFX_MSG_MAP
    // Put update commands here so we can remove them temporarily for overhead assesments
	ON_UPDATE_COMMAND_UI(ID_FORMAT_INCREASE_FONTSIZE, OnUpdateIncreaseFontSize)
	ON_UPDATE_COMMAND_UI(ID_FORMAT_DECREASE_FONTSIZE, OnUpdateDecreaseFontSize)
    ON_UPDATE_COMMAND_UI(ID_COMBO_PARA, OnUpdateParagraphComboBox)
    ON_UPDATE_COMMAND_UI(ID_COMBO_FONTFACE, OnUpdateFontFaceComboBox)
	ON_UPDATE_COMMAND_UI(ID_COMBO_FONTSIZE, OnUpdateFontSizeComboBox)
	ON_UPDATE_COMMAND_UI(ID_COMBO_FONTCOLOR, OnUpdateFontColorComboBox)
	ON_UPDATE_COMMAND_UI(ID_REMOVE_FONTFACE,  HaveEditContext)
    ON_UPDATE_COMMAND_UI(ID_PROPS_LINK, OnUpdateLinkProperties)
    ON_UPDATE_COMMAND_UI(ID_INSERT_LINK, OnUpdateInsertLink)
    ON_UPDATE_COMMAND_UI(ID_MAKE_LINK, HaveEditContext)
    ON_UPDATE_COMMAND_UI(ID_REMOVE_LINKS, OnUpdateRemoveLinks)
    ON_UPDATE_COMMAND_UI(ID_PROPS_TARGET, OnUpdateTargetProperties)
    ON_UPDATE_COMMAND_UI(ID_INSERT_TARGET, HaveEditContext)
    ON_UPDATE_COMMAND_UI(ID_PROPS_TARGET, HaveEditContext)
    ON_UPDATE_COMMAND_UI(ID_INSERT_IMAGE, HaveEditContext)
    ON_UPDATE_COMMAND_UI(ID_INSERT_HRULE, HaveEditContext)
    ON_UPDATE_COMMAND_UI(ID_INSERT_TAG, HaveEditContext)
    ON_UPDATE_COMMAND_UI(ID_PROPS_TAG, OnUpdateTagProperties)
    ON_UPDATE_COMMAND_UI(ID_PROPS_HRULE, OnUpdateHRuleProperties)
    ON_UPDATE_COMMAND_UI(ID_PROPS_TEXT, HaveEditContext)
    ON_UPDATE_COMMAND_UI(ID_PROPS_PARAGRAPH, HaveEditContext)
    ON_UPDATE_COMMAND_UI(ID_PROPS_CHARACTER, HaveEditContext)
    ON_UPDATE_COMMAND_UI(ID_PROPS_DOC_COLOR, HaveEditContext)
    ON_UPDATE_COMMAND_UI(ID_FORMAT_CHAR_NONE, OnCanInteract)     
    ON_UPDATE_COMMAND_UI(ID_FORMAT_CHAR_BOLD, OnUpdateCharacterBold)          
    ON_UPDATE_COMMAND_UI(ID_FORMAT_CHAR_ITALIC, OnUpdateCharacterItalic)      
    ON_UPDATE_COMMAND_UI(ID_FORMAT_CHAR_NOBREAKS, OnUpdateCharacterNoBreaks)   
    ON_UPDATE_COMMAND_UI(ID_FORMAT_CHAR_UNDERLINE, OnUpdateCharacterUnderline)
	ON_UPDATE_COMMAND_UI(ID_FORMAT_CHAR_SUPER, OnUpdateCharacterSuper)        
    ON_UPDATE_COMMAND_UI(ID_FORMAT_CHAR_SUB, OnUpdateCharacterSub)            
    ON_UPDATE_COMMAND_UI(ID_FORMAT_CHAR_STRIKEOUT, OnUpdateCharacterStrikeout)
    ON_UPDATE_COMMAND_UI(ID_FORMAT_CHAR_BLINK, OnUpdateCharacterBlink)        
    ON_UPDATE_COMMAND_UI(ID_FORMAT_INDENT, HaveEditContext)
    ON_UPDATE_COMMAND_UI(ID_FORMAT_OUTDENT, HaveEditContext)
    ON_UPDATE_COMMAND_UI(ID_ALIGN_POPUP, HaveEditContext)
    ON_UPDATE_COMMAND_UI(ID_PROPS_LOCAL, OnUpdatePropsLocal)
    ON_UPDATE_COMMAND_UI(ID_INSERT_POPUP, HaveEditContext)
    ON_UPDATE_COMMAND_UI(ID_ALIGN_CENTER, HaveEditContext)
    ON_UPDATE_COMMAND_UI(ID_ALIGN_LEFT, OnUpdateAlignLeft)
    ON_UPDATE_COMMAND_UI(ID_ALIGN_RIGHT, HaveEditContext)
    ON_UPDATE_COMMAND_UI(ID_INSERT_LINE_BREAK, OnUpdateInsertBreak)
    ON_UPDATE_COMMAND_UI(ID_INSERT_BREAK_LEFT, OnUpdateInsertBreak)
    ON_UPDATE_COMMAND_UI(ID_INSERT_BREAK_RIGHT, OnUpdateInsertBreak)
    ON_UPDATE_COMMAND_UI(ID_INSERT_BREAK_BOTH, OnUpdateInsertBreak)
    ON_UPDATE_COMMAND_UI(ID_REMOVE_LIST, OnUpdateRemoveList)
    ON_UPDATE_COMMAND_UI(ID_UNUM_LIST, OnUpdateUnumList)
    ON_UPDATE_COMMAND_UI(ID_NUM_LIST, OnUpdateNumList)
    ON_UPDATE_COMMAND_UI(ID_EDIT_SELECTALL, HaveEditContext)
    ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateUndo)
	ON_UPDATE_COMMAND_UI(ID_FILE_PUBLISH, HaveEditContext)
    ON_UPDATE_COMMAND_UI(ID_EDIT_DISPLAY_PARAGRAPH_MARKS, OnUpdateDisplayParagraphMarks)
    ON_UPDATE_COMMAND_UI(ID_ALIGN_TABLE_LEFT, OnUpdateInTable)
    ON_UPDATE_COMMAND_UI(ID_ALIGN_TABLE_RIGHT, OnUpdateInTable)
    ON_UPDATE_COMMAND_UI(ID_ALIGN_TABLE_CENTER, OnUpdateInTable)
    ON_UPDATE_COMMAND_UI(ID_SELECT_TABLE, OnUpdateInTable)
    ON_UPDATE_COMMAND_UI(ID_SELECT_TABLE_ROW, OnUpdateInTableRow)
    ON_UPDATE_COMMAND_UI(ID_SELECT_TABLE_COLUMN, OnUpdateInTableColumn)
    ON_UPDATE_COMMAND_UI(ID_SELECT_TABLE_CELL, OnUpdateInTableCell)
    ON_UPDATE_COMMAND_UI(ID_SELECT_TABLE_ALL_CELLS, OnUpdateInTableCell)
    ON_UPDATE_COMMAND_UI(ID_MERGE_TABLE_CELLS, OnUpdateMergeTableCells)
    ON_UPDATE_COMMAND_UI(ID_SPLIT_TABLE_CELL, OnUpdateSplitTableCell)
    ON_UPDATE_COMMAND_UI(ID_TABLE_TEXT_CONVERT, OnUpdateTableTextConvert)
    ON_UPDATE_COMMAND_UI(ID_INSERT_TABLE, OnUpdateInsertTable)
    ON_UPDATE_COMMAND_UI(ID_INSERT_TABLE_ROW, OnUpdateInsertTableRow)
    ON_UPDATE_COMMAND_UI(ID_INSERT_TABLE_ROW_ABOVE, OnUpdateInsertTableRow)
    ON_UPDATE_COMMAND_UI(ID_INSERT_TABLE_CELL, OnUpdateInsertTableCell)
    ON_UPDATE_COMMAND_UI(ID_INSERT_TABLE_CELL_BEFORE, OnUpdateInsertTableCell)
    ON_UPDATE_COMMAND_UI(ID_INSERT_TABLE_COLUMN, OnUpdateInsertTableColumn)
    ON_UPDATE_COMMAND_UI(ID_INSERT_TABLE_COLUMN_BEFORE, OnUpdateInsertTableColumn)
    ON_UPDATE_COMMAND_UI(ID_INSERT_TABLE_CAPTION, OnUpdateInsertTableCaption)
    ON_UPDATE_COMMAND_UI(ID_DELETE_TABLE, OnUpdateInTable)
    ON_UPDATE_COMMAND_UI(ID_DELETE_TABLE_ROW, OnUpdateInTableRow)
    ON_UPDATE_COMMAND_UI(ID_DELETE_TABLE_COLUMN, OnUpdateInTableColumn)
    ON_UPDATE_COMMAND_UI(ID_DELETE_TABLE_CELL, OnUpdateInTableCell)
    ON_UPDATE_COMMAND_UI(ID_DELETE_TABLE_CAPTION, OnUpdateInTableCaption)
    ON_UPDATE_COMMAND_UI(ID_TOGGLE_TABLE_BORDER, OnUpdateToggleTableBorder)
    ON_UPDATE_COMMAND_UI(ID_TOGGLE_HEADER_CELL, OnUpdateToggleHeaderCell)
    ON_UPDATE_COMMAND_UI(ID_PROPS_TABLE, OnUpdateInTable)
    ON_UPDATE_COMMAND_UI(ID_PROPS_TABLE_ROW, OnUpdateInTableRow)
    ON_UPDATE_COMMAND_UI(ID_PROPS_TABLE_COLUMN, OnUpdateInTableColumn)
    ON_UPDATE_COMMAND_UI(ID_PROPS_TABLE_CELL, OnUpdateInTableCell)
    ON_UPDATE_COMMAND_UI(ID_DISPLAY_TABLES, OnUpdateDisplayTables)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
	ON_UPDATE_COMMAND_UI(ID_COPY_STYLE, OnUpdateCopyStyle)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditCut)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE, OnCanInteract)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
	ON_UPDATE_COMMAND_UI(ID_LOCAL_POPUP, OnCanInteract)
	ON_UPDATE_COMMAND_UI(ID_FILE_EDITSOURCE, OnCanInteract)
	ON_UPDATE_COMMAND_UI(ID_GO_PUBLISH_LOCATION, OnCanInteract)
    // Paragraph tags cover whole range starting from P_TEXT (=0),
    //  but "Normal" is first menu item (tag = P_PARAGRAPH)
    ON_UPDATE_COMMAND_UI((ID_FORMAT_PARAGRAPH_BASE+P_NSDT), OnUpdateParagraphMenu)
    ON_UPDATE_COMMAND_UI((ID_FORMAT_PARAGRAPH_BASE+P_HEADER_1), OnUpdateParagraphMenu)
    ON_UPDATE_COMMAND_UI((ID_FORMAT_PARAGRAPH_BASE+P_HEADER_2), OnUpdateParagraphMenu)
    ON_UPDATE_COMMAND_UI((ID_FORMAT_PARAGRAPH_BASE+P_HEADER_3), OnUpdateParagraphMenu)
    ON_UPDATE_COMMAND_UI((ID_FORMAT_PARAGRAPH_BASE+P_HEADER_4), OnUpdateParagraphMenu)
    ON_UPDATE_COMMAND_UI((ID_FORMAT_PARAGRAPH_BASE+P_HEADER_5), OnUpdateParagraphMenu)
    ON_UPDATE_COMMAND_UI((ID_FORMAT_PARAGRAPH_BASE+P_HEADER_6), OnUpdateParagraphMenu)
    ON_UPDATE_COMMAND_UI((ID_FORMAT_PARAGRAPH_BASE+P_PARAGRAPH), OnUpdateParagraphMenu)
    ON_UPDATE_COMMAND_UI((ID_FORMAT_PARAGRAPH_BASE+P_DESC_TEXT), OnUpdateParagraphMenu)
    ON_UPDATE_COMMAND_UI((ID_FORMAT_PARAGRAPH_BASE+P_ADDRESS), OnUpdateParagraphMenu)
    ON_UPDATE_COMMAND_UI((ID_FORMAT_PARAGRAPH_BASE+P_PREFORMAT), OnUpdateParagraphMenu)
    ON_UPDATE_COMMAND_UI((ID_FORMAT_PARAGRAPH_BASE+P_DESC_TITLE), OnUpdateParagraphMenu)
    ON_UPDATE_COMMAND_UI((ID_FORMAT_PARAGRAPH_BASE+P_BLOCKQUOTE), OnUpdateParagraphMenu)
	ON_UPDATE_COMMAND_UI((ID_FORMAT_FONTSIZE_BASE), OnUpdateFontSizeMenu)
	ON_UPDATE_COMMAND_UI((ID_FORMAT_FONTSIZE_BASE+1), OnUpdateFontSizeMenu)
	ON_UPDATE_COMMAND_UI((ID_FORMAT_FONTSIZE_BASE+2), OnUpdateFontSizeMenu)
	ON_UPDATE_COMMAND_UI((ID_FORMAT_FONTSIZE_BASE+3), OnUpdateFontSizeMenu)
	ON_UPDATE_COMMAND_UI((ID_FORMAT_FONTSIZE_BASE+4), OnUpdateFontSizeMenu)
	ON_UPDATE_COMMAND_UI((ID_FORMAT_FONTSIZE_BASE+5), OnUpdateFontSizeMenu)
	ON_UPDATE_COMMAND_UI((ID_FORMAT_FONTSIZE_BASE+6), OnUpdateFontSizeMenu)
#ifdef _IME_COMPOSITION
    ON_WM_LBUTTONDOWN()
    #ifdef WIN32
    ON_MESSAGE(WM_IME_COMPOSITION,CNetscapeEditView::OnWmeImeComposition)
    ON_MESSAGE(WM_IME_STARTCOMPOSITION,CNetscapeEditView::OnWmeImeStartComposition)
    ON_MESSAGE(WM_IME_ENDCOMPOSITION,CNetscapeEditView::OnWmeImeEndComposition)
    ON_MESSAGE(WM_IME_KEYDOWN,CNetscapeEditView::OnWmeImeKeyDown)
    ON_MESSAGE(WM_INPUTLANGCHANGE,CNetscapeEditView::OnInputLanguageChange)
    ON_MESSAGE(WM_INPUTLANGCHANGEREQUEST,CNetscapeEditView::OnInputLanguageChangeRequest)
    #else //win16
    ON_MESSAGE(WM_IME_REPORT,CNetscapeEditView::OnReportIme)
    #endif //WIN32
#endif //_IME_COMPOSITION
    ON_MESSAGE(NSBUTTONMENUOPEN, OnButtonMenuOpen)
    ON_COMMAND((ID_FORMAT_FONTSIZE_BASE),   OnFontSize_2)
    ON_COMMAND((ID_FORMAT_FONTSIZE_BASE+1), OnFontSize_1)
    ON_COMMAND((ID_FORMAT_FONTSIZE_BASE+2), OnFontSize0)
    ON_COMMAND((ID_FORMAT_FONTSIZE_BASE+3), OnFontSize1)
    ON_COMMAND((ID_FORMAT_FONTSIZE_BASE+4), OnFontSize2)
    ON_COMMAND((ID_FORMAT_FONTSIZE_BASE+5), OnFontSize3)
    ON_COMMAND((ID_FORMAT_FONTSIZE_BASE+6), OnFontSize4)
    ON_COMMAND(ID_FORMAT_POINTSIZE_BASE, OnPointSize8)
    ON_COMMAND((ID_FORMAT_POINTSIZE_BASE+1), OnPointSize9)
    ON_COMMAND((ID_FORMAT_POINTSIZE_BASE+2), OnPointSize10)
    ON_COMMAND((ID_FORMAT_POINTSIZE_BASE+3), OnPointSize11)
    ON_COMMAND((ID_FORMAT_POINTSIZE_BASE+4), OnPointSize12)
    ON_COMMAND((ID_FORMAT_POINTSIZE_BASE+5), OnPointSize14)
    ON_COMMAND((ID_FORMAT_POINTSIZE_BASE+6), OnPointSize16)
    ON_COMMAND((ID_FORMAT_POINTSIZE_BASE+7), OnPointSize18)
    ON_COMMAND((ID_FORMAT_POINTSIZE_BASE+8), OnPointSize20)
    ON_COMMAND((ID_FORMAT_POINTSIZE_BASE+9), OnPointSize22)
    ON_COMMAND((ID_FORMAT_POINTSIZE_BASE+10), OnPointSize24)
    ON_COMMAND((ID_FORMAT_POINTSIZE_BASE+11), OnPointSize28)
    ON_COMMAND((ID_FORMAT_POINTSIZE_BASE+12), OnPointSize36)
    ON_COMMAND((ID_FORMAT_POINTSIZE_BASE+13), OnPointSize48)
    ON_COMMAND((ID_FORMAT_POINTSIZE_BASE+14), OnPointSize72)
	ON_UPDATE_COMMAND_UI(ID_FORMAT_POINTSIZE_BASE, HaveEditContext)
	ON_UPDATE_COMMAND_UI((ID_FORMAT_POINTSIZE_BASE+1), HaveEditContext)
	ON_UPDATE_COMMAND_UI((ID_FORMAT_POINTSIZE_BASE+2), HaveEditContext)
	ON_UPDATE_COMMAND_UI((ID_FORMAT_POINTSIZE_BASE+3), HaveEditContext)
	ON_UPDATE_COMMAND_UI((ID_FORMAT_POINTSIZE_BASE+4), HaveEditContext)
	ON_UPDATE_COMMAND_UI((ID_FORMAT_POINTSIZE_BASE+5), HaveEditContext)
	ON_UPDATE_COMMAND_UI((ID_FORMAT_POINTSIZE_BASE+6), HaveEditContext)
	ON_UPDATE_COMMAND_UI((ID_FORMAT_POINTSIZE_BASE+7), HaveEditContext)
	ON_UPDATE_COMMAND_UI((ID_FORMAT_POINTSIZE_BASE+8), HaveEditContext)
	ON_UPDATE_COMMAND_UI((ID_FORMAT_POINTSIZE_BASE+9), HaveEditContext)
	ON_UPDATE_COMMAND_UI((ID_FORMAT_POINTSIZE_BASE+10), HaveEditContext)
	ON_UPDATE_COMMAND_UI((ID_FORMAT_POINTSIZE_BASE+11), HaveEditContext)
	ON_UPDATE_COMMAND_UI((ID_FORMAT_POINTSIZE_BASE+12), HaveEditContext)
	ON_UPDATE_COMMAND_UI((ID_FORMAT_POINTSIZE_BASE+13), HaveEditContext)
	ON_UPDATE_COMMAND_UI((ID_FORMAT_POINTSIZE_BASE+14), HaveEditContext)
	ON_COMMAND(ID_CHECK_SPELLING, OnCheckSpelling)
	ON_UPDATE_COMMAND_UI(ID_CHECK_SPELLING, OnCanInteract)
	ON_UPDATE_COMMAND_UI(ID_INSERT_NONBREAK_SPACE, OnCanInteract)
	ON_UPDATE_COMMAND_UI(ID_GET_COLOR, OnCanInteract)
    ON_UPDATE_COMMAND_UI(ID_EDT_FILE_SAVE, OnUpdateFileSave)
    ON_UPDATE_COMMAND_UI(ID_EDT_FILE_SAVEAS, OnCanInteract)
    ON_UPDATE_COMMAND_UI(ID_FILE_MAILTO, OnCanInteract)
    ON_UPDATE_COMMAND_UI(ID_FILE_PUBLISH, OnCanInteract)
    ON_UPDATE_COMMAND_UI(ID_FILE_PAGE_SETUP, OnCanInteract)
    ON_UPDATE_COMMAND_UI(ID_FILE_PRINT_PREVIEW, OnCanInteract)
    ON_UPDATE_COMMAND_UI(ID_EDIT_FINDINCURRENT, OnCanInteract)
    ON_UPDATE_COMMAND_UI(ID_EDIT_FINDAGAIN, OnUpdateEditFindAgain)
    ON_UPDATE_COMMAND_UI(ID_FILE_PRINT, OnCanInteract)
    ON_UPDATE_COMMAND_UI(ID_FILE_PRINT, OnCanInteract)
    ON_UPDATE_COMMAND_UI(ID_FILE_DOCINFO, OnUpdateFileDocinfo)
    ON_UPDATE_COMMAND_UI(ID_OPEN_NAV_WINDOW, OnCanInteract)
END_MESSAGE_MAP()

#define new DEBUG_NEW  // MSVC Debugging new...goes to regular new in release mode

CNetscapeEditView::CNetscapeEditView()  
{
	m_bIsEditor = TRUE;
    m_pEditDropTarget = NULL;
    m_bAutoSelectObject = FALSE;
    m_nLoadingImageCount = 0;
    m_pLoadingImageDlg = NULL;
    m_FileSaveStatus = ED_ERROR_NONE;
	
    memset( (void*)&m_EditState, 0, sizeof( ED_FORMATSTATE ) );
    m_EditState.nParagraphFormat = P_UNKNOWN;

    // Initialize with black
    m_EditState.crFontColor = NO_COLORREF;
    
    m_bSaveRemoteToLocal = FALSE;

    m_nFontColorCount = 16;
    m_crDefColor = RGB(0,0,0);
    m_pImagePage = NULL;
    m_pColorPage = NULL;

    m_caret.bEnabled = FALSE;
    m_caret.cShown = 0;
    m_caret.x = 0;
    m_caret.y = 0;
    // Initialize caret size in case we get focus before DisplayCaret is called
    m_caret.width = 2;
    m_caret.height = 20;

    m_bDragOver = FALSE;
    m_crLastDragRect.SetRectEmpty();

    m_pPluginInfo = NULL;
    m_NumPlugins = 0;
    m_pime=NULL;
#ifdef XP_WIN16
    m_hIME=NULL;
#endif
#ifdef _IME_COMPOSITION
    m_imebool=FALSE;
    m_pchardata=NULL;
    m_imeoldcursorpos= (DWORD)-1;
#endif
    m_pTempFilename = NULL;

    SetEditChanged();
}

CNetscapeEditView::~CNetscapeEditView()  
{
    if(m_pEditDropTarget) {
        m_pEditDropTarget->Revoke();
        delete m_pEditDropTarget;
        m_pEditDropTarget = NULL;
    }
#ifdef XP_WIN16
    ImeDestroy();
#endif
#ifdef _IME_COMPOSITION
    if (m_pime)//inline eastern input
        delete m_pime;
    XP_FREEIF(m_pchardata);
#endif

    XP_FREEIF(m_pPluginInfo);
    XP_FREEIF(m_pTempFilename);
}




/////////////////////////////////////////////////////////////////////////////
// CNetscapeEditView diagnostics

#ifdef _DEBUG
void CNetscapeEditView::AssertValid() const
{
    CGenericView::AssertValid();
}

void CNetscapeEditView::Dump(CDumpContext& dc) const
{
    CGenericView::Dump(dc);
}
#endif //_DEBUG



// Layout routines call this when caret has moved and there might be 
//   a change in format states
void CNetscapeEditView::SetEditChanged()
{
	// Set flags to check if items in comboboxes need to change
    m_EditState.bParaFormatMaybeChanged = TRUE;
    m_EditState.bFontFaceMaybeChanged = TRUE;
    m_EditState.bFontSizeMaybeChanged = TRUE;
    m_EditState.bFontColorMaybeChanged = TRUE;
}


/////////////////////////////////////////////////////////////////////////////
int CNetscapeEditView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    // Show the Wait cursor. The cursor is restored in the destructor.
    CWaitCursor WaitCursor;

    int nRetVal = CNetscapeView::OnCreate(lpCreateStruct);

    // Save our frame since we use it alot
    // Do it even if we fail creating the base class 'cause we
    //  might get OnUpdateCommandUI messages that use frame

    if ( nRetVal ){

        m_EditState.crFontColor = DEFAULT_COLORREF;
        m_crDefColor = RGB(0,0,0);
    }
    // Replace the base class' drop target with ours
    if(m_pDropTarget) {
        m_pDropTarget->Revoke();
        delete m_pDropTarget;
        m_pDropTarget = NULL;
    }
    if(!m_pEditDropTarget) {
        m_pEditDropTarget = new CEditViewDropTarget;
        m_pEditDropTarget->Register(this);
    }
#if _IME_COMPOSITION
#ifdef XP_WIN32
    m_csid=CIntlWin::CodePageToCsid(GetOEMCP());
#endif //XP_WIN32
#endif
	AddPluginMenus();
#ifdef XP_WIN16
    initializeIME();
#endif //XP_WIN16
    return nRetVal;
}

// AddPluginMenus - called from OnCreate to add menu items for the loaded editor plugin modules.

void CNetscapeEditView::AddPluginMenus()
{
	CFrameWnd *pFrame = GetParentFrame();
	ASSERT(pFrame != NULL);

    // Get the menu bar
    CMenu *pMenu=pFrame->GetMenu();
    if (pMenu == NULL)
    {
        ASSERT(FALSE);
        return;
    }

    // find the Tools submenu
    CString MenuText;
    MenuText.LoadString(IDS_TOOLS_MENU);

    CMenu   *pToolsMenu = NULL;
    int     ToolsMenuPosition = WFE_FindMenu(pMenu, MenuText); 

    if (ToolsMenuPosition != -1)
        pToolsMenu = pMenu->GetSubMenu(ToolsMenuPosition);

    if (pToolsMenu == NULL)
    {
        ASSERT(FALSE);
        return;
    }

    // Remove the dummy plugin menu item
	if (pToolsMenu->RemoveMenu(ID_EDITOR_PLUGINS_BASE, MF_BYCOMMAND) == 0)
        return;     // couldn't locate the menu item

	// Add menu ids for loaded editor plugins
    int32 NumCategories = EDT_NumberOfPluginCategories();

	if (NumCategories != 0)
	    pToolsMenu->InsertMenu(ID_STOP_EDITOR_PLUGIN, MF_SEPARATOR, 0, (LPCTSTR)NULL);
    else
	    pToolsMenu->RemoveMenu(ID_STOP_EDITOR_PLUGIN, MF_BYCOMMAND);

    // Create a popup menu for each plugin category
    UINT MenuId = ID_EDITOR_PLUGINS_BASE;
    for (int32 Cat = 0; Cat < NumCategories; Cat++)
    {
        char*   CatName = EDT_GetPluginCategoryName(Cat);

        if (CatName != NULL)
        {
    	    int32   NumPlugins = EDT_NumberOfPlugins(Cat);
            if (NumPlugins == 0)        // category with no plugins?
                continue;

            HMENU   PopupMenu = ::CreatePopupMenu();
            if (PopupMenu == NULL)
            {
#ifdef DEBUG
                MessageBox("Could not create popup menu for editor plugins");
#endif
                return;
            }

            // Add the popup menu to the Tools menu group
            pToolsMenu->InsertMenu(ID_STOP_EDITOR_PLUGIN, MF_BYCOMMAND | MF_STRING | MF_POPUP, (UINT)PopupMenu, CatName);
              
            // Add plugin menu items to the popup menu group
    	    for (int32 Plugin = 0; Plugin < NumPlugins; Plugin++, MenuId++)
	        {
	            char* PluginName = EDT_GetPluginName(Cat, Plugin);
	            if (PluginName != NULL)
                {
                    if (MenuId < ID_EDITOR_PLUGINS_BASE + MAX_EDITOR_PLUGINS)
                    {
                        BOOL Status = ::AppendMenu(PopupMenu, MF_STRING, MenuId, PluginName);
                        if (Status == FALSE)
                        {
                            ASSERT(FALSE);
                            return;
                        }

                        m_NumPlugins++;
                        m_pPluginInfo = (PluginInfo *)XP_REALLOC(m_pPluginInfo, 
                                                                 sizeof(PluginInfo) * m_NumPlugins);
                        if (m_pPluginInfo == NULL)
                        {
                            ASSERT(FALSE);
                            return;
                        }
                        int Index = CASTINT(MenuId - ID_EDITOR_PLUGINS_BASE);
                        m_pPluginInfo[Index].CategoryId = Cat;
                        m_pPluginInfo[Index].PluginId = Plugin;
                    }
                    else
                    {
#ifdef DEBUG
                        MessageBox("Too many editor plugins.");
#endif
                        return;
                    }
                }
	            else
		            ASSERT(FALSE);          // NULL plugin name
            }
            
        }
        else
            ASSERT(FALSE);                  // NULL category name
    } // for
}



BOOL CNetscapeEditView::GetPluginInfo(UINT MenuId, uint32 *CategoryId, uint32 *PluginId)
{
    // Check for invalid menu id
	if (MenuId < ID_EDITOR_PLUGINS_BASE || MenuId >= (ID_EDITOR_PLUGINS_BASE + m_NumPlugins))
        return FALSE;

    UINT Index = CASTUINT(MenuId - ID_EDITOR_PLUGINS_BASE);
    *CategoryId = m_pPluginInfo[Index].CategoryId;
    *PluginId = m_pPluginInfo[Index].PluginId;
    return TRUE;
}
/////////////////////////////////////////////////////////
// Simple dialog with 2 choices -- use so buttons have
//   better text than "Yes"/"No" or "OK"/"Cancel"
// Used only in OnSetFocus() below
class CResolveEditChangesDlg : public CDialog
{
// Construction
public:
	CResolveEditChangesDlg(CWnd * pParent = NULL);
    BOOL  bUseExternalChanges;
	               
private:
    // This will change resource hInstance to Editor dll
    CEditorResourceSwitcher m_ResourceSwitcher;
protected:
	//{{AFX_MSG(CPublishDlg)
   	virtual BOOL OnInitDialog();
	afx_msg void OnUseInternalChanges();
	afx_msg void OnUseExternalChanges();
	//}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

CResolveEditChangesDlg::CResolveEditChangesDlg(CWnd * pParent)
	: CDialog(IDD_RESOLVE_EDIT_CHANGES, pParent),
    bUseExternalChanges(FALSE)
{
}

BOOL CResolveEditChangesDlg::OnInitDialog() 
{
    // Switch back to NETSCAPE.EXE for resource hInstance
    m_ResourceSwitcher.Reset();
	CDialog::OnInitDialog();
	return TRUE;
}

BEGIN_MESSAGE_MAP(CResolveEditChangesDlg, CDialog)
	//{{AFX_MSG_MAP(CResolveEditChangesDlg)
	ON_BN_CLICKED(IDC_USE_INTERNAL_CHANGES, OnUseInternalChanges)
	ON_BN_CLICKED(IDC_USE_EXTERNAL_CHANGES, OnUseExternalChanges)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CResolveEditChangesDlg::OnUseInternalChanges()
{
    bUseExternalChanges = FALSE;
    OnOK();
}

void CResolveEditChangesDlg::OnUseExternalChanges()
{
    bUseExternalChanges = TRUE;
    OnOK();
}
///////////////////////////////////////////////////////

void CNetscapeEditView::OnSetFocus(CWnd *pOldWin)   
{
    if (m_pChild && ::IsWindow(m_pChild->m_hWnd))
    {
      m_pChild->SetFocus();
      return;
    }
#ifdef MOZ_MAIL_NEWS
    if (GetParentFrame()->IsKindOf(RUNTIME_CLASS(CComposeFrame)))
    {
        CComposeFrame * pFrame = (CComposeFrame*)GetParentFrame();
        // This simply sets CComposeFrame::m_FocusField,
        //  which may be a problem if we really want to return 
        //  focus to previous window, such as address area
        pFrame->SetFocusField(this);
    }
#endif // MOZ_MAIL_NEWS
    CNetscapeView::OnSetFocus(pOldWin);
    //TRACE2("CNetscapeEditView::OnSetFocus hCurrentFocus=%X, View=%X\n", (pOldWin ? pOldWin->m_hWnd : 0), this->m_hWnd );

    // create a caret if there is no selected text
    MWContext * pMWContext = GET_MWCONTEXT;
    if ( pMWContext ){
        // Check if the file has been modified by an outside editor
        // Reload it reload if it has. SaveAs triggers this, so check flag first
        if( EDT_IsFileModified(pMWContext) ){
            if( EDT_DirtyFlag(pMWContext) ){
                // In this case, there are unsaved changes in current page
                //  AND changes in the external editor's version (saved to the file)
                //  User must pick one or the other 
                CResolveEditChangesDlg dlg(GET_DLG_PARENT(this));
                dlg.DoModal();
                if( dlg.bUseExternalChanges ){
                    // Load changes from the file, but
                    //  we must fool editor in thinking its not dirty
                    //  else we get a save prompt when loading URL
                    EDT_SetDirtyFlag(pMWContext, FALSE);
                    GetContext()->NiceReload();
                }
                // Note: we do NOT save the changes to file 
                //    if use rejected using the external changes
            } else if( IDYES == MessageBox( szLoadString(IDS_FILE_IS_MODIFIED),
                        szLoadString(IDS_FILE_MODIFIED_CAPTION),
                        MB_YESNO | MB_ICONQUESTION) ){
                GetContext()->NiceReload();
            }
        } else if( !EDT_IsSelected(pMWContext) ){
            //TRACE1( "CNetscapeEditView::OnSetFocus: Caret created and enabled. bEnable was %d\n",m_caret.bEnabled);
            CreateSolidCaret(m_caret.width,m_caret.height);
            SetCaretPos(CPoint(m_caret.x, m_caret.y));
            ShowCaret();
            m_caret.cShown = 1;
            m_caret.bEnabled = TRUE;
        }
    }
#ifdef _IME_COMPOSITION
    if (m_imebool)
    {    
        if (m_pime)
        {
    #ifdef XP_WIN32
            CFont *t_phfont=CFont::FromHandle((HFONT)GetStockObject(SYSTEM_FONT));
            if (t_phfont)
            {
                LOGFONT t_logfont;
                if (t_phfont->GetLogFont(&t_logfont))
                    ImeSetFont(this->GetSafeHwnd(),&t_logfont);
            }
    #else
    #endif //XP_WIN32 else xp_win16
        }
    }
#ifdef XP_WIN16
    initializeIME();
    if (!pOldWin||(pOldWin->m_hWnd!=GetParent()->m_hWnd))
    {
        LPIMESTRUCT lpIme;
        if (!(lpIme = (LPIMESTRUCT)GlobalLock(m_hIME)))
            return;

        lpIme->fnc=IME_GETOPEN;
        lpIme->wParam=0;
        lpIme->lParam1=0;
        lpIme->lParam2=0;
        lpIme->lParam3=0;
        GlobalUnlock(m_hIME);
        if (m_pime->SendIMEMessageEx(this->GetSafeHwnd(),m_lIMEParam))
        {
            if (!(lpIme = (LPIMESTRUCT)GlobalLock(m_hIME)))
                return;
            lpIme->fnc=IME_SETOPEN;
            lpIme->wParam=0;
            lpIme->lParam1=0;
            lpIme->lParam2=0;
            lpIme->lParam3=0;
            GlobalUnlock(m_hIME);
            m_pime->SendIMEMessageEx(this->GetSafeHwnd(),m_lIMEParam);
            if (!(lpIme = (LPIMESTRUCT)GlobalLock(m_hIME)))
                return;
            lpIme->fnc=IME_SETOPEN;
            lpIme->wParam=1;
            lpIme->lParam1=0;
            lpIme->lParam2=0;
            lpIme->lParam3=0;
            GlobalUnlock(m_hIME);
            m_pime->SendIMEMessageEx(this->GetSafeHwnd(),m_lIMEParam);
        }
        else
            GlobalUnlock(m_hIME);
    }
#endif //XP_WIN16
#endif //_IME_COMPOSITION
}

void CNetscapeEditView::OnKillFocus(CWnd *pOldWin)  
{
    MWContext * pMWContext=NULL;

    //TRACE1("CNetscapeView::KillFocusEdit hOldWin=%X\n", (pOldWin ? pOldWin->GetSafeHwnd() : NULL));
    if (  m_caret.bEnabled &&
        GetContext() && (NULL != (pMWContext = GET_MWCONTEXT)) &&
        !EDT_IsSelected(pMWContext) )
    {
        CPoint cPoint = GetCaretPos();
        m_caret.x = cPoint.x;
        m_caret.y = cPoint.y;

        DestroyCaret();
        m_caret.cShown = 0;
        m_caret.bEnabled = FALSE;
        //TRACE0("CNetscapeEditView::OnKillFocus: Caret destroyed\n");
    }
    
#ifdef _IME_COMPOSITION
    if (m_imebool)
    {    
        if (m_pime)
        {
    #ifdef XP_WIN32
            HIMC hIMC;
            if (hIMC = m_pime->ImmGetContext(this->m_hWnd))
            {
                m_pime->ImmNotifyIME(hIMC,NI_COMPOSITIONSTR,CPS_COMPLETE,NULL);
                m_pime->ImmReleaseContext(this->m_hWnd,hIMC);
            }
    #else //xp_win16
            if (!pOldWin||(pOldWin->m_hWnd!=GetParent()->m_hWnd))
            {
                if (!pMWContext)
                    pMWContext = GET_MWCONTEXT;
                if (pMWContext)
                {
                    EDT_SetInsertPointToOffset(pMWContext,m_imeoffset+m_imelength,0);
                    m_pchardata->mask= -1;
                    CLEARBIT(m_pchardata->values,TF_INLINEINPUT);//we are done!
                    EDT_SetCharacterDataAtOffset(pMWContext,m_pchardata,m_imeoffset,m_imelength);
                    m_imelength=0;
                    OnImeEndComposition();
                }
            }
    #endif //XP_WIN32 else xp_win16
        }
    }
#endif //_IME_COMPOSITION
    // CNetscapeView doens't need this?
    CNetscapeView::OnKillFocus(pOldWin);
}

BOOL CNetscapeEditView::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) 
{
    // was this a windows menu selection ?
    switch (nCode)
    {
    case CN_COMMAND:
        if (nID >= ID_EDITOR_PLUGINS_BASE && nID < ID_EDITOR_PLUGINS_BASE + MAX_EDITOR_PLUGINS)
        {
            uint32 CategoryId, PluginId;
            if (GetPluginInfo(nID, &CategoryId, &PluginId))
            {
                if (!EDT_PerformPlugin(GET_MWCONTEXT, CategoryId, PluginId, NULL, NULL))
                    MessageBox(szLoadString(IDS_ERR_LAUNCH_EDITOR_PLUGIN), NULL, MB_ICONSTOP);
                
                return TRUE;            // the message was handled here
            }
            ASSERT(FALSE);
        }
        else if(nID >= ID_EDIT_HISTORY_BASE && nID <= (ID_EDIT_HISTORY_BASE + MAX_EDIT_HISTORY_LOCATIONS))
        {
            char * pURL = NULL;
            if( EDT_GetEditHistory(GET_MWCONTEXT, nID-ID_EDIT_HISTORY_BASE, &pURL, NULL) )
            {
                FE_LoadUrl(pURL, LOAD_URL_COMPOSER);
                return TRUE;
            }
        }
        else if (nID == ID_STOP_EDITOR_PLUGIN)
        {
            EDT_StopPlugin(GET_MWCONTEXT);
            return TRUE;
        }
        // Dynamically-built font face menu items
        else if (nID >= ID_FORMAT_FONTFACE_BASE && nID < (ID_FORMAT_FONTFACE_BASE+MAX_TRUETYPE_FONTS+3))
        {
            // 0 = Variable Width, 1 = Fixed Width
            int iIndex = (nID == ID_FORMAT_FONTFACE_BASE+1) ? 1 : 0;
            char * pFace = NULL;

            if( nID >= ID_FORMAT_TRUETYPE_BASE){
                // The TrueType fonts start at index = 2 in font list
                pFace = wfe_ppTrueTypeFonts[nID - ID_FORMAT_FONTFACE_BASE-2];
            }
            // Change the font face
            EDT_SetFontFace(GET_MWCONTEXT, NULL, iIndex, pFace);
            // Trigger update of toolbar combobox
            m_EditState.bFontFaceMaybeChanged = TRUE;
            return TRUE;
        }
        else if(nID >= ID_FORMAT_PARAGRAPH_BASE && nID <= ID_FORMAT_PARAGRAPH_END)
        {
            TagType t = TagType(nID - ID_FORMAT_PARAGRAPH_BASE);
            // NOTE: P_UNUM_LIST and P_NUM_LIST should be handled here
            //       and remove separate message handlers, but not 
            //       done now (7/23/97) to avoid changing the ID 
            //       associated with these in res\editor.rc2
	        if( t == P_DIRECTORY ||
                t == P_MENU ||
                t == P_DESC_LIST ){
                EDT_ToggleList(GET_MWCONTEXT, t);
                return TRUE;
            }
            OnFormatParagraph(nID);
            return TRUE;
        }
        break;
    // TODO: Do this for these ranges as well:
    //ON_COMMAND_RANGE(ID_FORMAT_FONTSIZE_BASE, ID_FORMAT_FONTSIZE_BASE+MAX_FONT_SIZE, OnFontSize)
    //ON_COMMAND_RANGE(ID_FORMAT_FONTCOLOR_BASE, ID_FORMAT_FONTCOLOR_BASE+MAX_FONT_COLOR, OnFontColorMenu)

    case CN_UPDATE_COMMAND_UI:
        if (!pExtra)
        {
            ASSERT(FALSE);
            break;
        }        
        CCmdUI* pCmdUI = (CCmdUI*)pExtra;

        if (nID >= ID_EDITOR_PLUGINS_BASE && nID < ID_EDITOR_PLUGINS_BASE + MAX_EDITOR_PLUGINS)
        {
            pCmdUI->Enable(CAN_INTERACT && !EDT_IsPluginActive(GET_MWCONTEXT));
            return TRUE;
        }
        else if(nID >= ID_EDIT_HISTORY_BASE && nID <= (ID_EDIT_HISTORY_BASE + MAX_EDIT_HISTORY_LOCATIONS))
        {
            pCmdUI->Enable(GetContext()->CanOpenUrl());
            return TRUE;
        }
        else if (nID == ID_STOP_EDITOR_PLUGIN)
        {
            pCmdUI->Enable(EDT_IsPluginActive(GET_MWCONTEXT));
            return TRUE;
        }
        else if (IsEncodingMenu(nID))
        {
            // First let the frame class check-mark the current encoding menu entry.
            CMainFrame *pFrame = (CMainFrame *)GetParentFrame();
            pFrame->OnUpdateEncoding(pCmdUI);
            // Now conditionally enable the menu entry.
            pCmdUI->Enable(CAN_INTERACT);
            return TRUE;
        }
        // Dynamically-built font face menu items
        else if (nID >= ID_FORMAT_FONTFACE_BASE && nID < (ID_FORMAT_FONTFACE_BASE+MAX_TRUETYPE_FONTS+3))
        {
            // Hopefully, the cached index is up to date!
            pCmdUI->SetCheck(nID == (UINT)(m_EditState.iFontIndex + ID_FORMAT_FONTFACE_BASE));
            pCmdUI->Enable(CAN_INTERACT);
            return TRUE;
        }
        else if( nID > ID_FORMAT_PARAGRAPH_BASE ){
            TagType t = TagType(nID - ID_FORMAT_PARAGRAPH_BASE);
	        if( t == P_BLOCKQUOTE ||
                t == P_DIRECTORY ||
                t == P_MENU ||
                t == P_DESC_LIST){
                UpdateListMenuItem(pCmdUI, t);
                return TRUE;
            }
        }

        break;
    }

	return CNetscapeView::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

BOOL CNetscapeEditView::PreTranslateMessage(MSG * pMsg)
{
    if(	pMsg->message == WM_KEYDOWN )
    {
        // Process Ctrl+equals keydown -- for some bizarre reason,
        //  we don't get an OnKeyDown call for this message
        if( pMsg->wParam == 17 )
        {
            UpdateCursor();
        }
        else if( pMsg->wParam == 187 && (GetKeyState(VK_CONTROL) & 0x8000) )
        {
#ifdef XP_WIN32
            //this is for Swedish input of a '\'  in swedish that = <CTRL>=
            HKL hKL=GetKeyboardLayout(0);//0=active thread
            int32 langid=(int32)hKL;
            langid&=0xFFFF;//we need to zero out the high order word
            if (PRIMARYLANGID((int16)langid)!=LANG_ENGLISH)
                return CNetscapeView::PreTranslateMessage(pMsg);
#endif
            PostMessage(WM_COMMAND, ID_FORMAT_INDENT, 0);
            return TRUE;
        }
    } else if( pMsg->message == WM_KEYUP &&
               pMsg->wParam == 17 )
    {
        UpdateCursor();
    }
    return CNetscapeView::PreTranslateMessage(pMsg);
}

void CNetscapeEditView::UpdateCursor()
{
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(&pt);
    // Send mouse_move message using current 
    //   mouse location to trigger changing cursor
    SendMessage(WM_MOUSEMOVE, 0, MAKELPARAM(pt.x, pt.y));  
}

void CNetscapeEditView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    BOOL bShift = GetKeyState(VK_SHIFT) & 0x8000;
    BOOL bControl = GetKeyState(VK_CONTROL) & 0x8000;

    MWContext * pMWContext = GET_MWCONTEXT;
    if( pMWContext ){
        // Ignore keys if we can't interact
        if( pMWContext->waitingMode ||
            EDT_IsBlocked(pMWContext) ){
            return;
        }
        
        int iResult = EDT_COP_OK;
        switch(nChar) {
                // For some bizarre reason (or not?),
                //  we don't get a message on Ctrl+equals (nChar=187)
                // We must handle that in CNetscapeEditView::PreTranslateMessage
            case 189:   //  "-" (minus) key
                if( bControl){
                    OnFormatOutdent();
                }
                break;
            case 81:
                if( bControl) {
                    TRACE0("Ctrl-Q was pressed\n");
                } else {
                    TRACE0("Q was pressed\n");
                }
                break;
            case VK_ESCAPE:
                {
                // Cancel an Object Sizing operation
                if( EDT_IsSizing(pMWContext) )
                    GetContext()->CancelSizing();

                // Cancel copy style action
                if( EDT_CanPasteStyle(pMWContext) )
                {
                    EDT_PasteStyle(pMWContext, FALSE);
                    UpdateCursor();
                }

                // Continue to do other actions on Esc key
                break;
                }
            case VK_BACK:
                if( EDT_COP_OK != (iResult = CASTINT(EDT_DeletePreviousChar(pMWContext))) ){
                    ReportCopyError(iResult);
                }
                return;
            case VK_DELETE:
                if (bShift)
                    OnEditCut();

                else if( EDT_COP_OK != (iResult = CASTINT(EDT_DeleteChar(pMWContext))) ){
                    ReportCopyError(iResult);
                }
                return;

            case VK_RETURN:
                if (bShift)
                    OnInsertLineBreak();
                else if (!(GetKeyState(VK_MENU) & 0x8000))      // ALT+Enter is handled thru accelerators
                    EDT_ReturnKey(pMWContext);
                return;

            case VK_LEFT:
                if (OnLeftKey(bShift, bControl))
                    return;
                break;

            case VK_RIGHT:
                if (OnRightKey(bShift, bControl))
                    return;
                break;

            case VK_DOWN:
                if (OnDownKey(bShift, bControl))
                    return;
                break;

            case VK_UP:
                if (OnUpKey(bShift, bControl))
                    return;
                break;

            case VK_HOME:
                if (OnHomeKey(bShift, bControl))
                    return;
                break;

            case VK_TAB:
                // 2nd param: TRUE = Move Forward, FALSE = back, (ONLY WHEN IN A TABLE)
                // 3rd param: TRUE = Force tab to be spaces at all times, even in table
                if( EDT_COP_OK != (iResult = EDT_TabKey(pMWContext, !bShift, bControl)) ){
                    ReportCopyError(iResult);
                }
                return;

            case VK_END:
                if (OnEndKey(bShift, bControl))
                    return;
                break;

            case VK_INSERT:
                if (OnInsertKey(bShift, bControl))
                    return;
                break;

            case ' ':
                CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
                return;
        }
    }
    CNetscapeView::OnKeyDown(nChar, nRepCnt, nFlags);
}



void CNetscapeEditView::OnChar(UINT nChar, UINT nRepCnt, UINT nflags)
{
    // Ignore keys if we can't interact
    if( !CAN_INTERACT ){
        return;
    }
    BOOL bShift = GetKeyState(VK_SHIFT) & 0x8000;
    BOOL bControl = GetKeyState(VK_CONTROL) & 0x8000;

    MWContext * pMWContext = GET_MWCONTEXT;

    EDT_ClearTableAndCellSelection(pMWContext);

    // Ignore keys if we can't interact
    INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(pMWContext);
    int16 win_csid = INTL_GetCSIWinCSID(csi);
    
    // Ignore keys if we can't interact
    int iResult = EDT_COP_SELECTION_CROSSES_TABLE_DATA_CELL;
        // Since any key can replace a selection,
        //   we need to test for this for any key!
    switch (nChar) {
        case 22: // Ctrl+V
            OnEditPaste();
            return;
        case ' ':
        case VK_UP:
        case VK_DOWN:
        case VK_LEFT:
        case VK_RIGHT:
//            TRACE("Cursor key %x\n", nChar);
            break;
    }
#ifdef XP_WIN32
    if ((win_csid==CS_UTF8)||(win_csid==CS_UTF7))
    {
        if (nChar >=' ')//do not pass back control characters. does not get filtered like keydown does.
        {
            char characterCode[2];
            characterCode[0] = (char) nChar;
            characterCode[1] = '\0';
            unsigned char* t_unicodestring= INTL_ConvertLineWithoutAutoDetect(m_csid,win_csid,(unsigned char *)characterCode,1);
            if(t_unicodestring)
            {
                EDT_InsertText( pMWContext, (char *)t_unicodestring ); 
                XP_FREE(t_unicodestring);
            }
        }
    }
    else
    {
#endif //XP_WIN32
        if( EDT_COP_OK != (iResult = CASTINT(EDT_KeyDown( GET_MWCONTEXT, nChar, nRepCnt, nflags ))) ){
          ReportCopyError(iResult);
        }
#ifdef XP_WIN32
    }
#endif
}

BOOL CNetscapeEditView::OnLeftKey(BOOL bShift, BOOL bControl)
{
    if (bShift && bControl)
        OnSelectPreviousWord();
    else if (bShift)
        OnSelectPreviousChar();
    else if (bControl)
        OnPreviousWord();
    else
        OnPreviousChar();

    return TRUE;
}

BOOL CNetscapeEditView::OnRightKey(BOOL bShift, BOOL bControl)
{
    if (bShift && bControl)
        OnSelectNextWord();
    else if (bShift)
        OnSelectNextChar();
    else if (bControl)
        OnNextWord();
    else
        OnNextChar();

    return TRUE;
}

BOOL CNetscapeEditView::OnDownKey(BOOL bShift, BOOL bControl)
{
    if (bShift && bControl)
        return FALSE;                     
    else if (bShift)
        OnSelectDown();
    else if (bControl)
        OnNextParagraph();                     
    else
        OnDown();

    return TRUE;
}

BOOL CNetscapeEditView::OnUpKey(BOOL bShift, BOOL bControl)
{
    if (bShift && bControl)
        return FALSE;
    else if (bShift)
        OnSelectUp();
    else if (bControl)
        OnPreviousParagraph();                     
    else
        OnUp();

    return TRUE;
}

BOOL CNetscapeEditView::OnHomeKey(BOOL bShift, BOOL bControl)
{
    if (bShift && bControl)
        OnSelectBeginOfDocument();
    else if (bShift)
        OnSelectBeginOfLine();
    else if (bControl)
        EDT_BeginOfDocument(GET_MWCONTEXT, FALSE);
    else
        OnBeginOfLine();

    return TRUE;
}

BOOL CNetscapeEditView::OnEndKey(BOOL bShift, BOOL bControl)
{
    if (bShift && bControl)
        OnSelectEndOfDocument();
    else if (bShift)
        OnSelectEndOfLine();
    else if (bControl)
        EDT_EndOfDocument(GET_MWCONTEXT, FALSE);
    else
        OnEndOfLine();

    return TRUE;
}

BOOL CNetscapeEditView::OnInsertKey(BOOL bShift, BOOL bControl)
{
    if (bShift && bControl)
        return FALSE;
    else if (bShift)
        OnEditPaste();
    else if (bControl)
        OnEditCopy();                  
    else
        return FALSE;

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// Local function to terminate a string at first new-line character
// Bookmark strings (clipboard and drop formats) all need this
void wfe_TerminateStringAtNewline( char * pString )
{
    char * pCR = strstr( pString, "\n" );
    if ( pCR ) {
        *pCR = '\0';
    }
}

// Convert screen (e.g., mouse) point to doc coordinates
void CNetscapeEditView::ClientToDocXY( CPoint& cPoint, int32* pX, int32* pY )
{
    CWinCX *  pContext = GetContext();
    if ( !pContext)
        return;
    XY Point;
    pContext->ResolvePoint(Point, cPoint);

    *pX = Point.x;
    *pY = Point.y;
}

void ImageHack( MWContext *pContext ){
    if( EDT_GetCurrentElementType(pContext) == ED_ELEMENT_IMAGE ) {
        EDT_ImageData *pId = EDT_GetImageData(pContext);
        if( pId ){
            
            #if 0
            if( pId->pWidth ){
                XP_FREE( pId->pWidth );
                pId->pWidth = 0;
            }
            else {
                pId->pWidth = XP_STRDUP( "50%" );
            }
            #endif

            pId->align = (ED_Alignment)(pId->align+1);
            if( (int)pId->align == 9 ){
                pId->align = (ED_Alignment)0;
            }

			int bKeepImages;
			PREF_GetBoolPref("editor.publish_keep_images",&bKeepImages);
            EDT_SetImageData( pContext, pId, bKeepImages );
            EDT_FreeImageData( pId );
        }
    }
}

void PageHack( MWContext *pContext ){
    EDT_PageData *pPageData = EDT_GetPageData( pContext );
    if( pPageData->pColorBackground ) XP_FREE( pPageData->pColorBackground );
    pPageData->pColorBackground = XP_NEW( LO_Color );

    pPageData->pColorBackground->red = 0xff;
    pPageData->pColorBackground->green = 0xff;
    pPageData->pColorBackground->blue = 0x80;

    char *p = pPageData->pTitle;
    while( p && *p ){
        *p = toupper( *p );
        p++;
    }

    if( pPageData->pBackgroundImage != 0 ){
        pPageData->pBackgroundImage = 0;
    }
    else {
        pPageData->pBackgroundImage = XP_STRDUP("grunt.gif");
    }

    EDT_SetPageData( pContext, pPageData );
    EDT_FreePageData( pPageData );
}

     
void HRuleHack( MWContext *pContext ){
    if( EDT_GetCurrentElementType(pContext) == ED_ELEMENT_HRULE ) {
        EDT_HorizRuleData *pId = EDT_GetHorizRuleData(pContext);
        if( pId ){
            
            #if 0
            if( pId->pWidth ){
                XP_FREE( pId->pWidth );
                pId->pWidth = 0;
            }
            else {
                pId->pWidth = XP_STRDUP( "50%" );
            }
            #endif

            pId->size = (pId->size + 10 % 100 );
            pId->bNoShade = TRUE;

            EDT_SetHorizRuleData( pContext, pId );
            EDT_FreeHorizRuleData( pId );
        }
    }
}

void CNetscapeEditView::OnNextParagraph()
{
    if ( GetContext() && GET_MWCONTEXT) {
        //EDT_Down(GET_MWCONTEXT);  TODO: IMPLEMENT OnNextParagraph and OnPreviousParagraph
    }
}

void CNetscapeEditView::OnPreviousParagraph()
{
    if ( GetContext() && GET_MWCONTEXT) {
        //EDT_Up(GET_MWCONTEXT);
    }
}

void CNetscapeEditView::OnUp()
{
    if ( GetContext() && GET_MWCONTEXT ) {
        EDT_Up( GET_MWCONTEXT, FALSE );
    }
}

void CNetscapeEditView::OnDown()
{
    if ( GetContext() && GET_MWCONTEXT ) {
        EDT_Down( GET_MWCONTEXT, FALSE );
    }
}

void CNetscapeEditView::OnNextChar()
{
    if ( GetContext() && GET_MWCONTEXT ) {
        EDT_NextChar( GET_MWCONTEXT, FALSE );
    }
}

void CNetscapeEditView::OnPreviousChar()
{
    if ( GetContext() && GET_MWCONTEXT ) {
        EDT_PreviousChar( GET_MWCONTEXT, FALSE );
    }
}

void CNetscapeEditView::OnBeginOfLine()
{
    if ( GetContext() && GET_MWCONTEXT ) {
        EDT_BeginOfLine( GET_MWCONTEXT, FALSE );
    }
}

void CNetscapeEditView::OnEndOfLine()
{
    if ( GetContext() && GET_MWCONTEXT ) {
        EDT_EndOfLine( GET_MWCONTEXT, FALSE );
    }
}

void CNetscapeEditView::OnPageUp()
{
    if ( GetContext() && GET_MWCONTEXT ) {
        EDT_PageUp( GET_MWCONTEXT, FALSE );
    }
}

void CNetscapeEditView::OnPageDown()
{
    if ( GetContext() && GET_MWCONTEXT ) {
        EDT_PageDown( GET_MWCONTEXT, FALSE );
    }
}

void CNetscapeEditView::OnNextWord()
{
    if ( GetContext() && GET_MWCONTEXT) {
        EDT_NextWord(GET_MWCONTEXT, FALSE);
    }
}

void CNetscapeEditView::OnPreviousWord()
{
    if ( GetContext() && GET_MWCONTEXT) {
       EDT_PreviousWord(GET_MWCONTEXT, FALSE);
    }
}

void CNetscapeEditView::OnSelectUp()
{
    if ( GetContext() && GET_MWCONTEXT ) {
        EDT_Up( GET_MWCONTEXT, TRUE );
    }
}

void CNetscapeEditView::OnSelectDown()
{
    if ( GetContext() && GET_MWCONTEXT ) {
        EDT_Down( GET_MWCONTEXT, TRUE );
    }
}

void CNetscapeEditView::OnSelectNextChar()
{
    if ( GetContext() && GET_MWCONTEXT ) {
        EDT_NextChar( GET_MWCONTEXT, TRUE );
    }
}

void CNetscapeEditView::OnSelectPreviousChar()
{
    if ( GetContext() && GET_MWCONTEXT ) {
        EDT_PreviousChar( GET_MWCONTEXT, TRUE );
    }
}

void CNetscapeEditView::OnSelectBeginOfLine()
{
    if ( GetContext() && GET_MWCONTEXT ) {
        EDT_BeginOfLine( GET_MWCONTEXT, TRUE );
    }
}

void CNetscapeEditView::OnSelectEndOfLine()
{
    if ( GetContext() && GET_MWCONTEXT ) {
        EDT_EndOfLine( GET_MWCONTEXT, TRUE );
    }
}

void CNetscapeEditView::OnSelectPageUp()
{
    if ( GetContext() && GET_MWCONTEXT ) {
        EDT_PageUp( GET_MWCONTEXT, TRUE );
    }
}

void CNetscapeEditView::OnSelectPageDown()
{
    if ( GetContext() && GET_MWCONTEXT ) {
        EDT_PageDown( GET_MWCONTEXT, TRUE );
    }
}

void CNetscapeEditView::OnSelectBeginOfDocument()
{
    if ( GetContext() && GET_MWCONTEXT ) {
        EDT_BeginOfDocument( GET_MWCONTEXT, TRUE );
    }
}

void CNetscapeEditView::OnSelectEndOfDocument()
{
    if ( GetContext() && GET_MWCONTEXT ) {
        EDT_EndOfDocument( GET_MWCONTEXT, TRUE );
    }
}

void CNetscapeEditView::OnSelectNextWord()
{
    if ( GetContext() && GET_MWCONTEXT) {
        EDT_NextWord(GET_MWCONTEXT, TRUE );
    }
}

void CNetscapeEditView::OnSelectPreviousWord()
{
    if ( GetContext() && GET_MWCONTEXT) {
        EDT_PreviousWord(GET_MWCONTEXT, TRUE);
    }
}

//
// Global FE functions so it is accessible from anyone who needs it

BOOL FE_ResolveLinkURL( MWContext* pMWContext, CString& csURL, BOOL bAutoAdjustLinks )
{
    csURL.TrimLeft();
    csURL.TrimRight();

    // Empty is OK - this is how we remove links
    if ( csURL.IsEmpty() ){
        return TRUE;
    }
    // Don't try to make relative to file://Untitled.
    if (EDT_IS_NEW_DOCUMENT(pMWContext)){
      return TRUE;
    }
          
    // Base URL is the address of current document
    History_entry * hist_ent = SHIST_GetCurrent(&(pMWContext->hist));
    if ( hist_ent == NULL || hist_ent->address == NULL ){
        return FALSE;
    }
    // Be sure we are URL format first
    CString csTemp;
    WFE_ConvertFile2Url(csTemp, csURL);
    char *szRelativeURL = NULL;
    int Result = NET_MakeRelativeURL( hist_ent->address,
                                      (char*)(LPCSTR(csTemp)), // Input URL
                                      &szRelativeURL);           // Result put here
    switch( Result ){
        case NET_URL_SAME_DIRECTORY:
            TRACE0("NET_URL_SAME_DIRECTORY\n");
            break;
        case NET_URL_SAME_DEVICE:
            TRACE0("NET_URL_SAME_DEVICE\n");
            break;
        case NET_URL_NOT_ON_SAME_DEVICE:
            // Warn user if they try to make a link to a different local device
            // (HTTP: URLs are OK)
            if ( bAutoAdjustLinks && NET_IsLocalFileURL(szRelativeURL) ){
                // Start message with a size-limited version of URL
                CString csMsg = szRelativeURL;
                WFE_CondenseURL(csMsg, 50, FALSE);
                // add rest of message from resources
                csMsg += szLoadString(IDS_ERR_DIF_LOCAL_DEVICE_LINK);

                if ( IDNO == ::MessageBox( PANECX(pMWContext)->GetPane(),
                                         LPCSTR(csMsg),
                                         szLoadString(IDS_RESOLVE_LINK_URL),
                                         MB_YESNO | MB_ICONQUESTION ) ){
                    // Cancel using string
                    XP_FREE( szRelativeURL );
                    szRelativeURL = NULL;
                }
            }
            break;
        case NET_URL_FAIL:                  // Failed for lack of params, etc.
            TRACE0("NET_URL_FAIL\n");
            break;
    }

    if ( szRelativeURL ){
        csURL = szRelativeURL;
        XP_FREE( szRelativeURL );
        return TRUE;
    }
    return FALSE;
}

void CNetscapeEditView::OnGoToDefaultPublishLocation()
{
    // Get editor app from preferences or prompt user to edit preferences        
    CString csBrowseLocation = 
        ((CEditFrame*)GetFrame())->GetLocationFromPreferences("editor.publish_browse_location",
                                          IDS_BROWSETO_PROMPT,
                                          IDS_BROWSETO_CAPTION,
                                          IDS_SELECT_BROWSETO_CAPTION);
    if(!csBrowseLocation.IsEmpty()){
        GetContext()->NormalGetUrl((char*)LPCSTR(csBrowseLocation));
    }
}

void CNetscapeEditView::OnEditCopy()
{
    CWnd * pWnd = GetFocus();
    if( pWnd != this ){
        // If not in editor view, probably in Address
        //   area, so send message there
        if( pWnd )
            pWnd->PostMessage(WM_COPY,0,0);
        return;
    }
    HANDLE h;
    HANDLE hData;
    XP_HUGE_CHAR_PTR string;
    char * text;
    int iResult;
    CWinCX * pContext = GetContext();
    if(!pContext)
      return;

    MWContext* pMWContext = pContext->GetContext();
    
    // TODO: CHECK FOR INDIVIDUALLY-SELECTED IMAGE AND COPY USING EDT_CopyImageData struct

    int32 textLen, hLen;
    XP_HUGE_CHAR_PTR htmlData = 0;
    SetCursor(theApp.LoadStandardCursor(IDC_WAIT));
    if( EDT_COP_OK != (iResult = CASTINT(EDT_CopySelection(pMWContext, &text, &textLen, &htmlData, &hLen))) ){
        ReportCopyError(iResult);
        return;
    }
    SetCursor(theApp.LoadStandardCursor(IDC_ARROW));
    if(!text)
      return;

    if(!CWnd::OpenClipboard()) {
        FE_Alert(pMWContext, szLoadString(IDS_OPEN_CLIPBOARD));
        return;
    }

    if(!EmptyClipboard()) {
        FE_Alert(pMWContext, szLoadString(IDS_EMPTY_CLIPBOARD));
        return;
    }
                  
    int32 len = XP_STRLEN(text) + 1;
    
    hData = GlobalAlloc(GMEM_MOVEABLE, (DWORD) len);
    string = (char *) GlobalLock(hData);
    XP_MEMCPY( string, text, CASTSIZE_T(len) );
    string[len - 1] = '\0';
    GlobalUnlock(hData);

    h = SetClipboardData(CF_TEXT, hData); 

#ifdef XP_WIN32
	int datacsid = INTL_GetCSIWinCSID(
						LO_GetDocumentCharacterSetInfo(pMWContext))
							& ~CS_AUTO;

	if((CS_USER_DEFINED_ENCODING != datacsid) && (0 != datacsid))
	{
		len = CASTINT((INTL_StrToUnicodeLen(datacsid, (unsigned char*)text)+1) * 2);
		hData = GlobalAlloc(GMEM_MOVEABLE, (DWORD) len);
		string = (char *) GlobalLock(hData);
		INTL_StrToUnicode(datacsid, (unsigned char*)text, (INTL_Unicode*)string, len);

		GlobalUnlock(hData);		
	}
	h = ::SetClipboardData(CF_UNICODETEXT, hData); 

#endif

    if( htmlData ){
        hData = GlobalAlloc(GMEM_MOVEABLE, (DWORD) hLen);
        string = (char *) GlobalLock(hData);
        XP_HUGE_MEMCPY(string, htmlData, hLen);
        GlobalUnlock(hData);
        h = SetClipboardData(m_cfEditorFormat, hData); 
    }
    XP_HUGE_FREE( htmlData );


    CloseClipboard();
    XP_FREE(text);
    BOOL bCanPaste = (BOOL)EDT_CanPaste(pContext->GetContext(), TRUE);
    TRACE1("Can paste=%d\n", bCanPaste);
}     

void CNetscapeEditView::ReportCopyError(int iError)
{
    SetCursor(theApp.LoadStandardCursor(IDC_ARROW));
    UINT nIDS = 0;
    switch(iError) {
        case EDT_COP_DOCUMENT_BUSY:
            nIDS = IDS_DOCUMENT_BUSY;
            break;
        //TODO: Maybe don't do this message either?
        // This doesn't occur as often with new table selection model
        case EDT_COP_SELECTION_CROSSES_TABLE_DATA_CELL:
            nIDS = IDS_SELECTION_CROSSES_TABLE_DATA_CELL;
            break;
    }
    // Trap case where error tells us its a table error,
    //  but really there's nothing selected, thus nothing to delete
    if( nIDS == IDS_SELECTION_CROSSES_TABLE_DATA_CELL && 
        !EDT_IsSelected(GET_MWCONTEXT) ) {
        nIDS = 0; //IDS_NOTHING_TO_DELETE; Don't do message if just no selection
    }

    if( nIDS ){
        MessageBox( szLoadString(nIDS),
                    szLoadString(IDS_COPY_ERROR_CAPTION), 
                    MB_OK | MB_ICONINFORMATION );
    }
}

void CNetscapeEditView::OnEditCut()
{
    CWnd * pWnd = GetFocus();
    if( pWnd != this ){
        // If not in editor view, probably in Address
        //   area, so send message there
        if( pWnd )
            pWnd->PostMessage(WM_CUT,0,0);
        return;
    }
    HANDLE h;
    HANDLE hData;
    char * string;
    char * text;
    CWinCX *     pContext = GetContext();
    if ( !pContext ) {
        return;
    }
    MWContext  * pMWContext = pContext->GetContext();
    int iResult;
    
    // Don't bother if no selection
    if(!pMWContext  ||
       !m_bIsEditor )
    {
        return;
    }

    int32 textLen, hLen;
    XP_HUGE_CHAR_PTR htmlData = 0;
    SetCursor(theApp.LoadStandardCursor(IDC_WAIT));
    if( EDT_COP_OK != (iResult = CASTINT(EDT_CutSelection(pMWContext, &text, &textLen, &htmlData, &hLen)) )||
        !text ) {
        ReportCopyError(iResult);
        return;
    }
    SetCursor(theApp.LoadStandardCursor(IDC_ARROW));


    if(!CWnd::OpenClipboard()) {
       FE_Alert(pMWContext, szLoadString(IDS_OPEN_CLIPBOARD));
        return;
    }

    if(!EmptyClipboard()) {
        FE_Alert(pMWContext, szLoadString(IDS_EMPTY_CLIPBOARD));
        return;
    }
                  
    int len = XP_STRLEN(text) + 1;
    
    hData = GlobalAlloc(GMEM_MOVEABLE, (DWORD) len);
    string = (char *) GlobalLock(hData);
    strncpy(string, text, len);
    string[len - 1] = '\0';
    GlobalUnlock(hData);

    h = SetClipboardData(CF_TEXT, hData); 

#ifdef XP_WIN32
	int datacsid = INTL_GetCSIWinCSID(
						LO_GetDocumentCharacterSetInfo(GetContext()->GetContext()))
							& ~CS_AUTO;

	if((CS_USER_DEFINED_ENCODING != datacsid) && (0 != datacsid))
	{
		len = CASTINT((INTL_StrToUnicodeLen(datacsid, (unsigned char*)text)+1) * 2);
		hData = GlobalAlloc(GMEM_MOVEABLE, (DWORD) len);
		string = (char *) GlobalLock(hData);
		INTL_StrToUnicode(datacsid, (unsigned char*)text, (INTL_Unicode*)string, len);

		GlobalUnlock(hData);		
	}
	h = ::SetClipboardData(CF_UNICODETEXT, hData); 

#endif

    if( htmlData ){
        hData = GlobalAlloc(GMEM_MOVEABLE, (DWORD) hLen);
        string = (char *) GlobalLock(hData);
        XP_HUGE_MEMCPY(string, htmlData, hLen);
        GlobalUnlock(hData);
        h = SetClipboardData(m_cfEditorFormat, hData); 
    }
    XP_HUGE_FREE( htmlData );

    CloseClipboard();
    XP_FREE(text);
}     

void CNetscapeEditView::OnEditDelete()
{
    if ( GET_MWCONTEXT != NULL ) {
        int iResult;
        SetCursor(theApp.LoadStandardCursor(IDC_WAIT));
        if( EDT_COP_OK == (iResult = CASTINT(EDT_DeleteChar(GET_MWCONTEXT))) ){
        } else {
            ReportCopyError(iResult);
        }
    }
}

void CNetscapeEditView::OnCopyStyle()
{
    EDT_CopyStyle(GET_MWCONTEXT);
}

void CNetscapeEditView::OnUpdateCopyStyle(CCmdUI* pCmdUI)
{
    //TODO: CHECK FOR TEXT ELEMENT?
    pCmdUI->Enable(CAN_INTERACT);
}

// Use this for any commands that are enabled most of the time
void CNetscapeEditView::OnCanInteract(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(CAN_INTERACT);
}

// Gets data from bookmark item - returns TRUE if found
BOOL wfe_GetBookmarkData( COleDataObject* pDataObject, char ** ppURL, char ** ppTitle ) {
    HGLOBAL h = pDataObject->GetGlobalData(RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT));
    
    if ( h == NULL || ppTitle == NULL || ppURL == NULL ){
        return FALSE;
    }

    LPBOOKMARKITEM pBookmark = (LPBOOKMARKITEM)GlobalLock(h);
    if (!pBookmark){
        GlobalUnlock(h);
        return FALSE;
    }
    *ppTitle = XP_STRDUP(pBookmark->szText);
    *ppURL = XP_STRDUP(pBookmark->szAnchor);
    GlobalUnlock(h);
    return TRUE;
}

// Common Paste handler for Clipboard or drag/drop
BOOL CNetscapeEditView::DoPasteItem(COleDataObject* pDataObject, 
                                CPoint *pPoint, BOOL bDeleteSource )
{
    CWinCX *     pContext = GetContext();
    if ( !pContext ) {
        return(FALSE);    
    }
    MWContext  * pMWContext = pContext->GetContext();
    // Assume drop data is from dragged object
    BOOL bDropObject = TRUE;
    
    if(!pMWContext ) {
        return(FALSE);
    }

	// use clipboard data if not doing drag/drop
	COleDataObject clipboardData;
	if (pDataObject == NULL)
	{
		if ( ! clipboardData.AttachClipboard() ){
            return FALSE;
        }		
		pDataObject = &clipboardData;
        // No data on clipboard?
        if (
#ifdef XP_WIN32
                 !pDataObject->m_bClipboard ||
#endif
                 -1 == GetPriorityClipboardFormat(m_pClipboardFormats, MAX_CLIPBOARD_FORMATS )) {
            return FALSE;
        }
        bDropObject = FALSE;
	}

    CPoint cDocPoint;
    int32  xVal, yVal;
    BOOL bPtInSelection = FALSE;

    if ( pPoint == NULL )
    {
        // Use last caret location if no point supplied
        cDocPoint.x = m_caret.x;
        cDocPoint.y = m_caret.y;
        pPoint = &cDocPoint;
    } else {
        cDocPoint = *pPoint;
    }

    // Convert everything to Document coordinates
    ClientToDocXY( cDocPoint, &xVal, &yVal );
    cDocPoint.x = CASTINT(xVal);
    cDocPoint.y = CASTINT(yVal);
    
    // We are doing Drag/Drop if we have an object
    if( bDropObject ){
        bPtInSelection = GetContext()->PtInSelectedRegion(cDocPoint);
    }

    // Don't do anything if drag source is selected region and
    //   drop target is same.
    if ( GetContext()->IsDragging() && bPtInSelection ) {
        return(FALSE);
    }
 
    HGLOBAL hString = NULL;
    char * pString = NULL;

    // Get any string data
    if ( pDataObject->IsDataAvailable(CF_TEXT) ) {
        hString = pDataObject->GetGlobalData(CF_TEXT);

        // get a pointer to the actual bytes
        if ( hString ) {
            pString = (char *) GlobalLock(hString);    
        }
    }

    if ( m_bIsEditor )
    {
        CWinCX    * pContext = GetContext();
        MWContext * pMWContext = pContext->GetContext();
        HGLOBAL     h;
        char        *pURL;
        char        *pTitle;
        SetCursor(theApp.LoadStandardCursor(IDC_WAIT));

        EDT_BeginBatchChanges(pMWContext);

        if( bDeleteSource ){
            // This deletes current selection and sets
            //  cursor at point where moved data will be inserted.
            EDT_DeleteSelectionAndPositionCaret( pMWContext, xVal, yVal );
        } else if( !bPtInSelection && bDropObject ) {
            // Remove existing selection and set new caret position
            //   to the drop ONLY if doing a drop NOT within the selection
            EDT_PositionCaret( pMWContext, xVal, yVal );
        }

        // Drop according to type of data
        if( pDataObject->IsDataAvailable(m_cfBookmarkFormat) &&
            wfe_GetBookmarkData(pDataObject, &pURL, &pTitle) ){
            // Create new link: insert both Anchor text and HREF,
            // First resolve (convert to relative) URL
            CString csURL = pURL;
            char *pTitleText = pTitle;
			int bKeepLinks;
			PREF_GetBoolPref("editor.publish_keep_links",&bKeepLinks);

            if ( FE_ResolveLinkURL(pMWContext, csURL,bKeepLinks) ){
                // Check for ugly format from Livewire Sitemanager:
                if( 0 == _strnicmp(pTitle, "File: file://", 13) ){
                    // Skip over redundant "File: "
                    pTitleText += 6;
                }
                // Bug in Sitemanager: treats graphic files that are not
                //  "managed" as a link. Convert to image 
                char * pExt = strrchr( pURL, '.');
                if( CanSupportImageFile(pExt) ){
                    EDT_ImageData* pEdtData = EDT_NewImageData();
                    if( pEdtData ){
                        // We just need to set the URL
                        pEdtData->pSrc = pURL;

						int bKeepImages;
						PREF_GetBoolPref("editor.publish_keep_images",&bKeepImages);

                        EDT_InsertImage(pMWContext, pEdtData,bKeepImages);
                        EDT_FreeImageData(pEdtData);
                        // String was just freed, so prevent double free
                        pURL = NULL;
                    }
                } else {
                    char * pRelative = (char*)LPCSTR(csURL);
                    if( bPtInSelection ){
                        // Make selected text/image a link to dropped item
                        EDT_SetHREF( pMWContext, pRelative );
                    } else {
                        // Insert a new link, including link (Title) text
                        EDT_PasteHREF( pMWContext, &pRelative, &pTitleText, 1 );
                    }
                }
            }
            if( pURL) XP_FREE(pURL);
            XP_FREE(pTitle);
        } else if ( pDataObject->IsDataAvailable(m_cfEditorFormat) ) {
            h = pDataObject->GetGlobalData(m_cfEditorFormat);
            if( h ){
                char * pHTML = (char*) GlobalLock( h );
                EDT_PasteHTML( pMWContext, pHTML );
                GlobalUnlock( h );
            }
        } 
#ifdef EDITOR
        else if(pDataObject->IsDataAvailable(m_cfImageFormat) ) {
            h = pDataObject->GetGlobalData(m_cfImageFormat);
            WFE_DragDropImage(h, pMWContext);
        } 
#endif // EDITOR
#ifdef XP_WIN32
        else if( pDataObject->IsDataAvailable(CF_HDROP) ){
            HDROP handle = (HDROP)pDataObject->GetGlobalData(CF_HDROP);
            if( handle ){
                // Do same thing we would do for Win3.x and NT
                //  (old-style file-manager-drop)
                // FALSE = caret is already positioned correctly
                DropFiles(handle, FALSE);
            }
        // **** Test for other formats here
        } 
        else if ( pDataObject->IsDataAvailable(CF_UNICODETEXT) && 
					(CS_USER_DEFINED_ENCODING != INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(pMWContext)))
				  )	{	// Let's try CF_UNICODETEXT before CF_TEXT
			
			int datacsid = 
				INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(pMWContext)) & ~CS_AUTO;
			HGLOBAL hUnicodeStr = NULL;
			char * pUnicodeString = NULL;
		    char * pConvertedString = NULL;

			// Get any string data
			if ( pDataObject->IsDataAvailable(CF_UNICODETEXT) ) {
				hUnicodeStr = pDataObject->GetGlobalData(CF_UNICODETEXT);

				// get a pointer to the actual bytes
				if ( hUnicodeStr ) {
					pUnicodeString = (char *) GlobalLock(hUnicodeStr);    

					// Now, let's convert the Unicode text into the datacsid encoding
					int ucs2len = CASTINT(INTL_UnicodeLen((INTL_Unicode*)pUnicodeString));
					int	mbbufneeded = CASTINT(INTL_UnicodeToStrLen(datacsid, 
																	(INTL_Unicode*)pUnicodeString, 
																	ucs2len));
					if(NULL != (pConvertedString = (char*)XP_ALLOC(mbbufneeded + 1)))
					{
						INTL_UnicodeToStr(datacsid, (INTL_Unicode*)pUnicodeString, ucs2len, 
													(unsigned char*) pConvertedString, mbbufneeded + 1);
						
						EDT_PasteText( pMWContext, pConvertedString ); 
						XP_FREE(pConvertedString);
					}
					GlobalUnlock(hUnicodeStr);    
				}
			}
        }
#endif //XP_WIN32
        else if ( pDataObject->IsDataAvailable(CF_TEXT) ) {
            if( pString ) {
                // *** TODO: Analyze string:
                // Check if its a valid local filename (use XP_STAT). If yes, pop-up menu:
                //    1. If over selected text: Create a link to 
                //         Pop-up menu: Create Link or paste filename or load file
                //    2. If over an existing link:
                //         Pop-up menu: Modify Link or load file or 
                //    3. In ordinary text:
                //         Popup:  Insert Link or paste filename or load file
                
                EDT_PasteText( pMWContext, pString ); 
            }
        }
#ifdef EDITOR
#ifdef _IMAGE_CONVERT
        else if( pDataObject->IsDataAvailable(CF_DIB) ) {
            CONVERT_IMGCONTEXT imageContext;
            CONVERT_IMG_INFO imageInfo;
            memset(&imageContext,0,sizeof(CONVERT_IMGCONTEXT));
            HBITMAP handle = (HBITMAP)pDataObject->GetGlobalData(CF_DIB);
            if (handle)
            {
                imageContext.m_stream.m_type=CONVERT_MEMORY;
                imageContext.m_stream.m_mem=(XP_HUGE_CHAR_PTR)GlobalLock(handle);
                if (imageContext.m_stream.m_mem)
                {
                    imageContext.m_imagetype=conv_bmp;
                    imageContext.m_callbacks.m_dialogimagecallback=FE_ImageConvertDialog;
                    imageContext.m_callbacks.m_displaybuffercallback=FE_ImageConvertDisplayBuffer;
                    imageContext.m_callbacks.m_completecallback=FE_ImageDoneCallBack;
                    imageContext.m_pMWContext=pMWContext;
                    imageContext.m_parentwindow=(void *)this;
                    CONVERT_IMAGERESULT result=convert_stream2image(imageContext,&imageInfo,1,NULL);//1 for 1 output
                    GlobalUnlock(handle);
                    if (result>CONV_OK)//not cancel or ok
                    {
                        AfxMessageBox(m_converrmsg[result]);
                    }
                }
            }
        } 
#endif //_IMAGE_CONVERT
#endif // EDITOR
    } else if ( pString) {              // Navigator window - assume string is a URL
        char * szURL = XP_STRDUP(pString);
        // Maybe check for valid local filename here?
	    if ( NET_URL_Type != 0 ) {
	        GetContext()->NormalGetUrl(szURL);
        }
        XP_FREE(szURL);
    }

    if ( hString ) {
        GlobalUnlock(hString);
    }
    
    if ( m_bIsEditor )
        EDT_EndBatchChanges(pMWContext);

    SetCursor(theApp.LoadStandardCursor(IDC_ARROW));

    return(TRUE);
}

void CNetscapeEditView::OnEditPaste(){

    CWnd * pWnd = GetFocus();
    if( pWnd != this ){
        // If not in editor view, probably in Address
        //   area, so send message there
        if( pWnd )
            pWnd->PostMessage(WM_PASTE,0,0);
        return;
    }

    // NULL params tell it to use clipboard for data source
    //  and current caret position.
    CWinCX *pContext = GetContext();
    if ( m_bIsEditor && pContext
        && EDT_COP_OK != EDT_CanPaste(pContext->GetContext(), TRUE) )
        return;
    DoPasteItem(NULL, NULL, FALSE );
}

BOOL UpdateCanCopyInEditControl(CWnd *pView, CCmdUI* pCmdUI)
{
    CWnd *pWnd = pView->GetFocus();
    // We are in the view - check for selection in edit buffer instead
    if( pView == pWnd )
        return FALSE;

    // If control with focus is CEdit, then check if there is a selection
    if( pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CEdit)) ){
        // We have a selection if start and end are different.
        DWORD dwStart, dwEnd;
    	pWnd->SendMessage(EM_GETSEL, (WPARAM)(LPDWORD)&dwStart, 
			              (LPARAM)(LPDWORD)&dwEnd);
        pCmdUI->Enable(dwStart != dwEnd);
    } else {
        pCmdUI->Enable(FALSE);
    }
    return TRUE;
}

void CNetscapeEditView::OnUpdateEditCut(CCmdUI* pCmdUI)
{
    if( UpdateCanCopyInEditControl(this, pCmdUI) )
        return;

    CWinCX *pContext = GetContext();
    if ( pContext && !m_imebool){//added check for IME composition)
            // False means allow attempt when crossing table cell 
            //  but tell user why later
        pCmdUI->Enable(CAN_INTERACT && EDT_COP_OK == EDT_CanCut(pContext->GetContext(), FALSE));
    } else { 
        pCmdUI->Enable(FALSE);
    }
}

// Something like this should be used by the non-editor version as well,
//    but use LO_HaveSelection instead of EDT_IsSelected
void CNetscapeEditView::OnUpdateEditCopy(CCmdUI* pCmdUI)
{
    if( UpdateCanCopyInEditControl(this, pCmdUI) )
        return;

    CWinCX *pContext = GetContext();
    if ( pContext && !m_imebool) { //added check for IME composition
        pCmdUI->Enable( CAN_INTERACT && EDT_COP_OK == EDT_CanCopy(pContext->GetContext(), FALSE));
    } else { 
        pCmdUI->Enable(FALSE);
    }
}

// Used only for Edit views
void CNetscapeEditView::OnUpdateEditPaste(CCmdUI* pCmdUI)
{
    MWContext *pMWContext = GET_MWCONTEXT;
    if( pMWContext && EDT_IS_EDITOR(pMWContext) && 
        !pMWContext->waitingMode && !EDT_IsBlocked(pMWContext)  &&
        !m_imebool){//added check for IME composition
        // Check if any of our supported formats is in the clipboard
        int iClip = GetPriorityClipboardFormat(m_pClipboardFormats, MAX_CLIPBOARD_FORMATS);
        pCmdUI->Enable( iClip != 0 && iClip != -1 && 
                        EDT_COP_OK == EDT_CanPaste(pMWContext, TRUE) );
    } else { 
        pCmdUI->Enable(FALSE);
    }
}

// The old-fashioned Method works for Win3.x and NT3.5,
// Only Win95 does OLE dragNdrop from file manager
void CNetscapeEditView::DropFiles( HDROP hDropInfo, BOOL bGetDropPoint)
{
    MWContext *pMWContext = NULL;
    if( GetContext() ){
        pMWContext = GetContext()->GetContext();
    }
    if( pMWContext == NULL ){
        return;
    }

    if( bGetDropPoint ){
        POINT pointDrop;
        // Get mouse at time of drop
        DragQueryPoint( hDropInfo, &pointDrop);
        // Drop Message went to frame,
        //  convert to doc coordinates
        //  (frame->screen->view->doc)
        GetParentFrame()->ClientToScreen( &pointDrop );
        ScreenToClient( &pointDrop );        
        int32 xVal, yVal;        
        CPoint cpDrop(pointDrop);
        ClientToDocXY( cpDrop, &xVal, &yVal );        
        EDT_PositionCaret( pMWContext, xVal, yVal );
    }
    
    // We accept HTML for link creation,
    // GIF and JPG for image insertion
    char pFilename[1024];
    // This is where we put image data
    EDT_ImageData* pEdtData = EDT_NewImageData();

    int iFileCount = DragQueryFile(hDropInfo, (UINT)-1, NULL,0);

    // Can't handle > 1 images at a time
    // TODO: TEST IF WE CAN DO > 1 IMAGE SINCE WE DON'T DO FILE SAVE UNTIL LATER!
    BOOL bImageInserted = FALSE;
    for ( int i = 0; i < iFileCount; i++ ) {
        if ( DragQueryFile(hDropInfo, i, pFilename, 1024) ) {
            CString csFilename(pFilename);
            int iLastDot = csFilename.ReverseFind('.');
            if(iLastDot > 0){
                CString csExt = csFilename.Mid(iLastDot);
                if ( 0 == csExt.CompareNoCase(".htm") ||
                     0 == csExt.CompareNoCase(".html") ||
                     0 == csExt.CompareNoCase(".shtml") ) {
                    // Use filename to paste a link
                    // Convert to relative URL
					int bKeepLinks;
					PREF_GetBoolPref("editor.publish_keep_links",&bKeepLinks);
                    if ( FE_ResolveLinkURL(pMWContext, csFilename, bKeepLinks) ){
                        char * pRelative = (char*)LPCSTR(csFilename);
                        EDT_PasteHREF( pMWContext, &pRelative, &pRelative, 1 );
                    }
                }
                else if( CanSupportImageFile(LPCSTR(csExt))  &&
                          !bImageInserted ){    // Only allow 1 inserted image
                    if (0 == csExt.CompareNoCase(".bmp"))
                    {
#ifdef _IMAGE_CONVERT
                        //need to convert bmp to xxx
                        CONVERT_IMGCONTEXT imageContext;
                        CONVERT_IMG_INFO imageInfo;
                        memset(&imageContext,0,sizeof(CONVERT_IMGCONTEXT));
                        imageContext.m_stream.m_type=CONVERT_FILE;
                        imageContext.m_stream.m_file=XP_FileOpen(pFilename,xpTemporary,XP_FILE_READ_BIN);
                        XP_STRCPY(imageContext.m_filename,pFilename);/* used for output filename creation*/
                        if (imageContext.m_stream.m_file)
                        {
                            imageContext.m_imagetype=conv_bmp;
                            imageContext.m_callbacks.m_dialogimagecallback=FE_ImageConvertDialog;
                            imageContext.m_callbacks.m_displaybuffercallback=FE_ImageConvertDisplayBuffer;
                            imageContext.m_callbacks.m_completecallback=FE_ImageDoneCallBack;

                            imageContext.m_pMWContext=pMWContext;
                            imageContext.m_parentwindow=(void *)this;
                            CONVERT_IMAGERESULT result=convert_stream2image(imageContext,&imageInfo,1,NULL);//1 for 1 output, NULL is for outputfile names we dont need.
                            //there is a callback when complete 
                            XP_FileClose(imageContext.m_stream.m_file);
                            if (result>CONV_OK)//not cancel or ok
                            {
                                AfxMessageBox(m_converrmsg[result]);
                            }
                        }
#endif //IMAGE_CONVERT
                    }
                    else
                    {
                        WFE_ConvertFile2Url(csFilename, (const char*)pFilename);
                        pEdtData->pSrc = XP_STRDUP(csFilename);
                        // Paste the image file:
                        if ( pEdtData->pSrc ) {
						    int bKeepImages;
						    PREF_GetBoolPref("editor.publish_keep_images",&bKeepImages);

                            EDT_InsertImage(pMWContext, pEdtData,bKeepImages);
                            XP_FREE(pEdtData->pSrc);
                            pEdtData->pSrc = NULL;
                            bImageInserted = TRUE;
                        }
                    }
                }
            } 
        }
    }
    EDT_FreeImageData(pEdtData);

    // We're done -- release data
    ::DragFinish( hDropInfo );
}

void CNetscapeEditView::OnEditBarToggle()
{
    GetFrame()->GetChrome()->ShowToolbar(IDS_EDIT_TOOLBAR_CAPTION,
                                         !GetFrame()->GetChrome()->GetToolbarVisible(IDS_EDIT_TOOLBAR_CAPTION));
}

void CNetscapeEditView::OnCharacterBarToggle()
{
    GetFrame()->GetChrome()->ShowToolbar(IDS_CHAR_TOOLBAR_CAPTION,
                                         !GetFrame()->GetChrome()->GetToolbarVisible(IDS_CHAR_TOOLBAR_CAPTION));
}

void CNetscapeEditView::OnUpdateEditBarToggle(CCmdUI* pCmdUI)
{
    if( pCmdUI->m_pMenu ){
        pCmdUI->m_pMenu->ModifyMenu(ID_OPT_EDITBAR_TOGGLE, MF_BYCOMMAND | MF_STRING, ID_OPT_EDITBAR_TOGGLE,
                                    szLoadString(CASTUINT(GetFrame()->GetChrome()->GetToolbarVisible(IDS_EDIT_TOOLBAR_CAPTION) ?
                                                 IDS_HIDE_EDITBAR : IDS_SHOW_EDITBAR)) );
        pCmdUI->Enable(CAN_INTERACT);
    }
}

void CNetscapeEditView::OnUpdateCharacterBarToggle(CCmdUI* pCmdUI)
{
    if( pCmdUI->m_pMenu ){
        pCmdUI->m_pMenu->ModifyMenu(ID_OPT_CHARBAR_TOGGLE, MF_BYCOMMAND | MF_STRING, ID_OPT_CHARBAR_TOGGLE,
                                    szLoadString(CASTUINT(GetFrame()->GetChrome()->GetToolbarVisible(IDS_CHAR_TOOLBAR_CAPTION) ?
                                                 IDS_HIDE_FORMATBAR : IDS_SHOW_FORMATBAR)) );
        pCmdUI->Enable(CAN_INTERACT);
    }
}

//////////////////////////////////////////////////////////////////////
// File/DOC-Save functions
//////////////////////////////////////////////////////////////////////

// We need to save current changes even though
//   we are opening a new Browser window
//   so the browser shows recent changes
void CNetscapeEditView::OnOpenNavigatorWindow()
{
    MWContext * pMWContext = GET_MWCONTEXT;
	if( !pMWContext || !EDT_IS_EDITOR(pMWContext) ){
		return;
	}
    // Force saving (or don't continue) if new document,
    //  or ask user to save first 
    if( !FE_SaveDocument(pMWContext) ){
        return;
    }
    ((CGenericFrame*)GetParentFrame())->OpenNavigatorWindow(pMWContext);
}

//
// Prompt the user for a file name where we should store this document
//
void CNetscapeEditView::OnFileSaveAs()
{
	int bKeepImages;
	PREF_GetBoolPref("editor.publish_keep_images",&bKeepImages);
	int bKeepLinks;
	PREF_GetBoolPref("editor.publish_keep_links",&bKeepLinks);

    SaveDocumentAs( bKeepImages,bKeepLinks );
}

void CNetscapeEditView::OnUpdateFileSave(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(CAN_INTERACT && EDT_DirtyFlag(GET_MWCONTEXT));
}

// Call the front end after every Autosave event so toolbar SAVE button can be grayed
void FE_UpdateEnableStates(MWContext * pMWContext)
{
    CGenericFrame * pFrame = (CGenericFrame*)GetFrame(pMWContext);
    if( pFrame )
    {
        // This triggers update of all UI components
		CNetscapeApp::SendMessageToDescendants(pFrame->m_hWnd, 
                                    WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
    }
}

BOOL CNetscapeEditView::SaveDocumentAs(BOOL bKeepImagesWithDoc,
                                       BOOL bAutoAdjustLinks )
{
    BOOL bRetVal = TRUE;
    if(GetContext() )    // GetActiveContext())
	{
        MWContext * pMWContext = GET_MWCONTEXT;
        if ( pMWContext == NULL ) {
            return FALSE;
        }
		History_entry * hist_entry = SHIST_GetCurrent(&(pMWContext->hist));
		if(hist_entry && hist_entry->address)
		{
	        int  iType = HTM_ONLY;	// This restricts filter to "*.htm;*.html"
            char *pSuggested = NULL;

            // Try to suggest a name if not a new doc or local directory)
            if ( !EDT_IS_NEW_DOCUMENT(pMWContext) ) {
                if ( NET_IsLocalFileURL(hist_entry->address ) ) {
                    // Supply full pathname for a local URL
               		XP_ConvertUrlToLocalFile( hist_entry->address, &pSuggested );
                } else {
                    // What we really want is an attempt to get 8+3 filename
    				//   from the URL, but there are many cases when this will be NULL:
                    //   then dialog uses "*.htm; *.html" as name
                    pSuggested = fe_URLtoLocalName(hist_entry->address, NULL);
                }
            }
        GET_SAVEAS_FILENAME:
            char *pLocalFile = wfe_GetSaveFileName(GET_DLG_PARENT(this)->m_hWnd,
	                                 szLoadString(IDS_SAVE_AS_CAPTION), pSuggested, &iType, NULL/*ppTitle*/);
            if ( pSuggested ) {
                XP_FREE(pSuggested);
            }
            pSuggested = NULL;

            if ( pLocalFile ) {
                HCURSOR hOldCursor = 0;
                if( iType == TXT )
                {
                    hOldCursor = SetCursor(theApp.LoadStandardCursor(IDC_WAIT));
                    SaveDocumentAsText(pLocalFile);
                } else {
                    EDT_PageData * pPageData = EDT_GetPageData(pMWContext);
                    // Prompt for the Page Title if it doesn't exist
                    // ( Extending common dialog for Save File is too much of a pain:
                    //   Had display errors in Win versions 4+ and to much work for Win16 and Win3.51! )
                    if( pPageData){
                        if( !pPageData->pTitle || pPageData->pTitle[0] == '\0'){
                            XP_FREEIF(pPageData->pTitle);
                            pPageData->pTitle = EDT_GetPageTitleFromFilename(pLocalFile);
                            CPageTitleDlg dlg(this, &pPageData->pTitle);
                            if( dlg.DoModal() == IDOK ){
                                EDT_SetPageData(pMWContext, pPageData);
                                // (Note: This triggers changing Title everywhere that is needed)
                            }
                        }
                        EDT_FreePageData(pPageData);
                    }

                    CString csUrlFileName; 
                    // EDT_ is cross-platform - convert to URL format
                    WFE_ConvertFile2Url(csUrlFileName, (const char*)pLocalFile);
                    char * szURL = (char*)(LPCTSTR(csUrlFileName));

                    // Check if this file is already being edited in another edit frame(+context)
                    for(CGenericFrame * f = theApp.m_pFrameList; f; f = f->m_pNext) {
                        MWContext *pOtherContext = f->GetMainContext()->GetContext();
                        if ( pOtherContext && EDT_IS_EDITOR(pOtherContext) &&
                             pOtherContext != pMWContext ) {
                
                            History_entry * pEntry = SHIST_GetCurrent(&pOtherContext->hist);
                    	    if( pEntry && pEntry->address &&
                                0 == XP_STRCMP(pEntry->address, szURL ) ) {
                                // User selected a file they were editing in 
                                //   a different buffer!  Get further confirmation:
                                // Yes to overwrite, No to select a different name, 
                                //   Cancel to abort editing
                                CString csMsg;
                                AfxFormatString1( csMsg, IDS_OVERWRITE_ACTIVE_FILE, szURL );  
                                UINT nResult = MessageBox( csMsg, 
                                                           szLoadString(IDS_SAVE_AS_CAPTION),
                                                           MB_YESNOCANCEL | MB_ICONQUESTION );
                                if ( nResult == IDNO ) {
                                    goto GET_SAVEAS_FILENAME;
                                }
                                if ( nResult == IDCANCEL ) {
                                    XP_FREE(pLocalFile);
                                    return FALSE;
                                }
                                break;
                            }
                        }
                    }
                    // If here, we are ready to save
                
                    // Add the URL we are about to save to the history list
                    //   so user can browse back to it
                    // Add the original source URL to the history list so we can browse back
                    //   to it later. THIS COULD ALSO BE USED FOR PUBLISHING?
                    if( m_bSaveRemoteToLocal ){
                        m_csSourceUrl = hist_entry->address;
                	    
                	    // Round-about way of copying current history to a new one!
                	    URL_Struct *pURL = SHIST_CreateURLStructFromHistoryEntry(pMWContext, hist_entry);
                        if ( pURL ) {
                            // Zero out the form etc. data else we double free it later
                		    memset((void *)&(pURL->savedData), 0, sizeof(SHIST_SavedData));
                		    
                            // Always check the server to get most up-to-date version
                            pURL->force_reload = NET_NORMAL_RELOAD;

                            // If there is no title, it will be set to URL address
                            ///  in CFE_FinishedLayout. It makes no sense to
                            //  use this title when changing the filename
                            char * pTitle = NULL;
                            if ( hist_entry->title == NULL || 
                                 XP_STRCMP(hist_entry->title, hist_entry->address) ) {
                                pTitle = hist_entry->address;
                            }

                		    History_entry * new_entry = SHIST_CreateHistoryEntry( pURL, pTitle); 

                            if( new_entry ){
                                // Make sure we add a new one by changing post data
                                //   and history number (must fool SHIST_AddDocument!);
                                if(new_entry->post_data) XP_FREE(hist_entry->post_data);
                                new_entry->post_data = XP_STRDUP("!");
                                new_entry->post_data_size = 1;
                                new_entry->history_num = 0;

            	                SHIST_AddDocument(pMWContext, new_entry);

                                // Remove bogus post data
                                XP_FREE(new_entry->post_data);
                                new_entry->post_data = NULL;
                                new_entry->post_data_size = 0;
                            }
                        }
                        m_bSaveRemoteToLocal = FALSE;
                    }
                    hOldCursor = SetCursor(theApp.LoadStandardCursor(IDC_WAIT));
                
                    ED_FileError nResult = EDT_SaveFile(pMWContext, 
                                                 hist_entry->address,             // Source
                                                 szURL,                         // Destination
                                                 TRUE,                          // Do SaveAs
                                                 bKeepImagesWithDoc,
                                                 bAutoAdjustLinks);

                    if( nResult == ED_ERROR_NONE ){
                        //  Spin the message loop and network until the
                        //  asynchronous file save procedure has finished
                        //  The final error status will be written to m_FileSaveStatus
                        //    when the SaveFile dialog is destroyed.
                        while( m_pSaveFileDlg ){
                            FEU_StayingAlive();
                            // Note: this seems to work as well - which is better?
                            // nettimer.OnIdle();
                        }
                        // Tell edit core the file Auto Save preference
                        // Primary purpose is to reset autosave if user Canceled when
                        //  prompted to autosave a new doc (that sets AutoSave time to 0)
					    int32 iSave;
					    PREF_GetIntPref("editor.auto_save_delay",&iSave);
                        EDT_SetAutoSavePeriod(pMWContext, iSave );

                        // Return TRUE only if we actually saved, or user said NO,
                        //  but return FALSE if they Canceled or other error.
                        if ( m_FileSaveStatus == ED_ERROR_NONE ) {
#ifdef XP_WIN32
                            if( bSiteMgrIsActive ) {
                                // Tell SiteManager we saved a file
                                pITalkSMClient->SavedURL(pLocalFile);
                            }
                        
                            if( sysInfo.m_bWin4 || sysInfo.m_dwMajor == 4 ){
                                // Add file to Win95 Shell's Documents menu
                                SHAddToRecentDocs( SHARD_PATH, pLocalFile );
                            }
#endif
                        } else 
                            bRetVal = FALSE;  // Error at end of save
                    } else
                        bRetVal = FALSE;      // Error during start of save
                }
                if (pLocalFile){
                    XP_FREE(pLocalFile);
                }
                if( hOldCursor )
                    SetCursor(hOldCursor);

           } else bRetVal = FALSE;       // No local file (user canceled)
    	
        } // Note: end of "we have a current history entry" Should we return FALSE if not?
	}
    return bRetVal; // Default = TRUE
}

void CNetscapeEditView::OnFileSave()
{
    SaveDocument();
}

BOOL CNetscapeEditView::SaveDocument()
{
    BOOL bRetVal = TRUE;
	if(GetContext())
	{
        MWContext * pMWContext = GET_MWCONTEXT;
        if( !pMWContext ){
            return TRUE;
        }
		History_entry * hist_entry = SHIST_GetCurrent(&(pMWContext ->hist));

		if(hist_entry && hist_entry->address)
		{
            char *pLocalFile = NULL;

            // TODO: Current model does local SaveAs for all new pages - 
            //       have pref to use Publish as default?

            if ( !EDT_IS_NEW_DOCUMENT(pMWContext ) )
            {
           	    if( XP_ConvertUrlToLocalFile( hist_entry->address, &pLocalFile ) )
                {
				    int bKeepImages;
				    PREF_GetBoolPref("editor.publish_keep_images",&bKeepImages);
				    int bKeepLinks;
				    PREF_GetBoolPref("editor.publish_keep_links",&bKeepLinks);

                    HCURSOR hOldCursor = SetCursor(theApp.LoadStandardCursor(IDC_WAIT));
                    ED_FileError nResult = EDT_SaveFile(pMWContext ,hist_entry->address,  // Source
                                                hist_entry->address,        // Destination
                                                FALSE,                      // Not doing "SaveAs"
                                                bKeepImages, bKeepLinks);
            
                    if( nResult == ED_ERROR_NONE ){
                        //  Spin the message loop and network until the
                        //  asynchronous file save procedure has finished
                        //  The final error status will be written to m_FileSaveStatus
                        //    when the SaveFile dialog is destroyed.
                        // Because of asynchronous Composer Plugin hook, 
                        //   CSaveDialog is not created after return from EDT_SaveFile,
                        //   so check the context flag as well. That flag is cleared 
                        //   near end of saving, so checking for m_pSaveFileDlg also
                        //   makes sure everything is done before continuing.
                        //   This is important for maintaining proper window focus 
                        while( pMWContext->edit_saving_url || m_pSaveFileDlg ){
                            FEU_StayingAlive();
                            // Note: this seems to work as well
                            // nettimer.OnIdle();
                        }
                        // Return TRUE only if we actually saved, or user said NO,
                        //  but return FALSE if they Canceled or other error.
                        if ( m_FileSaveStatus == ED_ERROR_NONE ) {
    #ifdef XP_WIN32
                            if( bSiteMgrIsActive ) {
                                // Tell SiteManager we saved a file
                                pITalkSMClient->SavedURL(pLocalFile);
                            }
                            if( sysInfo.m_bWin4 || sysInfo.m_dwMajor == 4 ){
                                // Add file to Win95 Shell's Documents menu
                                SHAddToRecentDocs( SHARD_PATH, pLocalFile );
                            }
    #endif
                        } else 
                            bRetVal = FALSE;  // Error at end of save
                    } else
                        bRetVal = FALSE;      // Error during start of save

                    if (pLocalFile){
                        XP_FREE(pLocalFile);
                    }
				    SetCursor(hOldCursor);
                    return bRetVal;
                }
            }
            // If we didn't save above, fall through to do SaveAs
            //  (new page or unknown URL type)
			int bKeepImages;
			PREF_GetBoolPref("editor.publish_keep_images",&bKeepImages);
			int bKeepLinks;
			PREF_GetBoolPref("editor.publish_keep_links",&bKeepLinks);

            return SaveDocumentAs( bKeepImages,bKeepLinks );
        } // Note: end of "we have a current history entry" Should we return FALSE if not?
	}
    return bRetVal; // Default = TRUE
}

// To prevent recursion
static BOOL bCheckAndSave = FALSE;

// This doesn't prompt to save file if a new document
// Use FE_SaveDocument when we must save the new document even if not dirty
BOOL CheckAndSaveDocument(MWContext * pMWContext, BOOL bAutoSaveNewDoc) 
{
    if ( bCheckAndSave ||
         pMWContext == NULL ||
         !EDT_IS_EDITOR(pMWContext) ||
         !EDT_HaveEditBuffer(pMWContext) ||
         pMWContext->type == MWContextMessageComposition){
        return TRUE;
    }
    // Get the edit view
    CNetscapeEditView* pView = (CNetscapeEditView*)WINCX(pMWContext)->GetView();
    if ( !pView->IsKindOf(RUNTIME_CLASS(CNetscapeEditView))) {
        return TRUE;
    }
    // Skip autosave prompt for new page if it is not the window with focus
    //  This avoids popping up AutoSave dialog when in other windows
	if( bAutoSaveNewDoc && pView != pView->GetFocus() ){
        TRACE0("Skipping Autosave of new doc - View not in focus\n");
        return TRUE;
    }

    BOOL bRetVal = TRUE;

    // Set flag to prevent recursive call
    bCheckAndSave = TRUE;

	History_entry * hist_entry = SHIST_GetCurrent(&(pMWContext->hist));
    if( hist_entry && hist_entry->address &&  
		( /*EDT_IS_NEW_DOCUMENT(pMWContext) ||*/ EDT_DirtyFlag(pMWContext) ) )   //BIG CHANGE
    {
        CString csSavePrompt;
        if ( EDT_IS_NEW_DOCUMENT(pMWContext) ) {
            AfxFormatString1( csSavePrompt, IDS_SAVE_DOC_PROMPT, EDT_NEW_DOC_NAME );  
        } else { 
            char * pLocalFile = NULL;
            CString csName;
            // Change URL name to local filename format
            //  returns true if it local file was found
            if ( XP_ConvertUrlToLocalFile( hist_entry->address, &pLocalFile) ) {
                // We are editing a local file
                csName = pLocalFile;
                XP_FREE( pLocalFile );
            } else {    
                // We edited a non-local document
                // TODO: ADD NEW DIALOG TO GIVE USER CHOICE OF SAVING LOCALLY OR PUBLISHING
                // IDS_SAVE_HTTP_PROMPT was used for old behavior. Modify and use here
                csName = hist_entry->address;
            }
            // Don't let the name get too big!
            WFE_CondenseURL(csName, 50, FALSE);
            AfxFormatString1( csSavePrompt, IDS_SAVE_DOC_PROMPT, LPCSTR(csName));  
        }
        if( bAutoSaveNewDoc ){
            csSavePrompt += "\n";
            csSavePrompt += szLoadString(IDS_AUTOSAVE_OFF_MSG );
        }

        char *pCaption = szLoadString(CASTUINT(bAutoSaveNewDoc ? IDS_AUTOSAVE_CAPTION : AFX_IDS_APP_TITLE));
        int nSaveDoc = GET_DLG_PARENT(pView)->MessageBox( LPCSTR(csSavePrompt), pCaption,
                                          MB_YESNOCANCEL | MB_ICONQUESTION );
        if( nSaveDoc == IDCANCEL ){
            bRetVal = FALSE;
        } else if( nSaveDoc == IDYES &&
                   !pView->SaveDocument() ) {
            // Use changed their mind and cancelled out of SaveAs Dialog
            bRetVal = FALSE;
        }
    }

    bCheckAndSave = FALSE;
    return bRetVal;
}

// A cross between SaveDocument() and OnPublish()
// Return TRUE only if we actually save the files
BOOL CNetscapeEditView::SaveRemote()
{
    MWContext * pMWContext = GET_MWCONTEXT;
    if(pMWContext == NULL)
        return FALSE;

    History_entry * hist_entry = SHIST_GetCurrent(&(pMWContext ->hist));

    BOOL bRetVal = FALSE;

    if(hist_entry || hist_entry->address)
    {   
        int	nType = NET_URL_Type(hist_entry->address);
        if( nType == FTP_TYPE_URL ||
            HTTP_TYPE_URL         ||
            SECURE_HTTP_TYPE_URL )
        {
            ED_SaveFinishedOption finishedOpt = ED_FINISHED_REVERT_BUFFER;
            XP_Bool *pSelectedDefault;

            // Publishing location is just current URL without filename, so strip it off
            char * pLocation = EDT_ReplaceFilename(hist_entry->address, NULL, TRUE);
            if( pLocation )
            {
                // Get Pulishing params from preferences
                int bKeepImages;
                PREF_GetBoolPref("editor.publish_keep_images",&bKeepImages);
                int bKeepLinks;
                PREF_GetBoolPref("editor.publish_keep_links",&bKeepLinks);
                XP_Bool bRemberPassword;
                PREF_GetBoolPref("editor.publish_save_password",&bRemberPassword);
        
                // Get list of included image files with respective "leave image at original location" values
                char * pIncludedFiles = EDT_GetAllDocumentFilesSelected( pMWContext, &pSelectedDefault, bKeepImages );

                // Convert from ASIIZ format to an array of strings
                char **ppIncludedFiles = NULL;

                if( pIncludedFiles )
                {
                    int iFileCount = 0;

                    char * pString = pIncludedFiles;
                    int iLen;    

                    // Scan list once to count items
                    while( (iLen = XP_STRLEN(pString)) > 0 )
                    {
                        pString += iLen+1;
                        iFileCount++;
                    }
                    if( iFileCount )
                    {
                        ppIncludedFiles = (char**)XP_ALLOC((iFileCount+1) * sizeof(char*));

                        if(ppIncludedFiles)
                        {
                            pString = pIncludedFiles;
                            for( int i = 0; i< iFileCount; i++)
                            {
                                ppIncludedFiles[i] = XP_STRDUP(pString);
                                pString += XP_STRLEN(pString) + 1;
                            }
                            // Terminate the list
                            ppIncludedFiles[i] = NULL;
                        }
                    }
                    XP_FREE(pIncludedFiles);
                }                
                
                ED_FileError nResult =  
                                EDT_PublishFile(pMWContext,finishedOpt, hist_entry->address, 
                                                ppIncludedFiles, 
                                                pLocation,
                                                bKeepImages,
                                                bKeepLinks,
                                                bRemberPassword);
                if( nResult == ED_ERROR_NONE )
                {
                    //  Spin the message loop and network until the
                    //  asynchronous file save procedure has finished
                    //  The final error status will be written to m_FileSaveStatus
                    //    when the SaveFile dialog is destroyed.
                    while( m_pSaveFileDlg ){
                        FEU_StayingAlive();
                    }
                    // Return TRUE only if we actually saved, or user said NO,
                    //  but return FALSE if they Canceled or other error.
                    if ( m_FileSaveStatus == ED_ERROR_NONE )
                    {
                        bRetVal = TRUE;
    #ifdef XP_WIN32
                        if( bSiteMgrIsActive )
                        {
                            // Tell SiteManager we saved a file
                            pITalkSMClient->SavedURL(hist_entry->address);
                        }

                        /*  TODO: Should we add URLs to Window's document list?
                        if( sysInfo.m_bWin4 || sysInfo.m_dwMajor == 4 ){
                            // Add file to Win95 Shell's Documents menu
                            SHAddToRecentDocs( SHARD_PATH, hist_entry->address );
                        }*/
    #endif
                    }
                }

                XP_FREE(pLocation);
                //NOTE: DO NOT try to free ppIncludedFiles list - EDT_PublishFile will do that

                // Stay here untill file upload has finished
                while( m_pSaveFileDlg )
                    FEU_StayingAlive();
            }
        }
    }

    return bRetVal;
}

void CNetscapeEditView::OnPublish() 
{
    MWContext * pMWContext = GET_MWCONTEXT;
    if(pMWContext == NULL) {
        return;
    }

    char *pSrcURL;
    History_entry * pEntry = SHIST_GetCurrent(&pMWContext->hist);
    if (pEntry && pEntry->address && *pEntry->address) {
      pSrcURL = XP_STRDUP(pEntry->address);
    }
    else {
      // no source name.
      pSrcURL = XP_STRDUP(EDT_NEW_DOC_NAME);
    }

    // Get the destination and what files to include from the user
    // Pass in the URL of the last-failed publishing attempt
    //  so it can use the last-location prefs and correct typing errors
    CPublishDlg dlg(this, pMWContext, pSrcURL);

    if( IDOK == dlg.DoModal() ) {
        int bKeepImages;
        PREF_GetBoolPref("editor.publish_keep_images",&bKeepImages);
        int bKeepLinks;
        PREF_GetBoolPref("editor.publish_keep_links",&bKeepLinks);

        EDT_PublishFile(pMWContext,ED_FINISHED_REVERT_BUFFER, pSrcURL, 
                        dlg.m_ppIncludedFiles, 
                        dlg.m_pFullLocation,
                        bKeepImages,
                        bKeepLinks,
                        dlg.m_bRememberPassword);
    }

    XP_FREE(pSrcURL);
}

// USE BROWSER'S SAVE MECHANISM FOR TEXT FILES
void EdtSaveToTempCallback( char *pFileURL, void *hook )
{
    MWContext *pMWContext = (MWContext*)hook;
    if( pMWContext )
    {
        // Get the edit view
        CNetscapeEditView* pView = (CNetscapeEditView*)WINCX(pMWContext)->GetView();
        if ( !pView->IsKindOf(RUNTIME_CLASS(CNetscapeEditView)))
        {
            XP_ASSERT(0);    
            return;
        }

        // Clear any existing name
        XP_FREEIF( pView->m_pTempFilename );

        if( pFileURL )
        {
            pView->m_pTempFilename = XP_STRDUP(pFileURL);
        }
    }
}

char* CNetscapeEditView::SaveToTempFile()
{
    MWContext *pMWContext = GET_MWCONTEXT;
    if( pMWContext )
    {
	    History_entry * hist_entry = SHIST_GetCurrent(&(pMWContext->hist));
	    if(hist_entry && hist_entry->address)
        {
            EDT_SaveToTempFile(pMWContext, EdtSaveToTempCallback, pMWContext);

            // Spin here until temp file saving is finished
            while(  pMWContext->edit_saving_url ) 
            {
                FEU_StayingAlive();
            }

            // Note: All temp files are automatically deleted when we exit the app
            return m_pTempFilename;
        }
    }
    return NULL;
}

// Klude so we don't have to include cxsave.h to access CSaveCX::SaveAnchorAsText
// (Win16 compiler runs out of keys if we include cxsave.h)
extern BOOL wfe_SaveAnchorAsText(const char *pAnchor, History_entry *pHist,  CWnd *pParent, char *pFileName);

void CNetscapeEditView::SaveDocumentAsText(char * pFilename)
{
    MWContext *pMWContext = GET_MWCONTEXT;
    if( ! pMWContext || !pFilename )
        return;

    if( EDT_DirtyFlag(pMWContext) || EDT_IS_NEW_DOCUMENT(pMWContext) )
    {
        // First save current page as temp HTML file
        char *pTempFilename = SaveToTempFile();
        if(  pTempFilename )
        {
            // This saving is ASYNCHRONOUS, but isn't a problem since its
            //   doesn't affect what we edit - we don't switch to editing this
            //   content like we do when doing SaveAs for HTML.
            wfe_SaveAnchorAsText(pTempFilename, NULL, this, pFilename);
        }
    } 
    else 
    {
            // No temp file needed - just use current file
		History_entry * hist_entry = SHIST_GetCurrent( &(pMWContext->hist) );
		if( hist_entry && hist_entry->address )
        {
            wfe_SaveAnchorAsText( hist_entry->address, NULL, this, pFilename );
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////
// Call this instead of forcing file saving by making 
//  a temporary file from current page. This filename
//  replaces the pUrl->address in struct passed in.
//  If return is FALSE, then there was an error - don't continue
BOOL FE_PrepareTempFileUrl(MWContext * pMWContext, URL_Struct *pUrl)
{
    if( EDT_IS_EDITOR(pMWContext) &&
        (pMWContext->bIsComposeWindow || EDT_IS_NEW_DOCUMENT(pMWContext) || EDT_DirtyFlag(pMWContext)) )
    {
        CNetscapeEditView* pView = (CNetscapeEditView*)WINCX(pMWContext)->GetView();

        if ( !pView || !pView->IsKindOf(RUNTIME_CLASS(CNetscapeEditView))) {
            return FALSE;
        }
        char *pTempFilename = pView->SaveToTempFile();
        if( pTempFilename )
        {
            XP_FREEIF(pUrl->address);
            pUrl->address = XP_STRDUP(pTempFilename);
        } else {
            // Failed to save to the temp file - abort
            return FALSE;            
        }
    }
    // If we don't need to change anything in the URL struct,
    //  just return that its OK
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////
// Check if changes were made, prompt user to save if they were
// Returns TRUE for all cases except CANCEL by the user in any dialog
BOOL FE_CheckAndSaveDocument(MWContext * pMWContext) 
{
    return CheckAndSaveDocument( pMWContext, FALSE );
}

BOOL FE_CheckAndAutoSaveDocument(MWContext * pMWContext) 
{
    return CheckAndSaveDocument( pMWContext, TRUE );
}

// Not used any more, but its declared in ns/include/fe_proto.h
BOOL FE_SaveNonLocalDocument(MWContext * pMWContext, BOOL bSaveNewDocument) 
{
    return TRUE;
}

// Call when we MUST save a NEW document, such as before Printing or calling an external Editor
// Return TRUE: 
//      1. User saved the document
//      2. User didn't want to save, and doc already exists (not new)
//      3. Doc exists but is not dirty
//      4. We are not a Page Composer (allows calling without testing for EDT_IS_EDITOR() first )
//      5. We are a Message Compose window
// Note: We must always save a new/untitled document, even if not dirty
BOOL FE_SaveDocument(MWContext * pMWContext)
{
    if( !pMWContext ){
        return FALSE;
    }
    if( !EDT_IS_EDITOR(pMWContext) ||
        pMWContext->type == MWContextMessageComposition){
        return TRUE;
    }
    if(EDT_IS_NEW_DOCUMENT(pMWContext) ){
        // Force saving a new document
        // WHO CHANGED THIS TO GetPane()? That fails!
        CNetscapeEditView* pView = (CNetscapeEditView*)WINCX(pMWContext)->GetView(); //GetPane();
        if ( !pView->IsKindOf(RUNTIME_CLASS(CNetscapeEditView))) {
            return TRUE;
        }
        CSaveNewDlg dlg( GET_DLG_PARENT(pView) );
        if ( dlg.DoModal() == IDOK ) {
            return pView->SaveDocument();
        }
   } else {
        // Doc is already saved - save again only if dirty and user wants to
        return FE_CheckAndSaveDocument(pMWContext);
    }
    return FALSE;
}

/////////////////////////////////////////////////////////////////////////////

// Use this for any item that just needs a context to be active
//   AND we are not in "waiting mode"
void CNetscapeEditView::HaveEditContext(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(CAN_INTERACT);
}

// Use this for any item that is active if there is no selected text
void CNetscapeEditView::IsNotSelected(CCmdUI* pCmdUI)
{
    pCmdUI->Enable( CAN_INTERACT && !EDT_IsSelected(GET_MWCONTEXT) );
}

#ifdef _IMAGE_CONVERT




void
FE_ImageConvertPluginCallback(EDT_ImageEncoderStatus status, void* hook)
{
    PluginHookStruct *t_hook=(PluginHookStruct *)hook;
    if (ED_IMAGE_ENCODER_USER_CANCELED==status)
    {
        return;//nothing to do.
    }
    if (ED_IMAGE_ENCODER_EXCEPTION==status)
    {
        return;
    }
    if (ED_IMAGE_ENCODER_OK==status)
    {
        if (t_hook->m_outputimagecontext->m_callbacks.m_completecallback)
        (*t_hook->m_outputimagecontext->m_callbacks.m_completecallback)(t_hook->m_outputimagecontext,1,t_hook->m_pMWContext);
        XP_FREE(t_hook);//allocated elsewhere
    }
}

                              
CONVERT_IMAGERESULT
FE_ImageDoneCallBack(CONVERT_IMGCONTEXT *p_outputimageContext,int16 p_numoutputs,void *p_pMWContext)
{
    MWContext *t_mwcontext=(MWContext *)p_pMWContext;
    EDT_ImageData* pEdtData = EDT_NewImageData();
    if( pEdtData )
    {
        // We just need to set the URL
        pEdtData->pSrc = XP_STRDUP(XP_PlatformFileToURL(p_outputimageContext[0].m_filename));
        //and free memory associated with p_outputimageContext
        XP_FREE(p_outputimageContext);
        // Paste the image file:
        if ( pEdtData->pSrc ) {
			int bKeepImages;
			PREF_GetBoolPref("editor.publish_keep_images",&bKeepImages);

            EDT_InsertImage(t_mwcontext, pEdtData,bKeepImages);
            XP_FREE(pEdtData->pSrc);
            pEdtData->pSrc = NULL;
        }
    }
    else
        return CONVERR_UNKNOWN;
    EDT_FreeImageData(pEdtData);
    return CONV_OK;
}

//this function takes an array of options and frees their strings 
void
free_convert_options(CONVERTOPTION *options,int32 numoptions)
{
    if (numoptions)
        delete [] options;
    // Don't have to free the options because they are Java strings -- they're garbage collected. (Ooooo!).
}

              
CONVERT_IMAGERESULT
FE_ImageConvertDialog(CONVERT_IMGCONTEXT *input,CONVERT_IMGCONTEXT *outputarray,CONVERT_IMG_INFO *imginfo,int16 numoutput,CONVERT_IMG_ARRAY imagearray)
{
    CEditorResourceDll t_resourcedll;//loads dll if not allready loaded.  this contains code/resources for our dlls
    //we have 1 built in encoder, JPEG and n # of PLUGIN encoders
    int32 t_numencoders=EDT_NumberOfEncoders();
    //we must make a list of available encoders to pass to the convert dialog
    CONVERTOPTION *t_option=new CONVERTOPTION[1+t_numencoders];
    if (!t_option||!input||!outputarray||!imginfo||(numoutput!=1))
    {
        XP_ASSERT(FALSE);
        return CONVERR_UNKNOWN;
    }
    t_option[0].m_pencodername= "JPEG";
    t_option[0].m_pfileextention= "JPG";
    t_option[0].m_phelpstring= "This format is very good for photo style pictures."; // Bug. Make this a string resource.
    t_option[0].m_builtin=TRUE;
    for (int32 i=0;i<t_numencoders;i++)//EDT calls indexed by 0 must subtract 1
    {
        t_option[i+1].m_pencodername=EDT_GetEncoderName(i);
        t_option[i+1].m_pfileextention=EDT_GetEncoderFileExtension(i);
        t_option[i+1].m_phelpstring=EDT_GetEncoderMenuHelp(i);
        t_option[i+1].m_builtin=FALSE;
    }
    IImageConversionDialog *dlg=t_resourcedll.CreateImageConversionDialog(((CWnd *)(input->m_parentwindow))->GetSafeHwnd());
    if (!dlg)
    {
        XP_ASSERT(FALSE);
        free_convert_options(t_option,1+t_numencoders);
        return CONVERR_UNKNOWN;
    }
    char *t_filename=NULL;
    if (input->m_stream.m_type==CONVERT_FILE) //we will predict the outcome. we must be prepared for an allready existing file
    {
        t_filename=(char *)XP_ALLOC(strlen(input->m_filename)+4);//+4 for the future extention we will add on
        memcpy(t_filename,input->m_filename,strlen(input->m_filename)+1); //+1 for the null byte
        if ((strlen(t_filename)>=4)&&(t_filename[strlen(t_filename)-4]=='.'))
        {
            t_filename[strlen(t_filename)-4]=0;
        }//file was xxx.bmp  now= xxx
    }
    else if (input->m_stream.m_type==CONVERT_MEMORY)
    {
        t_filename=WH_TempName(xpTemporary,"image");
        t_filename[strlen(t_filename)-4]=0;
    }
    if (!t_filename)
    {
        XP_ASSERT(FALSE);
        free_convert_options(t_option,1+t_numencoders);
        return CONVERR_UNKNOWN;
    }

    dlg->setOutputFileName1(t_filename);
    dlg->setOutputImageType1(0);//JPEG= default
    dlg->setConvertOptions(t_option,t_numencoders+1);
    CWFEWrapper t_wfe;
    if (input->m_pMWContext)
    {
        t_wfe.setMWContext((MWContext *)input->m_pMWContext);
        dlg->setWFEInterface(&t_wfe);
    }
    free(t_filename);
    if (IDOK==dlg->DoModal())
    {
        //must check return value of plugin start encode!
        int32 t_type=dlg->getOutputImageType1();
        XP_ASSERT(t_type<1+t_numencoders);
        CString t_string(dlg->getOutputFileName1());
        strcpy(outputarray[0].m_filename,t_string);
        
        outputarray[0].m_stream.m_type=CONVERT_FILE;
        //call plugin
        if (!t_option[t_type].m_builtin)
        {//we have a plug in
            if (EDT_GetEncoderNeedsQuantizedSource(t_type-1))//-1 for built in JPEG
            {//quantize it!
                /*convert API*/
                CONVERT_IMAGERESULT t_result;
                t_result=convert_quantize_pixels((unsigned char **)imagearray,imginfo->m_image_width,imginfo->m_image_height,EDTMAXCOLORVALUE);
                if (t_result!=CONV_OK)
                {
                    free_convert_options(t_option,1+t_numencoders);
                    return t_result;
                }
            }
            PluginHookStruct *t_hook=(PluginHookStruct *)XP_ALLOC(sizeof(PluginHookStruct));
            t_hook->m_outputimagecontext=outputarray;
            t_hook->m_pMWContext=(MWContext *)input->m_pMWContext;
            //do NOT open file for encoder.
            if (!EDT_StartEncoder((MWContext *)input->m_pMWContext,t_type-1,imginfo->m_image_width,
                imginfo->m_image_height,(char**)imagearray,outputarray[0].m_filename,FE_ImageConvertPluginCallback,(void *)t_hook))
            {
                free_convert_options(t_option,1+t_numencoders);
                return CONVERR_UNKNOWN;
            }
            outputarray[0].m_imagetype=conv_plugin;
        }
        else
        {
            IJPEGOptionsDlg *t_jpgdlg=t_resourcedll.CreateJpegDialog(((CWnd *)input->m_parentwindow)->GetSafeHwnd());
            t_jpgdlg->setOutputImageQuality(DEFAULT_IMG_CONVERT_QUALITY);
            int t_result=t_jpgdlg->DoModal();
            outputarray[0].m_quality=(int16)t_jpgdlg->getOutputImageQuality();
            delete t_jpgdlg;
            if (IDOK!=t_result)
            {
                free_convert_options(t_option,1+t_numencoders);
                return CONV_CANCEL;
            }
            outputarray[0].m_imagetype=conv_jpeg;
            //must be open for rest of converter to work
        }
        free_convert_options(t_option,1+t_numencoders);
        return CONV_OK;
    }
    free_convert_options(t_option,1+t_numencoders);
    return CONV_CANCEL;
}

void FE_ImageConvertDisplayBuffer(void *)
{

}


#endif //_IMAGE_CONVERT


int 
CNetscapeEditView::findDifference(const char *p_str1,const char *p_str2,MWContext *p_pmwcontext) //-1==no difference
{
    if (!p_pmwcontext)
        return -1;
    char *t_pchar1=(char *)p_str1;
    char *t_pchar2=(char *)p_str2;
    char *t_fut1;
    char *t_fut2;
    INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(p_pmwcontext);
    int16 win_csid = INTL_GetCSIWinCSID(csi);
    for (;*t_pchar1&&*t_pchar2;)
    {
        t_fut1=INTL_NextChar(win_csid, t_pchar1);
        t_fut2=INTL_NextChar(win_csid, t_pchar2);
        if (memcmp(t_pchar1,t_pchar2,t_fut1-t_pchar1))
            return t_pchar1-p_str1; 
        t_pchar1=t_fut1; 
        t_pchar2=t_fut2; 
    }
    if (!*t_pchar1&&!*t_pchar2)
        return -1;
    return t_pchar1-p_str1;
}

// For automated testing by QA only
void CNetscapeEditView::OnSelectNextNonTextObject()
{
    EDT_SelectNextNonTextObject(GET_MWCONTEXT);
}
#endif // EDITOR
