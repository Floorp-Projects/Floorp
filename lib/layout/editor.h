/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


//
// Editor save stuff. LWT 06/01/95
//

#ifdef EDITOR

#ifndef _EDITOR_H
#define _EDITOR_H
#include "edttypep.h"       // include first so types are mapped.

// 
// Turn on new DT mapping code
//
#define EDT_DDT 1

#if defined(JAVA)
#define EDITOR_JAVA
#endif

//
//    Many of the system header files have C++ specific stuff in
//    them. If we just widely extern "C" {} the lot, we will lose.
//    So.. pre-include some that must be seen by C++.
//
#if defined(HPUX) || defined(SCO_SV)
#include "math.h"
#endif /*HPUX losing headers */

extern "C" {
#include "net.h"
//#include "glhist.h"
//#include "shist.h"
#include "merrors.h"
#include "xp.h"
#include "pa_parse.h"
#include "layout.h"
#include "edt.h"
#include "pa_tags.h"
#include "xpassert.h"
#include "xp_file.h"
#include "libmocha.h"
#include "libi18n.h"
}

#include "bits.h"
#include "garray.h"
#include "streams.h"


#include "gui.h"

// For textFE test, hardts
#include "xlate.h"

#include "fsfile.h"
#include "shist.h"

#ifdef XP_WIN32
#include	<crtdbg.h>
#endif

// XP Strings
extern "C" {
#include "xpgetstr.h"
#define WANT_ENUM_STRING_IDS
#include "allxpstr.h"
#undef WANT_ENUM_STRING_IDS
}

// Defining DEBUG_AUTO_SAVE will make the document AutoSave every six seconds.
//#define DEBUG_AUTO_SAVE

// Test if ele
#ifdef USE_SCRIPT
#define  ED_IS_NOT_SCRIPT(pEle) (0 == (pEle->Text()->m_tf & (TF_SERVER | TF_SCRIPT | TF_STYLE)))
#else
#define ED_IS_NOT_SCRIPT(pEle) (TRUE)
#endif

#define EDT_SELECTION_START_COMMENT "selection start"
#define EDT_SELECTION_END_COMMENT "selection end"

// TRY / CATCH / RETHROW macros for minimal error recovery on macs
#if defined(XP_MAC)
#define EDT_TRY try
#define EDT_CATCH_ALL catch(...)
#define EDT_RETHROW throw
#else
#define EDT_TRY if(1)
#define EDT_CATCH_ALL else
#define EDT_RETHROW

#endif

#ifdef XP_MAC
// taken from pa_amp.h
#define NON_BREAKING_SPACE  ((char)007)
#define NON_BREAKING_SPACE_STRING   "\07"
#else
#define NON_BREAKING_SPACE  ((char)160)
#define NON_BREAKING_SPACE_STRING   "\xa0"
#endif

#define DEF_FONTSIZE    3
#define MAX_FONTSIZE    7
#define DEFAULT_HR_THICKNESS    2

#define EDT_IS_LIVEWIRE_MACRO(s) ( (s) && (s)[0] == '`' )


// Macros for the public entry points.
// usage:
// GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) ; // For no return value
// GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) return-value; // to return a return value

#define GET_WRITABLE_EDIT_BUF_OR_RETURN(CONTEXT, BUFFER) CEditBuffer* BUFFER = LO_GetEDBuffer( CONTEXT );\
    if (!CEditBuffer::IsAlive(BUFFER) || !BUFFER->IsReady() || !BUFFER->IsWritable() ) return

#define GET_EDIT_BUF_OR_RETURN(CONTEXT, BUFFER) CEditBuffer* BUFFER = LO_GetEDBuffer( CONTEXT );\
    if (!CEditBuffer::IsAlive(BUFFER) || !BUFFER->IsReady() ) return

#define GET_EDIT_BUF_OR_RETURN_READY_OR_NOT(CONTEXT, BUFFER) CEditBuffer* BUFFER = LO_GetEDBuffer( CONTEXT );\
    if (!CEditBuffer::IsAlive(BUFFER) ) return

#define GET_WRITABLE_EDIT_BUF_OR_RETURN_READY_OR_NOT(CONTEXT, BUFFER) CEditBuffer* BUFFER = LO_GetEDBuffer( CONTEXT );\
    if (!CEditBuffer::IsAlive(BUFFER) || !BUFFER->IsWritable() ) return


//-----------------------------------------------------------------------------
// Edit Classes
//-----------------------------------------------------------------------------
class CEditElement;
class CEditLeafElement;
class CEditSubDocElement;
class CEditTableElement;
class CEditTableRowElement;
class CEditCaptionElement;
class CEditTableCellElement;
class CEditRootDocElement;
class CEditContainerElement;
class CEditListElement;
class CEditTextElement;
class CEditImageElement;
class CEditHorizRuleElement;
class CEditIconElement;
class CEditEndElement;
class CEditBreakElement;
class CEditTargetElement;
class CEditLayerElement;
class CEditDivisionElement;
class CParseState;
class CPrintState;
class CEditImageLoader;
class CEditSaveObject;
class CFileSaveObject;
struct ED_Link;
class CEditCommand;
class CEditCommandGroup;

class CEditSaveData;
class CEditSaveToTempData;

#ifdef DEBUG
class CEditTestManager;
#endif
#ifdef EDITOR_JAVA
// Opaque interface to Editor Plugin support classes.
// (It's too expensive to pull jri in in this header file.
// If we do, bsdi runs out of VM while compiling.

typedef void* EditorPluginManager;
EditorPluginManager EditorPluginManager_new(MWContext* pContext);
void EditorPluginManager_delete(EditorPluginManager a);
XP_Bool EditorPluginManager_IsPluginActive(EditorPluginManager a);
XP_Bool EditorPluginManager_PluginsExist();
class CEditorPluginInterface;
#endif

// Originally this was a sugared int32, but we ran into
// trouble working with big-endian architectures, like the
// Macintosh. So rather than being even cleverer with
// macros, I decided to make it a regular class.
// If this turns out to be a performance bottleneck
// we can always inline it.... jhp
class ED_Color {
public:
    ED_Color();
    ED_Color(LO_Color& pLoColor);
    ED_Color(int32 rgb);
    ED_Color(int r, int g, int b);
    ED_Color(LO_Color* pLoColor);
    XP_Bool IsDefined();
    XP_Bool operator==(const ED_Color& other) const;
    XP_Bool operator!=(const ED_Color& other) const;
    int Red();
    int Green();
    int Blue();
    LO_Color GetLOColor();
    long GetAsLong(); // 0xRRGGBB
	void SetUndefined();
	static ED_Color GetUndefined();
private:
    LO_Color m_color;
    XP_Bool m_bDefined;
};

// Copies the argument into a static buffer, and then upper-cases the
// buffer, and returns a pointer to the buffer. You don't own the
// result pointer, and you can't call twice in same line of code.

char* EDT_UpperCase(char*);

// Returns a string for the given alignment. Uses a static buffer, so
// don't call twice in the same line of code.

char* EDT_AlignString(ED_Alignment align);

// Backwards-compatable macros. Should clean this up some day.

#define EDT_COLOR_DEFAULT    (ED_Color())
#define EDT_RED(x)      ((x).Red())
#define EDT_GREEN(x)    ((x).Green())
#define EDT_BLUE(x)     ((x).Blue())
#define EDT_RGB(r,g,b)  (ED_Color(r,g,b))
#define EDT_LO_COLOR(pLoColor)  ED_Color(pLoColor)

/* Gives us quick access to first cell in each column or row 
 *  with location and index data
*/
struct _EDT_CellLayoutData {
    int32                 X;                 /* X value of cell */
    int32                 Y;                 /* Y value of cell */
    int32                 iColumn;           /* Geometric column index */
    int32                 iRow;              /* Geometric row index */
    int32                 iCellsInColumn;    /* Total number of cells in each column */
    int32                 iCellsInRow;       /*  and row */
    LO_Element            *pLoCell;          /* Layout element pointer */
    CEditTableCellElement *pEdCell;          /* Corresponding edit element object */
};

typedef struct _EDT_CellLayoutData EDT_CellLayoutData;


// Editor selections

// template declarations
Declare_GrowableArray(int32,int32)                      // TXP_GrowableArray_int32
Declare_GrowableArray(ED_Link,ED_Link*)                 // TXP_GrowableArray_ED_Link
Declare_GrowableArray(EDT_MetaData,EDT_MetaData*)       // TXP_GrowableArray_EDT_MetaData
Declare_GrowableArray(pChar,char*)                      // TXP_GrowableArray_pChar
Declare_GrowableArray(CEditCommand,CEditCommand*)       // TXP_GrowableArray_CEditCommand
#ifdef EDITOR_JAVA
Declare_GrowableArray(CEditorPluginInterface,CEditorPluginInterface*)       // TXP_GrowableArray_CEditorPluginInterface
#endif
//cmanske - store lists of selected cell elements
Declare_GrowableArray(CEditTableCellElement,CEditTableCellElement*)  // TXP_GrowableArray_CEditTableCellElement
Declare_GrowableArray(LO_CellStruct,LO_CellStruct*)                  // TXP_GrowableArray_LO_CellStruct
//List of tables encountered during Relayout()
Declare_GrowableArray(LO_TableStruct,LO_TableStruct*)                // TXP_GrowableArray_LO_TableStruct

Declare_GrowableArray(EDT_CellLayoutData, EDT_CellLayoutData*)       // TXP_GrowableArray_EDT_CellLayoutData

Declare_PtrStack(ED_Alignment,ED_Alignment)             // TXP_PtrStack_ED_Alignment
Declare_PtrStack(TagType,TagType)                       // TXP_PtrStack_TagType
Declare_PtrStack(CEditTextElement,CEditTextElement*)    // TXP_PtrStack_CEditTextElement
Declare_PtrStack(ED_TextFormat,int32)                   // TXP_PtrStack_ED_TextFormat
Declare_PtrStack(CEditCommandGroup,CEditCommandGroup*)  // TXP_PtrStack_CEditCommandGroup
Declare_PtrStack(CParseState,CParseState*)              // TXP_PtrStack_CParseState


/* iTableMode param for CEditTableElement::SetSizeMode() */
#define ED_MODE_TABLE_PERCENT     0x0001   /* Convert table to use % of parent width */
#define ED_MODE_TABLE_PIXELS      0x0002   /* Convert table to use absolute pixels */
#define ED_MODE_CELL_PERCENT      0x0004   /* Convert all cells to use % of parent width */
#define ED_MODE_CELL_PIXELS       0x0008   /* Convert all cells to use absolute pixels */
#define ED_MODE_USE_TABLE_WIDTH   0x0010   /* Set WIDTH param for table */
#define ED_MODE_NO_TABLE_WIDTH    0x0020   /* Remove WIDTH param for table */
#define ED_MODE_USE_TABLE_HEIGHT  0x0040   /* Set HEIGHT param for table*/
#define ED_MODE_NO_TABLE_HEIGHT   0x0080   /* Remove HEIGHT param table */
#define ED_MODE_USE_CELL_WIDTH    0x0100   /* Set WIDTH param for all cells */
#define ED_MODE_NO_CELL_WIDTH     0x0200   /* Remove WIDTH param for all cells */
#define ED_MODE_USE_CELL_HEIGHT   0x0400   /* Set HEIGHT param for all cells*/
#define ED_MODE_NO_CELL_HEIGHT    0x0800   /* Remove HEIGHT param for all cells*/
#define ED_MODE_USE_COLS          0x1000   /* Use COLS param for table (only 1st row used) */
#define ED_MODE_NO_COLS           0x2000   /* Remove COLS param for table (all cell widths used) */

#ifdef DEBUG
    #define DEBUG_EDIT_LAYOUT
#endif

typedef int32 DocumentVersion;

typedef int32 ElementIndex; // For persistent insert points, a position from the start of the document.
typedef int32 ElementOffset; // Within one element, an offset into an element. Is 0 or 1, except for text.

class CEditInsertPoint
{
public:
    CEditInsertPoint();
    CEditInsertPoint(CEditElement* pElement, ElementOffset iPos);
    CEditInsertPoint(CEditLeafElement* pElement, ElementOffset iPos);
    CEditInsertPoint(CEditElement* pElement, ElementOffset iPos, XP_Bool bStickyAfter);

    XP_Bool IsStartOfElement();
    XP_Bool IsEndOfElement();
    XP_Bool IsStartOfContainer();
    XP_Bool IsEndOfContainer();
    XP_Bool IsStartOfDocument();
    XP_Bool IsEndOfDocument();

    XP_Bool GapWithBothSidesAllowed();
    XP_Bool IsLineBreak();
    XP_Bool IsSoftLineBreak();
    XP_Bool IsHardLineBreak();
    XP_Bool IsSpace(); // After insert point
    XP_Bool IsSpaceBeforeOrAfter(); // Before or after insert point

    // Move forward and backward. Returns FALSE if can't move
    CEditInsertPoint NextPosition();
    CEditInsertPoint PreviousPosition();

    XP_Bool operator==(const CEditInsertPoint& other );
    XP_Bool operator!=(const CEditInsertPoint& other );
    XP_Bool operator<(const CEditInsertPoint& other );
    XP_Bool operator<=(const CEditInsertPoint& other );
    XP_Bool operator>=(const CEditInsertPoint& other );
    XP_Bool operator>(const CEditInsertPoint& other );
    XP_Bool IsDenormalizedVersionOf(const CEditInsertPoint& other);
    intn Compare(const CEditInsertPoint& other );

#ifdef DEBUG
    void Print(IStreamOut& stream);
#endif

    CEditLeafElement* m_pElement;
    ElementOffset m_iPos;
    XP_Bool m_bStickyAfter;

private:
    CEditElement* FindNonEmptyElement( CEditElement *pStartElement );
};


class CEditSelection
{
public:
    CEditSelection();
    CEditSelection(CEditElement* pStart, intn iStartPos, CEditElement* pEnd, intn iEndPos, XP_Bool fromStart = FALSE);
    CEditSelection(const CEditInsertPoint& start, const CEditInsertPoint& end, XP_Bool fromStart = FALSE);

    XP_Bool operator==(const CEditSelection& other );
    XP_Bool operator!=(const CEditSelection& other );
    XP_Bool IsInsertPoint();
    CEditInsertPoint* GetActiveEdge();
    CEditInsertPoint* GetAnchorEdge();
    CEditInsertPoint* GetEdge(XP_Bool bEnd);

    // Like ==, except ignores bFromStart.
    XP_Bool EqualRange(CEditSelection& other);
    XP_Bool Intersects(CEditSelection& other); // Just a test; doesn't change this.
    XP_Bool Contains(CEditInsertPoint& point);
    XP_Bool Contains(CEditSelection& other);
    XP_Bool ContainsStart(CEditSelection& other);
    XP_Bool ContainsEnd(CEditSelection& other);
    XP_Bool ContainsEdge(CEditSelection& other, XP_Bool bEnd);
    XP_Bool CrossesOver(CEditSelection& other);

    XP_Bool ClipTo(CEditSelection& other); // True if the result is defined. No change to this if it isn't

    CEditElement* GetCommonAncestor();
    XP_Bool CrossesSubDocBoundary();
    CEditSubDocElement* GetEffectiveSubDoc(XP_Bool bEnd);

    XP_Bool ExpandToNotCrossStructures(); // TRUE if this was changed
    void ExpandToBeCutable();
    void ExpandToIncludeFragileSpaces();
    void ExpandToEncloseWholeContainers();
    XP_Bool EndsAtStartOfContainer();
    XP_Bool StartsAtEndOfContainer();
    XP_Bool AnyLeavesSelected(); // FALSE if insert point or container end.
    XP_Bool IsContainerEnd(); // TRUE if just contains the end of a container.
    // Useful for cut & delete, where you don't replace the final container mark
    XP_Bool ContainsLastDocumentContainerEnd();
    void ExcludeLastDocumentContainerEnd();

    CEditContainerElement* GetStartContainer();
    CEditContainerElement* GetClosedEndContainer(); // Not 1/2 open!

#ifdef DEBUG
    void Print(IStreamOut& stream);
#endif

    CEditInsertPoint m_start;
    CEditInsertPoint m_end;
    XP_Bool m_bFromStart;
    
    //cmanske: Needed to differentiate between an empty selection 
    //  (indicating we should get the current selection)
    //  and a selection obtained from a table cell
    XP_Bool IsEmpty() { return (m_start.m_pElement == 0); }
};


class CPersistentEditInsertPoint
{
public:
    // m_bStickyAfter defaults to TRUE
    CPersistentEditInsertPoint();
    CPersistentEditInsertPoint(ElementIndex index);
    CPersistentEditInsertPoint(ElementIndex index, XP_Bool bStickyAfter);
#ifdef DEBUG
    void Print(IStreamOut& stream);
#endif
    XP_Bool operator==(const CPersistentEditInsertPoint& other );
    XP_Bool operator!=(const CPersistentEditInsertPoint& other );
    XP_Bool operator<(const CPersistentEditInsertPoint& other );
    XP_Bool operator<=(const CPersistentEditInsertPoint& other );
    XP_Bool operator>=(const CPersistentEditInsertPoint& other );
    XP_Bool operator>(const CPersistentEditInsertPoint& other );

    XP_Bool IsEqualUI(const CPersistentEditInsertPoint& other );

    // Used for undo
    // delta = this - other;
    void ComputeDifference(CPersistentEditInsertPoint& other, CPersistentEditInsertPoint& delta);
    // result = this + delta;
    void AddRelative(CPersistentEditInsertPoint& delta, CPersistentEditInsertPoint& result);

    ElementIndex m_index;
    // If this insert point falls between two elements, and
    // m_bStickyAfter is TRUE, then this insert point is attached
    // to the start of the second element. If m_bStickyAfter
    // is false, then this insert point is attached to the end of
    // the first element. It is FALSE by default. It is ignored
    // by all of the comparison operators except IsEqualUI. It
    // is ignored by ComputeDifference and AddRelative

     XP_Bool m_bStickyAfter;
};

#ifdef DEBUG
    #define DUMP_PERSISTENT_EDIT_INSERT_POINT(s, pt) { XP_TRACE(("%s %d", s, pt.m_index));}
#else
    #define DUMP_PERSISTENT_EDIT_INSERT_POINT(s, pt) {}
#endif

class CPersistentEditSelection
{
public:
    CPersistentEditSelection();
    CPersistentEditSelection(const CPersistentEditInsertPoint& start, const CPersistentEditInsertPoint& end);
#ifdef DEBUG
    void Print(IStreamOut& stream);
#endif
    XP_Bool operator==(const CPersistentEditSelection& other );
    XP_Bool operator!=(const CPersistentEditSelection& other );
    XP_Bool SelectedRangeEqual(const CPersistentEditSelection& other ); // Ignores m_bFromStart

    ElementIndex GetCount();
    XP_Bool IsInsertPoint();
    CPersistentEditInsertPoint* GetEdge(XP_Bool bEnd);

    // Used for undo
    // change this by the same way that original was changed into modified.
    void Map(CPersistentEditSelection& original, CPersistentEditSelection& modified);

    CPersistentEditInsertPoint m_start;
    CPersistentEditInsertPoint m_end;
    XP_Bool m_bFromStart;
};

#ifdef DEBUG
    #define DUMP_PERSISTENT_EDIT_SELECTION(s, sel) { XP_TRACE(("%s %d-%d %d", s,\
        sel.m_start.m_index,\
        sel.m_end.m_index,\
        sel.m_bFromStart));}
#else
    #define DUMP_PERSISTENT_EDIT_SELECTION(s, sel) {}
#endif

#ifdef DEBUG
void TraceLargeString(char* b);
#endif

//
// Element type used during stream constructor
//
enum EEditElementType {
    eElementNone,
    eElement,
    eTextElement,
    eImageElement,
    eHorizRuleElement,
    eBreakElement,
    eContainerElement,
    eIconElement,
    eTargetElement,
    eListElement,
    eEndElement,
    eTableElement,
    eTableRowElement,
    eTableCellElement,
    eCaptionElement,
    eLayerElement,
    eDivisionElement,
    eInternalAnchorElement
};

//
// Copy type written to stream
//    Used to decide how to paste into existing table
//
enum EEditCopyType {
    eCopyNormal,        // Default = 0. Normal HTML copying, not a table
    eCopyTable,         // Entire table - paste normally or embeded into existing table
    eCopyRows,          // Entire rows - inserts/overlays into existing table
    eCopyColumns,       // Entire columns   "
    eCopyCells          // Arbitrary cells - overlay selected target cells in table
};

//
// CEditElement
//
class CEditElement {
private:
    TagType m_tagType;
    CEditElement* m_pParent;
    CEditElement* m_pNext;
    CEditElement* m_pChild;
    char *m_pTagData;               // hack! remainder of tag data for regen.

protected:
    char* GetTagData(){ return m_pTagData; };
    void SetTagData(char* tagData);

public:

    // ctor, dtor
    CEditElement(CEditElement *pParent, TagType tagType, char* pData = NULL);
    CEditElement(CEditElement *pParent, PA_Tag *pTag, int16 csid);
    CEditElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer);
    virtual ~CEditElement();

    static CEditElement* StreamCtor( IStreamIn *pIn, CEditBuffer *pBuffer );
    static CEditElement* StreamCtorNoChildren( IStreamIn *pIn, CEditBuffer *pBuffer );
    virtual void StreamInChildren(IStreamIn* pIn, CEditBuffer* pBuffer);

    //
    // Accessor functions
    //
    CEditElement* GetParent(){ return m_pParent; }
    CEditElement* GetNextSibling(){ return m_pNext; }
    CEditElement* GetPreviousSibling();
    CEditElement* GetChild(){ return m_pChild; }
    virtual XP_Bool IsAcceptableChild(CEditElement& pChild);
    void SetParent(CEditElement* pParent){m_pParent = pParent;}
    void SetChild(CEditElement *pChild);
    void SetNextSibling(CEditElement* pNext);

    CEditElement* GetLastChild();
    CEditElement* GetLastMostChild();
    CEditElement* GetFirstMostChild();

    CEditCaptionElement* GetCaption(); // Returns containing cell, or NULL if none.
    CEditCaptionElement* GetCaptionIgnoreSubdoc(); // Returns containing cell, or NULL if none.
    CEditTableCellElement* GetTableCell(); // Returns containing cell, or NULL if none.
    CEditTableCellElement* GetTableCellIgnoreSubdoc(); // Returns containing cell, or NULL if none.
    CEditTableRowElement* GetTableRow(); // Returns containing row, or NULL if none.
    CEditTableRowElement* GetTableRowIgnoreSubdoc(); // Returns containing row, or NULL if none.
    CEditTableElement* GetTable(); // Returns containing table, or NULL if none.
    CEditTableElement* GetTableIgnoreSubdoc(); // Returns containing table, or NULL if none.
    CEditLayerElement* GetLayer(); // Returns containing Layer, or NULL if none.
    CEditLayerElement* GetLayerIgnoreSubdoc(); // Returns containing Layer, or NULL if none.
    CEditSubDocElement* GetSubDoc(); // Returns containing sub-doc, or NULL if none.
    CEditSubDocElement* GetSubDocSkipRoot(); // Returns containing sub-doc, or NULL if none.
    CEditRootDocElement* GetRootDoc(); // Returns containing root, or NULL if none.
    XP_Bool InMungableMailQuote(); // Returns true if this element is within a mungable mail quote.
    XP_Bool InMailQuote(); // Returns true if this element is within a mail quote.
    CEditListElement* GetMailQuote(); // Returns containing mail quote or NULL.

    //cmanske A bit more readable
    XP_Bool IsInTableCell() { return NULL != GetTableCellIgnoreSubdoc(); }
    XP_Bool IsInTable() { return NULL != GetTableIgnoreSubdoc(); }

    CEditElement* GetTopmostTableOrLayer(); // Returns topmost containing table or Layer, or NULL if none.
    CEditElement* GetTableOrLayerIgnoreSubdoc(); // Returns containing table or Layer, or NULL if none.
    CEditElement* GetSubDocOrLayerSkipRoot();

    CEditBuffer* GetEditBuffer(); // returns the edit buffer if the element is in one.

    virtual ED_Alignment GetDefaultAlignment();
    virtual ED_Alignment GetDefaultVAlignment();

    TagType GetType(){ return m_tagType; }
    void SetType( TagType t ){ m_tagType = t; }

    // Returns TRUE only if tag has WIDTH or HEIGHT already defined
    // Use NULL for params if you don't need those values
    XP_Bool GetWidth(XP_Bool * pPercent = NULL, int32 * pWidth = NULL);
    XP_Bool GetHeight(XP_Bool * pPercent = NULL, int32 * pHeight = NULL);
    
    virtual void SetSize(XP_Bool bWidthPercent, int32 iWidth,
                         XP_Bool bHeightPercent, int32 iHeight);
	
	virtual XP_Bool CanReflow();
	
    //
    // Cast routines.
    //
    CEditTextElement* Text();

    virtual XP_Bool IsLeaf();
    CEditLeafElement* Leaf();

    virtual XP_Bool IsRoot();
    CEditRootDocElement* Root();

    virtual XP_Bool IsContainer();
    CEditContainerElement* Container();

    //get previous non empty container to querry for preopen conditions ect
    CEditContainerElement*  GetPreviousNonEmptyContainer();
    //get next non empty container to querry for preclose conditions ect
    CEditContainerElement*  GetNextNonEmptyContainer();

    virtual XP_Bool IsList();
    CEditListElement* List();

    virtual XP_Bool IsBreak();
    CEditBreakElement* Break();

    virtual XP_Bool CausesBreakBefore();
    virtual XP_Bool CausesBreakAfter();
    virtual XP_Bool AllowBothSidesOfGap();

    virtual XP_Bool IsImage();
    CEditImageElement* Image();
    virtual XP_Bool IsIcon();
    CEditIconElement* Icon();

    CEditTargetElement* Target();

    CEditHorizRuleElement* HorizRule();
    virtual XP_Bool IsRootDoc();
    CEditRootDocElement* RootDoc();
    virtual XP_Bool IsSubDoc();
    CEditSubDocElement* SubDoc();
    virtual XP_Bool IsTable();
    CEditTableElement* Table();
    virtual XP_Bool IsTableRow();
    CEditTableRowElement* TableRow();
    virtual XP_Bool IsTableCell();
    CEditTableCellElement* TableCell();
    virtual XP_Bool IsCaption();
    CEditCaptionElement* Caption();
    virtual XP_Bool IsText();
    virtual XP_Bool IsLayer();
    CEditLayerElement* Layer();
    virtual XP_Bool IsDivision();
    CEditDivisionElement* Division();

    XP_Bool IsEndOfDocument();
    virtual XP_Bool IsEndContainer();

    // Can this element contain a paragraph?
    virtual XP_Bool IsContainerContainer();

    XP_Bool IsA( int tagType ){ return m_tagType == tagType; }
    XP_Bool Within( int tagType );
    XP_Bool InFormattedText();
    CEditContainerElement* FindContainer(); // Returns this if this is a container.
    CEditContainerElement* FindEnclosingContainer(); // Starts search with parent of this.
    CEditElement* FindList();
    void FindList( CEditContainerElement*& pContainer, CEditListElement*& pList );
    CEditElement* FindContainerContainer();

    //
    // Tag Generation functions
    //
    virtual PA_Tag* TagOpen( int iEditOffset );
    virtual PA_Tag* TagEnd( );
    void SetTagData( PA_Tag* pTag, char* pTagData);

    //
    // Output routines.
    //
    virtual void PrintOpen( CPrintState *pPrintState );
    void InternalPrintOpen( CPrintState *pPrintState, char* pTagData );
    virtual void PrintEnd( CPrintState *pPrintState );
    virtual void StreamOut( IStreamOut *pOut );
    virtual void PartialStreamOut( IStreamOut *pOut, CEditSelection& selection );
    virtual XP_Bool ShouldStreamSelf( CEditSelection& local, CEditSelection& selection);
    virtual void StreamToPositionalText( IStreamOut * /*pOut*/, XP_Bool /*bEnd*/ ){}


    virtual EEditElementType GetElementType();

    // Get parent table of any element
    CEditTableElement* GetParentTable();

    virtual XP_Bool Reduce( CEditBuffer *pBuffer );

    // Selection utilities
    XP_Bool ClipToMe(CEditSelection& selection, CEditSelection& local); // Returns TRUE if selection intersects with "this".
    virtual void GetAll(CEditSelection& selection); // Get selection that selects all of this.
    
    //cmanske: Used only by CEditTableElement and CEditTableCellElement
    virtual XP_Bool IsSelected() {return FALSE; }

    // iterators
    typedef XP_Bool (CEditElement::*PMF_EditElementTest)(void*);

    CEditElement* FindNextElement( PMF_EditElementTest pmf, void *pData );
    CEditElement* UpRight( PMF_EditElementTest pmf, void *pTestData );
    CEditElement* DownRight( PMF_EditElementTest pmf,void *pTestData,
            XP_Bool bIgnoreThis = FALSE );

    CEditElement* FindPreviousElement( PMF_EditElementTest pmf, void *pTestData );
    CEditElement* UpLeft( PMF_EditElementTest pmf, void *pTestData );
    CEditElement* DownLeft( PMF_EditElementTest pmf, void *pTestData,
            XP_Bool bIgnoreThis = FALSE );

    XP_Bool TestIsTrue( PMF_EditElementTest pmf, void *pTestData ){
                return (this->*pmf)(pTestData); }

    //
    // Search routines to be applied FindNextElement and FindPreviousElement
    //
    XP_Bool FindText( void* /*pVoid*/ );
    XP_Bool FindTextAll( void* /*pVoid*/ );
    XP_Bool FindLeaf( void* /*pVoid*/ );
    XP_Bool FindLeafAll( void* /*pVoid*/ );
    XP_Bool FindImage( void* /*pvoid*/ );
    XP_Bool FindTarget( void* /*pvoid*/ );
    XP_Bool FindUnknownHTML( void* /*pvoid*/ );
    XP_Bool SplitContainerTest( void* /*pVoid*/ );
    XP_Bool SplitFormattingTest( void* pVoid );
    XP_Bool FindTable( void* /*pvoid*/ );
    XP_Bool FindTableRow( void* /*pvoid*/ );
    XP_Bool FindTableCell( void* /*pvoid*/ );
    

    XP_Bool FindContainer( void* /*pVoid*/ );

    //
    // Split the tree.  You can stop when pmf returns true.
    //
    virtual CEditElement* Split(CEditElement *pLastChild,
                                CEditElement* pCloneTree,
                                PMF_EditElementTest pmf,
                                void *pData);

    virtual CEditElement* Clone( CEditElement *pParent = 0 );
    CEditElement* CloneFormatting( TagType endType );
    virtual void Unlink();
    virtual void Merge( CEditElement *pAppendBlock, XP_Bool bCopyAppendAttributes = TRUE );


#ifdef DEBUG
    virtual void ValidateTree();
#endif
    CEditElement* InsertAfter( CEditElement *pPrev );
    CEditElement* InsertBefore( CEditElement *pNext );
    void InsertAsFirstChild( CEditElement *pParent );
    void InsertAsLastChild( CEditElement *pParent );

    virtual XP_Bool IsFirstInContainer();
    CEditTextElement* TextInContainerAfter();
    CEditTextElement* PreviousTextInContainer();
    virtual CEditElement* Divide(int iOffset);

    CEditLeafElement* PreviousLeafInContainer();
    CEditLeafElement* LeafInContainerAfter();

    CEditLeafElement* PreviousLeaf() { return (CEditLeafElement*) FindPreviousElement( &CEditElement::FindLeaf, 0 ); }
    CEditLeafElement* NextLeaf() { return (CEditLeafElement*) FindNextElement( &CEditElement::FindLeaf, 0 ); }
    CEditLeafElement* NextLeafAll() { return (CEditLeafElement*) FindNextElement( &CEditElement::FindLeafAll, 0 ); }
    CEditLeafElement* NextLeafAll(XP_Bool bForward);
    CEditContainerElement* NextContainer() { return (CEditContainerElement*) FindNextElement( &CEditElement::FindContainer, 0 ); }
    CEditContainerElement* PreviousContainer() { return (CEditContainerElement*) FindPreviousElement( &CEditElement::FindContainer, 0 ); }

    CEditElement* GetRoot();
    CEditElement* GetCommonAncestor(CEditElement* pOther); // NULL if not in same tree
    CEditElement* GetChildContaining(CEditElement* pDescendant); // NULL if not a descendant

    XP_Bool DeleteElement( CEditElement *pTellIfKilled );
    void DeleteChildren();

    // should be in Container class
    int GetDefaultFontSize();

    // To and from persistent insert points
    virtual CEditInsertPoint IndexToInsertPoint(ElementIndex index, XP_Bool bStickyAfter);
    virtual CPersistentEditInsertPoint GetPersistentInsertPoint(ElementOffset offset);
    virtual ElementIndex GetElementIndexOf(CEditElement* child); // asserts if argument is not a child.
    virtual ElementIndex GetPersistentCount();
    ElementIndex GetElementIndex();

    virtual void FinishedLoad( CEditBuffer* pBuffer );
    virtual void AdjustContainers( CEditBuffer* pBuffer );

	int16 GetWinCSID(); // International Character set ID

protected:
    virtual void Finalize();
    void EnsureSelectableSiblings(CEditBuffer* pBuffer);

private:
    void CommonConstructor();
    // For CEditElement constructor, where the vtable isn't
    // set up enough to check if the item is acceptable.
    void RawSetNextSibling(CEditElement* pNext) {m_pNext = pNext; }
    void RawSetChild(CEditElement* pChild) {m_pChild = pChild; }
};


//
// CEditSubDocElement - base class for the root, captions, and table data cells
//
class CEditSubDocElement: public CEditElement {
public:
    CEditSubDocElement(CEditElement *pParent, int tagType, char* pData = NULL);
    CEditSubDocElement(CEditElement *pParent, PA_Tag *pTag, int16 csid);
    CEditSubDocElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer);

    virtual XP_Bool IsSubDoc() { return TRUE; }
    static CEditSubDocElement* Cast(CEditElement* pElement) {
        return pElement && pElement->IsSubDoc() ? (CEditSubDocElement*) pElement : 0; }

    virtual XP_Bool Reduce( CEditBuffer *pBuffer );
    virtual XP_Bool IsAcceptableChild(CEditElement& pChild){
            return ! pChild.IsLeaf();
        }
    virtual void FinishedLoad( CEditBuffer* pBuffer );
};

class CEditTableElement: public CEditElement {
public:
    CEditTableElement(intn columns, intn rows);
    CEditTableElement(CEditElement *pParent, PA_Tag *pTag, int16 csid, ED_Alignment align);
    CEditTableElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer);
    virtual ~CEditTableElement();

    virtual XP_Bool IsTable() { return TRUE; }
    static CEditTableElement* Cast(CEditElement* pElement) {
        return pElement && pElement->IsTable() ? (CEditTableElement*) pElement : 0; }
    virtual EEditElementType GetElementType() { return eTableElement; }
    virtual XP_Bool IsAcceptableChild(CEditElement& pChild){
            return pChild.IsTableRow() || pChild.IsCaption();
        }
    virtual void FinishedLoad( CEditBuffer* pBuffer );
    void AdjustCaption();
    virtual PA_Tag* TagOpen(int iEditOffset);
    PA_Tag* InternalTagOpen(int iEditOffset, XP_Bool bPrinting);
    virtual PA_Tag* TagEnd();

    void InsertRows(int32 Y, int32 newY, intn number, CEditTableElement* pSource = NULL);
    void InsertColumns(int32 X, int32 newX, intn number, CEditTableElement* pSource = NULL);
//cmanske: I doubt we will ever revive the old undo system, so skip it here
//    void DeleteRows(int32 Y, intn number, CEditTableElement* pUndoContainer = NULL);
    void DeleteRows(int32 Y, intn number);
    void DeleteColumns(int32 X, intn number, CEditTableElement* pUndoContainer = NULL);

    // Not used much - index based
    CEditTableRowElement* FindRow(intn number);

    // Find first row containing a cell that has Y as its top
    // Optionally return the "logical" row index
    CEditTableRowElement* GetRow(int32 Y, intn *pRow = NULL);

    // Find first geometric cell that is at or spans given Y value
    // The logical row is written to m_iGetRow
    CEditTableCellElement* GetFirstCellInRow(int32 Y, XP_Bool bSpan = FALSE);
    CEditTableCellElement* GetFirstCellInRow(CEditTableCellElement *pCell, XP_Bool bSpan = FALSE);
    // Find next cell using the supplied cell's Y value 
    //  (bSpan is FALSE, so Y must match exactly)
    // If pCell is NULL, use the same Y and bSpan as used for GetFirstCellInRow()
    // The logical row is written to the element's m_iGetRow
    // Note: If cell has ROWSPAN > 1, this will skip the next actual row
    CEditTableCellElement* GetNextCellInRow(CEditTableCellElement *pCell = NULL);
    
    // Find previous and cell using supplied cell's Y value 
    //  (does not depend on data from GetFirstCellInRow)
    CEditTableCellElement* GetPreviousCellInRow(CEditTableCellElement *pCell);
    CEditTableCellElement* GetLastCellInRow(CEditTableCellElement *pCell);

    // Same as above, but finds columns
    CEditTableCellElement* GetFirstCellInColumn(int32 X, XP_Bool bSpan = FALSE);
    CEditTableCellElement* GetFirstCellInColumn(CEditTableCellElement *pCell, XP_Bool bSpan = FALSE);
    // Note: If cell has COPSPAN > 1, this will skip the next actual column
    CEditTableCellElement* GetNextCellInColumn(CEditTableCellElement *pCell = NULL);
    
    // Find previous and last cell using supplied cell's X value 
    //  (does not depend on data from GetFirstCellInRow)
    CEditTableCellElement* GetPreviousCellInColumn(CEditTableCellElement *pCell);
    CEditTableCellElement* GetLastCellInColumn(CEditTableCellElement *pCell);

    // Find first cell in column or row exactly at the specified col or row index grid
    CEditTableCellElement *GetFirstCellAtColumnIndex(intn iIndex);
    CEditTableCellElement *GetFirstCellAtRowIndex(intn iIndex);

    // This gets the next geometric column or row 
    //  using supplied X or Y value as current location
    // (these use the tables m_RowLayoutData and m_ColumnLayoutData)
    CEditTableCellElement* GetFirstCellInNextColumn(int32 iCurrentColumnX);
    CEditTableCellElement* GetFirstCellInNextRow(int32 iCurrentRowY);

    // Get the defining left (for columns) and top (for rows) value from
    //  the index into m_ColumnLayoutData and m_RowLayoutData 
    int32 GetColumnX(intn iIndex);
    int32 GetRowY(intn iIndex);

    // Use m_ColumnLayoutData and m_RowLayoutData
    // to get grid coordinates of a cell
    intn GetColumnIndex(int32 X);
    intn GetRowIndex(int32 Y);

    CEditCaptionElement* GetCaption();
    void SetCaption(CEditCaptionElement*);
    void DeleteCaption();

    void SetData( EDT_TableData *pData );
    EDT_TableData* GetData();
    EDT_TableData* ParseParams( PA_Tag *pTag, int16 csid );
    static EDT_TableData* NewData();
    static void FreeData( EDT_TableData *pData );

    void SetBackgroundColor( ED_Color iColor );
    ED_Color GetBackgroundColor();
    void SetBackgroundImage( char* pImage );
    char* GetBackgroundImage();

    // Override printing and taging, since we don't allow ALIGN tag when editing.
    virtual void PrintOpen( CPrintState *pPrintState );
    virtual void PrintEnd( CPrintState *pPrintState );
    char* CreateTagData(EDT_TableData *pData, XP_Bool bPrinting);

    virtual void StreamOut( IStreamOut *pOut);

    //cmanske:
    // Get corresponding Layout table element
    LO_TableStruct* GetLoTable();

    // Get width of window view or parent cell of a table
    // Supply pLoTable if known, else find it
    void GetParentSize(MWContext *pContext, int32 *pWidth, int32 *pHeight = NULL, LO_TableStruct *pLoTable = NULL);

    // See edttypes.h for dConvert table and all rows and cells to Percent mode or not
    // Current bWidthPercent, bHeightPercent, bWidthDefined, and bHeightDefined are saved...
    void SetSizeMode(MWContext *pContext, int iTableMode);
    // ... to be restored by this after relayout after table, col, or row resizing
    void RestoreSizeMode(MWContext *pContext);

    // Next two use m_RowLayoutData and m_ColumnLayoutData
    // Clear all row and column layout data
    void DeleteLayoutData();
    
    // Add new cell to column and/or row layout data
    void AddLayoutData(CEditTableCellElement *pEdCell, LO_Element *pLoCell);

    int32 GetCellSpacing() {return m_iCellSpacing; }
    int32 GetCellPadding() {return m_iCellPadding; }
    int32 GetCellBorder() {return m_iCellBorder; }

    int32 GetWidth() {return m_iWidthPixels; }
    int32 GetHeight() {return m_iHeightPixels; }
    int32 GetWidthOrPercent() { return m_iWidth; }
    int32 GetHeightOrPercent() { return m_iHeight; }

    // Get first cell in table. 
    //  Use GetNextCellInTable() or resulting pCell->GetNextCellInTable() 
    //     to walk through the table, wrapping around rows,
    //  or resultCell->pGetNextCellInLogicalRow() to walk through cells in the row
    CEditTableCellElement* GetFirstCell();

    // Result from above is stored in m_pCurrentCell    
    CEditTableCellElement* GetNextCellInTable(intn *pRowCounter = NULL);

    // Analogous routine for rows
    CEditTableRowElement* GetFirstRow();

    // Use following BEFORE we layout and build our m_RowLayoutData and m_ColumnLayoutData    
    // This counts row elements in table and also saves result in m_iRows
    intn CountRows();
    // Max of number of columns 
    // (may be more than number of cells in any particular
    //  row because of ROWSPAN effect)
    intn CountColumns();

    intn GetRows()     {m_iRows = m_RowLayoutData.Size(); return m_iRows;}
    intn GetColumns()  {m_iColumns = m_ColumnLayoutData.Size(); return m_iColumns;}
    
    // Get number of columns between given values    
    intn GetColumnsSpanned(int32 iStartX, int32 iEndX);

    // Test if any cell in the first row has COLSPAN > 1
    XP_Bool FirstRowHasColSpan();

    
    // 1. Fixup COLSPAN and ROWSPAN errors, 
    //    If all cells in a column have COLSPAN > 1,
    //    that is bad, so fix it. Same for ROWSPAN
    // 2. Save accurate number of cells in each column,
    //    compensating for COLSPAN and ROWSPAN
    //    Uses m_ColumnLayoutData and m_RowLayoutData
    void FixupColumnsAndRows();

private:
    ED_Color m_backgroundColor;
    char* m_pBackgroundImage;
    ED_Alignment m_align;
    ED_Alignment m_malign;
    
    //Actual width, as determined from layout
    // See comments for similar params in CEditTableCellElement
    int32   m_iWidthPixels;       
    // This is % of parent (when bWidthPercent = TRUE) or absolute (in pixels)
    XP_Bool m_iWidth;
    XP_Bool m_bWidthPercent;       

    int32   m_iHeightPixels;       
    int32   m_iHeight;
    XP_Bool m_bHeightPercent;       

    // Pixels between cells
    int32   m_iCellSpacing;
    // Space between border and cell contents
    int32   m_iCellPadding;
    // Cell border is 1 if table border is > 0
    int32   m_iCellBorder;

    // Save last value of m_bWidthPercent, m_bWidthDefined,
    //  bHeightPercent, and bHeightDefined here
    //   during table, col, and row resizing.
    // Call RestoreTableSizeMode to reset back to these values
    XP_Bool m_bSaveWidthPercent;       
    XP_Bool m_bSaveHeightPercent;
    XP_Bool m_bSaveWidthDefined;       
    XP_Bool m_bSaveHeightDefined;

    // Maximum number of "geometric" columns
    // (May be > number in individual rows because of ROWSPAN)
    int32   m_iColumns;
    int32   m_iRows;

    // Used by GetFirstCell() and GetNextCellInTable()
    CEditTableCellElement *m_pCurrentCell;

    // For use with GetFirstCellInRow/Column and GetNextCellInRow/Column
    CEditTableCellElement *m_pFirstCellInColumnOrRow;
    CEditTableCellElement *m_pNextCellInColumnOrRow;
    int32                  m_iColX;
    int32                  m_iRowY;
    XP_Bool                m_bSpan;
    intn                   m_iGetRow;

public:
    intn m_iBackgroundSaveIndex;

    // Let CEditBuffer access this directly
    TXP_GrowableArray_EDT_CellLayoutData m_ColumnLayoutData;
    TXP_GrowableArray_EDT_CellLayoutData m_RowLayoutData;

};

class CEditTableRowElement: public CEditElement {
public:
    CEditTableRowElement();
    CEditTableRowElement(intn cells);
    CEditTableRowElement(CEditElement *pParent, PA_Tag *pTag, int16 csid);
    CEditTableRowElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer);
    virtual ~CEditTableRowElement();

    virtual XP_Bool IsTableRow() { return TRUE; }
    static CEditTableRowElement* Cast(CEditElement* pElement) {
        return pElement && pElement->IsTableRow() ? (CEditTableRowElement*) pElement : 0; }
    virtual EEditElementType GetElementType() { return eTableRowElement; }
    virtual XP_Bool IsAcceptableChild(CEditElement& pChild){
            return pChild.IsTableCell();
        }
    virtual void FinishedLoad( CEditBuffer* pBuffer );

    intn GetCells();
    // Use actual cell X and Y for Insert and Delete logic
    void InsertCells(int32 X, int32 newX, intn number, CEditTableRowElement* pSource = NULL);
    // Append (move) cells in supplied row to this row and delete supplied row if requested
    // (Note: Unlike most other "insert" methods, "this" is the existing row, not row to be inserted)
    // Returns the number of cells "positions" appended (sum of COLSPAN of appended cells)
    intn AppendRow( CEditTableRowElement *pAppendRow, XP_Bool bDeleteRow = TRUE );
    // Add extra empty cells to fill out a row to specified amount
    //   or use current table's max columns if iColumns is 0
    void PadRowWithEmptyCells(intn iColumns = 0);

    void DeleteCells(int32 X, intn number, CEditTableRowElement* pUndoContainer = NULL);
    // Find cell at specific location in column
    CEditTableCellElement* FindCell(int32 X, XP_Bool bIncludeRightEdge = FALSE);

    void SetData( EDT_TableRowData *pData );
    EDT_TableRowData* GetData( );
    static EDT_TableRowData* ParseParams( PA_Tag *pTag, int16 csid );
    static EDT_TableRowData* NewData();
    static void FreeData( EDT_TableRowData *pData );
    void SetBackgroundColor( ED_Color iColor );
    ED_Color GetBackgroundColor();
    void SetBackgroundImage( char* pImage );
    char* GetBackgroundImage();
    // Not sure if we need this, but it will be useful
    //   for padding tables with ragged right edges
    intn GetColumns() {return m_iColumns; }
    void SetColumns(intn iColumns) {m_iColumns = iColumns; }

    CEditTableRowElement *GetNextRow();
    // Use resultCell->pGetNextCellInLogicalRow() to walk through cells in row
    CEditTableCellElement *GetFirstCell();

private:
    ED_Color m_backgroundColor;
    char* m_pBackgroundImage;
    // Total number of "physical" columns in row
    //  (i.e., includes effect of COLSPAN and ROWSPAN)
    intn m_iColumns;

public:
    intn m_iBackgroundSaveIndex;

};

class CEditCaptionElement: public CEditSubDocElement {
public:
    CEditCaptionElement();
    CEditCaptionElement(CEditElement *pParent, PA_Tag *pTag, int16 csid);
    CEditCaptionElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer);

    virtual XP_Bool IsCaption() { return TRUE; }
    static CEditCaptionElement* Cast(CEditElement* pElement) {
        return pElement && pElement->IsCaption() ? (CEditCaptionElement*) pElement : 0; }
    virtual EEditElementType GetElementType() { return eCaptionElement; }

    void SetData( EDT_TableCaptionData *pData );
    EDT_TableCaptionData* GetData( );
    EDT_TableCaptionData* GetData( int16 csid );
    static EDT_TableCaptionData* ParseParams( PA_Tag *pTag, int16 csid );
    static EDT_TableCaptionData* NewData();
    static void FreeData( EDT_TableCaptionData *pData );
};

// Table data and Table header

class CEditTableCellElement: public CEditSubDocElement {
public:
    CEditTableCellElement();
    CEditTableCellElement(XP_Bool bIsHeader);
    CEditTableCellElement(CEditElement *pParent, PA_Tag *pTag, int16 csid);
    CEditTableCellElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer);
    virtual ~CEditTableCellElement();

    virtual XP_Bool IsTableCell();
    static CEditTableCellElement* Cast(CEditElement* pElement) {
        return pElement && pElement->IsTableCell() ? (CEditTableCellElement*) pElement : 0; }
    virtual EEditElementType GetElementType();
    virtual ED_Alignment GetDefaultAlignment();

    XP_Bool IsTableData();

    virtual void StreamOut(IStreamOut *pOut);

    void SetData( EDT_TableCellData *pData );
    // Supply the csid if getting data for table not
    //  yet part of doc, as when pasting from stream
    EDT_TableCellData* GetData(int16 csid = 0);

    // Clear mask bits for attributes different from supplied data
    void MaskData( EDT_TableCellData *pData );
    
    EDT_TableCellData* ParseParams( PA_Tag *pTag, int16 csid );
    static EDT_TableCellData* NewData();
    static void FreeData( EDT_TableCellData *pData );
    void SetBackgroundColor( ED_Color iColor );
    ED_Color GetBackgroundColor();
    void SetBackgroundImage( char* pImage );
    char* GetBackgroundImage();

    //cmanske:
    // Get corresponding Layout struct
    LO_CellStruct* GetLoCell();

    // Next cell in table - wraps around end of each
    //  row until end of table is found (returns null)
    // If pRowCount is supplied, value it is incremented
    //   when cell returned is 1st one in a next row
    CEditTableCellElement* GetNextCellInTable(intn *pRowCounter = NULL);
    // Similar to above, but finds previous cell until beginning is found (returns null)
    // If pRowCount is supplied, value it is decremented
    //   when cell returned is 1st one in a previous row
    CEditTableCellElement* GetPreviousCellInTable(intn *pRowCounter = NULL);
    
    // This gets next cell in the same logical row
    CEditTableCellElement* GetNextCellInLogicalRow();

    // Find first geometric cell that is at 
    //  given X (for column) or Y (for row)value
    // GetNext... continue using same X or Y
    // (These just call same functions in CEditTableElement)
    // If X or Y is not supplied, it is taken from "this" cell's X or Y
    CEditTableCellElement* GetFirstCellInRow(int32 Y = -1, XP_Bool bSpan = FALSE);
    CEditTableCellElement* GetFirstCellInColumn(int32 X = -1, XP_Bool bSpan = FALSE);
    CEditTableCellElement* GetNextCellInRow(CEditTableCellElement *pCell = NULL);
    CEditTableCellElement* GetNextCellInColumn(CEditTableCellElement *pCell = NULL);

    XP_Bool AllCellsInColumnAreSelected();
    XP_Bool AllCellsInRowAreSelected();

    // Test if cell contains only 1 container with 
    //  just the empty text field every new cell has
    XP_Bool IsEmpty();

    // Keep these in synch when selecting so
    //  we don't always have to rely on LO_Elements
    XP_Bool IsSelected() { return m_bSelected; }
    void    SetSelected(XP_Bool bSelected) { m_bSelected = bSelected; }

    // Move contents of supplied cell into this cell
    void MergeCells(CEditTableCellElement* pCell);

    // Delete all contents, leaving just the minimum empty text element
    // Set param to TRUE only when deleting all selected cells
    // using CEditBuffer::DeleteSelectedCells()
    void DeleteContents(XP_Bool bMarkAsDeleted = FALSE);

    inline XP_Bool IsDeleted() { return m_bDeleted; }
    inline void SetDeleted(XP_Bool bDeleted) {m_bDeleted = bDeleted;}

    // Get all text from a cell. If bJoinParagrphs is FALSE,
    //  appropriate CR/LF is added between containers,
    //  else they are appended without CR/LF so cell contents
    //  can be pasted into Excell spreadsheet
    char * GetText(XP_Bool bJoinParagraphs = FALSE);

    // Returns sum of widths of all cells in first row in table
    int32 GetParentWidth();
    // Sum of heights of all cells in first column of table
    int32 GetParentHeight();

    int32 GetX() { return m_X; }
    int32 GetY() { return m_Y; }
    intn  GetRow() { return m_iRow; }
    int32 GetWidth() { return m_iWidthPixels; }
    int32 GetHeight() { return m_iHeightPixels; }
    int32 GetWidthOrPercent() { return m_iWidth; }
    int32 GetHeightOrPercent() { return m_iHeight; }
    int32 GetColSpan() { return m_iColSpan; }
    int32 GetRowSpan() { return m_iRowSpan; }
    XP_Bool GetWidthPercent() { return m_bWidthPercent; }
    XP_Bool GetHeightPercent() { return m_bHeightPercent; }

    // This gets the width including the cell border,
    //   padding inside cell, and inter-cell space to the next cell
    // Supply pTable if available for efficiency
    int32 GetFullWidth(CEditTableElement *pTable = NULL);
    int32 GetFullHeight(CEditTableElement *pTable = NULL);

    // These will do appropriate action using SetData
    void IncreaseColSpan(int32 iIncrease);
    void IncreaseRowSpan(int32 iIncrease);
    void DecreaseColSpan(int32 iDecrease);
    void DecreaseRowSpan(int32 iDecrease);

    // Insert as last child of supplied parent 
    //   but first save current parent and next pointers
    void SwitchLinkage(CEditTableRowElement *pParentRow);
    // This switches parent and next pointers
    //   to those saved during SwitchLinkage
    void RestoreLinkage();

    // Save current percent and "defined" values -- do this before resizing, which may need
    //  to change the mode for better control of resizing
    // Since this is always called when we know the cell's data,
    //  supply the bWidthDefined and bHeightDefined values
    //  (percent mode params are held in class member variables)
    void SaveSizeMode(XP_Bool bWidthDefined, XP_Bool bHeightDefined);
    
    // Restore saved width and height percent modes,
    //   readjust m_bWidth an m_bHeight, and call SetData to set tag data
    void RestoreSizeMode(int32 iParentWidth, int32 iParentHeight);

    // Calculate the Percent iWidth or iHeight from the 
    //  iWidthPixels and iHeightPixels in pData...
    void CalcPercentWidth(EDT_TableCellData *pData);
    void CalcPercentHeight(EDT_TableCellData *pData);

    // ...vice versa
    void CalcPixelWidth(EDT_TableCellData *pData);
    void CalcPixelHeight(EDT_TableCellData *pData);

    void SetWidth(XP_Bool bWidthDefined, XP_Bool bWidthPercent, int32 iWidthPixels);
    void SetHeight(XP_Bool bHeightDefined, XP_Bool bHeightPercent, int32 iHeightPixels);

    // Next two are used when dragging the right border
    // Set all cells in a column to the width supplied
    void SetColumnWidthRight(CEditTableElement *pTable, LO_Element *pLoCell, EDT_TableCellData *pData);
    // Set all cells in a row to the width params supplied in pData
    void SetRowHeightBottom(CEditTableElement *pTable, LO_Element *pLoCell, EDT_TableCellData *pData);

    // Next two are use when resizing inside table cell property dialog
    // Supplying bWidthDefined or bHeightDefined allows clearing this for entire col or row
    // Set all cells in a column to the width supplied
    void SetColumnWidthLeft(CEditTableElement *pTable, CEditTableCellElement *pEdCell, EDT_TableCellData *pData);
    // Set all cells in a row to the width supplied
    void SetRowHeightTop(CEditTableElement *pTable, CEditTableCellElement *pEdCell, EDT_TableCellData *pData);

private:
    ED_Color m_backgroundColor;
    char* m_pBackgroundImage;

    // Cache this for efficiency
    XP_Bool m_bWidthPercent;       
    XP_Bool m_bHeightPercent;
    int32   m_iColSpan;
    int32   m_iRowSpan;

    // These are set only by FixupTableData(),
    //FIGURING OUT INDEXES IS TOO COMPLICATED!
    //  USE REAL LOCATIONS INSTEAD
    int32 m_X;
    int32 m_Y;
    intn m_iRow;    // Current logical and actual row index

    //Actual width, as determined from layout
    //Set by CEditBuffer::FixupTableData() after a table is layed out
    // We need this because tag data is not recorded
    // if m_bWidthDefined is FALSE, but we want to be able
    // to have the actual width available as default when
    // user edits the table or cell data
    int32   m_iWidthPixels;       
    int32   m_iHeightPixels;       

    // This is actual width or height in pixels, or Percent if b[Width|Height]Percent = TRUE;
    int32   m_iWidth;       
    int32   m_iHeight;       

    // Save last value of m_bWidthPercent, m_bWidthDefined,
    //  bHeightPercent, and bHeightDefined here
    //   during table, col, and row resizing.
    // Call RestoreTableSizeMode to reset back to these values
    XP_Bool m_bSaveWidthPercent;       
    XP_Bool m_bSaveHeightPercent;
    XP_Bool m_bSaveWidthDefined;       
    XP_Bool m_bSaveHeightDefined;

    // This is used to temporarily switch cell to 
    //  a table created for copying contents
    // These are used by SwitchLinkage() and RestoreLinkage()
    CEditElement *m_pSaveParent;
    CEditElement *m_pSaveNext;

    // This should be TRUE only if cell is also in CEditBuffer::m_SelectedEdCells list
    XP_Bool       m_bSelected;
    
    // Deleting an arbitrary set of cells is tricky 
    // since some cells only have contents deleted,
    // and we want to leave those selected, so we can't rely on 
    // m_SeletectedEdCells list to be empty after "deleting" all cells
    // This marks cells to be skipped over on successive recursions.
    // Must call CEditTable::ResetDeletedCells() to clear these flags when done
    XP_Bool      m_bDeleted;

public:
    intn m_iBackgroundSaveIndex;
};

class CEditLayerElement: public CEditElement {
public:
    CEditLayerElement();
    CEditLayerElement(CEditElement *pParent, PA_Tag *pTag, int16 csid);
    CEditLayerElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer);

    virtual XP_Bool IsLayer() { return TRUE; }
    static CEditLayerElement* Cast(CEditElement* pElement) {
        return pElement && pElement->IsLayer() ? (CEditLayerElement*) pElement : 0; }
    virtual EEditElementType GetElementType() { return eLayerElement; }
    virtual XP_Bool IsAcceptableChild(CEditElement& pChild){
            return pChild.IsContainer();
        }
    virtual void FinishedLoad( CEditBuffer* pBuffer );

    void SetData( EDT_LayerData *pData );
    EDT_LayerData* GetData( );
    static EDT_LayerData* ParseParams( PA_Tag *pTag, int16 csid );
    static EDT_LayerData* NewData();
    static void FreeData( EDT_LayerData *pData );

    // Temporary methods. Remove when we can display layers.
    virtual void PrintOpen( CPrintState *pPrintState );
    virtual void PrintEnd( CPrintState *pPrintState );


private:
};

class CEditDivisionElement: public CEditElement {
public:
    CEditDivisionElement();
    CEditDivisionElement(CEditElement *pParent, PA_Tag *pTag, int16 csid);
    CEditDivisionElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer);

    virtual XP_Bool IsDivision();
    static CEditDivisionElement* Cast(CEditElement* pElement);
    virtual EEditElementType GetElementType();
    virtual XP_Bool IsAcceptableChild(CEditElement& pChild);

    ED_Alignment GetAlignment();
    void ClearAlignment();
    XP_Bool HasData();

    void SetData( EDT_DivisionData *pData );
    EDT_DivisionData* GetData( );
    static EDT_DivisionData* ParseParams( PA_Tag *pTag, int16 csid );
    static EDT_DivisionData* NewData();
    static void FreeData( EDT_DivisionData *pData );

    virtual void PrintOpen( CPrintState *pPrintState );
    virtual void PrintEnd( CPrintState *pPrintState );

private:
};

//
// CEditRootDocElement - Top most element in the tree, tag HTML
//
class CEditRootDocElement: public CEditSubDocElement {
private:
    CEditBuffer* m_pBuffer;
public:
    CEditRootDocElement( CEditBuffer* pBuffer ):
            CEditSubDocElement(0, P_MAX), m_pBuffer( pBuffer ){}
    virtual XP_Bool Reduce( CEditBuffer * /*pBuffer */ ){ return FALSE; }
    virtual XP_Bool ShouldStreamSelf( CEditSelection& /*local*/, CEditSelection& /*selection*/ ) { return FALSE; }

    virtual void FinishedLoad( CEditBuffer *pBuffer );

    virtual void PrintOpen( CPrintState *pPrintState );
    virtual void PrintEnd( CPrintState *pPrintState );

    virtual XP_Bool IsRoot() { return TRUE; }
    static CEditRootDocElement* Cast(CEditElement* pElement) {
        return pElement && pElement->IsRoot() ? (CEditRootDocElement*) pElement : 0; }
    CEditBuffer* GetBuffer(){ return m_pBuffer; }
#ifdef DEBUG
    virtual void ValidateTree();
#endif
};

class CEditLeafElement: public CEditElement {
private:
    ElementOffset m_iSize;        // fake size.
public:
    // pass through constructors.
    CEditLeafElement(CEditElement *pParent, int tagType):
            CEditElement(pParent,tagType),m_iSize(1){}

    CEditLeafElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer):
            CEditElement(pStreamIn,pBuffer),m_iSize(1){}

    //
    // CEditElement Stuff
    //
    virtual XP_Bool IsLeaf() { return TRUE; }
    static CEditLeafElement* Cast(CEditElement* pElement) {
        return pElement && pElement->IsLeaf() ? (CEditLeafElement*) pElement : 0; }
    virtual PA_Tag* TagOpen( int iEditOffset );

    virtual void GetAll(CEditSelection& selection);
    virtual XP_Bool IsAcceptableChild(CEditElement& pChild );
    virtual ElementIndex GetPersistentCount();
    virtual void StreamToPositionalText( IStreamOut *pOut, XP_Bool bEnd );

    virtual XP_Bool IsContainerContainer();
    virtual XP_Bool IsFirstInContainer();

    //
    // Stuff created at this level (Leaf)
    //
    virtual void SetLayoutElement( intn iEditOffset, intn lo_type,
                LO_Element* pLoElement )=0;
    virtual void ResetLayoutElement( intn iEditOffset,
                LO_Element* pLoElement )=0;
    virtual LO_Element* GetLayoutElement()=0;

    virtual XP_Bool GetLOElementAndOffset( ElementOffset iEditOffset, XP_Bool bEditStickyAfter,
                LO_Element*& pRetElement,
                int& pLayoutOffset )=0;       // if appropriate
    virtual ElementOffset GetLen(){ return m_iSize; }
    virtual void DeleteChar( MWContext * /*pContext*/, int /*iOffset*/ ){ m_iSize = 0; }
    virtual CEditElement* Divide(int iOffset);
    virtual CEditTextElement* CopyEmptyText(CEditElement *pParent = 0);

    // HREF management...
    virtual void SetHREF( ED_LinkId ){}
    virtual ED_LinkId GetHREF(){ return ED_LINK_ID_NONE; }

    //
    // Leaf manipulation functions..
    //
    XP_Bool NextPosition(ElementOffset iOffset, CEditLeafElement*& pNew, ElementOffset& iNewOffset );
    XP_Bool PrevPosition(ElementOffset iOffset, CEditLeafElement*& pNew, ElementOffset& iNewOffset );

    virtual CPersistentEditInsertPoint GetPersistentInsertPoint(ElementOffset offset);

    // True if this is a bucket for totally unknown html
    virtual XP_Bool IsUnknownHTML();
    // True if this element is an HTML comment
    virtual XP_Bool IsComment();
    // True if this element is an HTML comment that matches a given prefix.
    // IsComment("FOO") will match "<!-- FOOBAR -->"
    virtual XP_Bool IsComment(char* pPrefix);

    XP_Bool IsMisspelled();

#ifdef DEBUG
    virtual void ValidateTree();
#endif
protected:
    void DisconnectLayoutElements(LO_Element* pElement);
    void SetLayoutElementHelper( intn desiredType, LO_Element** pElementHolder,
          intn iEditOffset, intn lo_type,
          LO_Element* pLoElement);
    void ResetLayoutElementHelper( LO_Element** pElementHolder, intn iEditOffset,
            LO_Element* pLoElement );
};

class CEditContainerElement: public CEditElement {
private:
    ED_Alignment m_align;
    ED_Alignment m_defaultAlign;
    XP_Bool m_hackPreformat;
public:
    XP_Bool m_bHasEndTag; // Only important for parsing.

    static CEditContainerElement* NewDefaultContainer( CEditElement *pParent,
            ED_Alignment align );
    CEditContainerElement(CEditElement *pParent, PA_Tag *pTag, int16 csid,
            ED_Alignment eAlign);
    CEditContainerElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer);
    virtual void StreamInChildren(IStreamIn* pIn, CEditBuffer* pBuffer);

    virtual void StreamOut( IStreamOut *pOut);
    virtual XP_Bool ShouldStreamSelf( CEditSelection& /*local*/, CEditSelection& /*selection*/ ) { return TRUE; }
    virtual XP_Bool IsContainer() { return TRUE; }
    static CEditContainerElement* Cast(CEditElement* pElement) {
        return pElement && pElement->IsContainer() ? (CEditContainerElement*) pElement : 0; }
    virtual XP_Bool IsContainerContainer() { return FALSE; }

    virtual ElementIndex GetPersistentCount();
    virtual void FinishedLoad( CEditBuffer *pBuffer );
    virtual void AdjustContainers( CEditBuffer *pBuffer );

    virtual PA_Tag* TagOpen( int iEditOffset );
    virtual PA_Tag* TagEnd( );
    virtual EEditElementType GetElementType();
    virtual void PrintOpen( CPrintState *pPrintState );
    virtual void PrintEnd( CPrintState *pPrintState );

    virtual CEditElement* Clone( CEditElement *pParent = 0);
    virtual void StreamToPositionalText( IStreamOut *pOut, XP_Bool bEnd );

    static EDT_ContainerData* NewData();
    static void FreeData( EDT_ContainerData *pData );

    virtual XP_Bool IsAcceptableChild(CEditElement& pChild) {return pChild.IsLeaf();}

    //
    // Implementation
    //
    void SetData( EDT_ContainerData *pData );
    EDT_ContainerData* GetData( );
    EDT_ContainerData* ParseParams( PA_Tag *pTag, int16 csid );

    void SetAlignment( ED_Alignment eAlign );
    ED_Alignment GetAlignment( ){ return m_align; }
    void CopyAttributes( CEditContainerElement *pContainer );
    XP_Bool IsEmpty();

    XP_Bool ShouldSkipTags();
    XP_Bool IsImplicitBreak();
    // 0..2, where 0 = not in pseudo paragraph.
    // 1 = first container of pseudo paragraph.
    // 2 = second container of pseudo paragraph.
    int16 GetPseudoParagraphState(); 
    XP_Bool ForcesDoubleSpacedNextLine();

    //cmanske: Get single string with all of container's text (no embeded CR/LF)
    // Caller must XP_FREE the result
    char *GetText();

#ifdef DEBUG
    virtual void ValidateTree();
#endif

    XP_Bool IsPlainFirstContainer();
    XP_Bool IsFirstContainer();
    XP_Bool SupportsAlign();
    void AlignIfEmpty( ED_Alignment eAlign );
    XP_Bool HasExtraData();
};

class CEditListElement: public CEditElement {
private:
    char *m_pBaseURL; /* If an ED_LIST_TYPE_CITE, this is the URL from the enclosing <BASE> tag. */

public:
    CEditListElement( CEditElement *pParent, PA_Tag *pTag, int16 csid );
    CEditListElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer);
    virtual ~CEditListElement();

    XP_Bool IsList(){ return TRUE; }
    static CEditListElement* Cast(CEditElement* pElement) {
        return pElement && pElement->IsList() ? (CEditListElement*) pElement : 0; }

    virtual PA_Tag* TagOpen( int iEditOffset );
    virtual CEditElement* Clone( CEditElement *pParent = 0);

    static EDT_ListData* NewData();
    static void FreeData( EDT_ListData *pData );

    // Streaming
    virtual EEditElementType GetElementType();

    //
    // Implementation
    //
    void SetData( EDT_ListData *pData );
    EDT_ListData* GetData( );
    static EDT_ListData* ParseParams( PA_Tag *pTag, int16 csid );

    void CopyData(CEditListElement* pOther);

    XP_Bool IsMailQuote();
#ifdef DEBUG
    virtual void ValidateTree();
#endif

};

//
// CEditTextElement
//

class CEditTextElement: public CEditLeafElement {
public:
    char* m_pText;                          // pointer to actual string.
    int m_textSize;                         // number of bytes allocated
    LO_Element* m_pFirstLayoutElement;
    LO_TextBlock* m_pTextBlock;
    ED_TextFormat m_tf;
    intn m_iFontSize;
    ED_Color m_color;
    ED_LinkId m_href;
    char* m_pFace;
    int16  m_iWeight;                 /* font weight range = 100-900, 400=Normal, 700=Bold*/
    int16  m_iPointSize;              /* not sure what default is! Use 0 to mean "default" */
    char* m_pScriptExtra;   // <SCRIPT> tag parameters

public:
    CEditTextElement(CEditElement *pParent, char *pText);
    CEditTextElement(IStreamIn *pStreamIn, CEditBuffer* pBuffer);
    virtual ~CEditTextElement();

    virtual XP_Bool IsText() { return TRUE; }
    static CEditTextElement* Cast(CEditElement* pElement) {
        return pElement && pElement->IsText() ? (CEditTextElement*) pElement : 0; }

    // accessor functions
    char* GetText(){ return m_pText; }
    char* GetTextWithConvertedSpaces();
    // If bConvertSpaces is FALSE, then csid is not used.
    void SetText(char* pText, XP_Bool bConvertSpaces=FALSE, int16 csid=0 );
    int GetSize(){ return m_textSize; }
    ElementOffset GetLen(){ return m_pText ? XP_STRLEN( m_pText ) : 0 ; }
    LO_Element* GetLayoutElement();
    virtual void SetLayoutElement( intn iEditOffset, intn lo_type,
                LO_Element* pLoElement );
    virtual void ResetLayoutElement( intn iEditOffset,
                LO_Element* pLoElement );

    void SetColor( ED_Color iColor );
    ED_Color GetColor(){ return m_color; }
    void SetFontSize( int iSize );
    int GetFontSize();
    void SetFontFace(char* face);
    char* GetFontFace();
    void SetFontWeight(int16 weight);
    int16 GetFontWeight();
    void SetFontPointSize(int16 pointSize);
    int16 GetFontPointSize();
    void SetScriptExtra(char* );
    char* GetScriptExtra();
    void SetHREF( ED_LinkId );
    ED_LinkId GetHREF(){ return m_href; }
    void ClearFormatting();

    void SetData( EDT_CharacterData *pData );
    EDT_CharacterData *GetData();
    void MaskData( EDT_CharacterData*& ppData );

    //
    // utility functions
    //
    XP_Bool InsertChar( int iOffset, int newChar );
    // returns number of bytes inserted.
    int32 InsertChars( int iOffset, char* pNewChars );
    XP_Bool GetLOTextAndOffset( ElementOffset iEditOffset, XP_Bool bEditStickyAfter, LO_TextStruct*& pRetText,
        int& pLayoutOffset );

    // for leaf implementation.
    void DeleteChar( MWContext *pContext, int iOffset );
    XP_Bool GetLOElementAndOffset( ElementOffset iEditOffset, XP_Bool bEditStickyAfter,
                LO_Element*& pRetText,
                int& pLayoutOffset );

    LO_TextStruct* GetLOText( int iEditOffset );
	LO_TextBlock* GetTextBlock();
	
	virtual XP_Bool CanReflow();

    virtual PA_Tag* TagOpen( int iEditOffset );

    // output funcitons
    virtual void PrintOpen( CPrintState *ps );
    virtual void PrintWithEscapes( CPrintState *ps, XP_Bool bTrimTrailingSpaces );
    virtual void PrintLiteral( CPrintState *ps );
    virtual void StreamOut( IStreamOut *pOut );
    // Why is "selection" param commented out?
    virtual void PartialStreamOut( IStreamOut *pOut, CEditSelection& /*selection*/ );
    virtual EEditElementType GetElementType();
    virtual XP_Bool Reduce( CEditBuffer *pBuffer );
    virtual void StreamToPositionalText( IStreamOut *pOut, XP_Bool bEnd );

    CEditElement* SplitText( int iOffset );
    void DeleteText();
    XP_Bool IsOffsetEnd( int iOffset ){
        ElementOffset iLen = GetLen();
        return ( iOffset == iLen
                  || ( iOffset == iLen-1
                        && m_pText[iLen-1] == ' ')
                );
    }
    CEditTextElement* CopyEmptyText( CEditElement *pParent = 0);
    void FormatOpenTags(PA_Tag*& pStartList, PA_Tag*& pEndList);
    void FormatTransitionTags(CEditTextElement *pNext,
            PA_Tag*& pStart, PA_Tag*& pEndList);
    void FormatCloseTags(PA_Tag*& pStartList, PA_Tag*& pEndList);
    char* DebugFormat();

    void PrintTagOpen( CPrintState *pPrintState, TagType t, ED_TextFormat tf, char* pExtra=0 );
    void PrintFormatDifference( CPrintState *ps, ED_TextFormat bitsDifferent );
    void PrintFormat( CPrintState *ps, CEditTextElement *pFirst, ED_TextFormat mask );
    void PrintTagClose( CPrintState *ps, TagType t );
    void PrintPopFormat( CPrintState *ps, int iStackTop );
    ED_TextFormat PrintFormatClose( CPrintState *ps );

    XP_Bool SameAttributes(CEditTextElement *pCompare);
    void ComputeDifference(CEditTextElement *pFirst,
        ED_TextFormat mask, ED_TextFormat& bitsCommon, ED_TextFormat& bitsDifferent);

#ifdef DEBUG
    virtual void ValidateTree();
#endif

private:
    void PrintRange(CPrintState *ps, int32 start, int32 end);
    void PrintOpen2(CPrintState* ps, XP_Bool bTrimTrailingSpaces);
};

//-----------------------------------------------------------------------------
//  CEditImageElement
//-----------------------------------------------------------------------------

class CEditImageElement: public CEditLeafElement {
    LO_ImageStruct *m_pLoImage;
    ED_Alignment m_align;
    char *m_pParams;            // partial parameter string

    //EDT_ImageData *pData;
    int32 m_iHeight;
    int32 m_iWidth;
    ED_LinkId m_href;
    XP_Bool m_bSizeWasGiven;
    XP_Bool m_bSizeIsBogus;
    XP_Bool m_bWidthPercent;
    XP_Bool m_bHeightPercent;
public:
    intn m_iSaveIndex;
    intn m_iSaveLowIndex;

public:
    // pass through constructors.
    CEditImageElement(CEditElement *pParent, PA_Tag* pTag = 0, int16 csid = 0,
            ED_LinkId href = ED_LINK_ID_NONE );
    CEditImageElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer);
    virtual ~CEditImageElement();

    LO_Element* GetLayoutElement();
    virtual void SetLayoutElement( intn iEditOffset, intn lo_type,
                LO_Element* pLoElement );
    virtual void ResetLayoutElement( intn iEditOffset,
                LO_Element* pLoElement );

    void StreamOut( IStreamOut *pOut );
    virtual EEditElementType GetElementType();


    //
    // CEditElement implementation
    //
    PA_Tag* TagOpen( int iEditOffset );
    void PrintOpen( CPrintState *pPrintState );

    //
    // CEditLeafElement Implementation
    //
    XP_Bool IsImage() { return TRUE; }
    static CEditImageElement* Cast(CEditElement* pElement) {
        return pElement && pElement->IsImage() ? (CEditImageElement*) pElement : 0; }
    void FinishedLoad( CEditBuffer *pBuffer );
    LO_ImageStruct* GetLayoutImage(){ return m_pLoImage; }

    XP_Bool GetLOElementAndOffset( ElementOffset iEditOffset, XP_Bool /*bEditStickyAfter*/ ,
                LO_Element*& pRetElement,
                int& pLayoutOffset );

    int32 GetDefaultBorder();
    void SetHREF( ED_LinkId );
    ED_LinkId GetHREF(){ return m_href; }

    //
    // Implementation
    //
    void SetImageData( EDT_ImageData *pData );
    EDT_ImageData* GetImageData( );
    EDT_ImageData* ParseParams( PA_Tag *pTag, int16 csid );
    char* FormatParams(EDT_ImageData* pData, XP_Bool bForPrinting);

    XP_Bool SizeIsKnown();

    //CLM: Since HREF is a "character" attribute,
    //  we look at images for this as well
    void MaskData( EDT_CharacterData*& pData );
    EDT_CharacterData* GetCharacterData();
};


//-----------------------------------------------------------------------------
//  CEditHRuleElement
//-----------------------------------------------------------------------------

class CEditHorizRuleElement: public CEditLeafElement {
    LO_HorizRuleStruct *m_pLoHorizRule;
public:
    // pass through constructors.
    CEditHorizRuleElement(CEditElement *pParent, PA_Tag* pTag = 0, int16 csid = 0 );
    CEditHorizRuleElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer);
    virtual ~CEditHorizRuleElement();

    void StreamOut( IStreamOut *pOut);
    virtual EEditElementType GetElementType();

    //
    // CEditLeafElement Implementation
    //
    virtual XP_Bool CausesBreakBefore() { return TRUE;}
    virtual XP_Bool CausesBreakAfter() { return TRUE;}
    virtual XP_Bool AllowBothSidesOfGap() { return TRUE; }

    virtual void SetLayoutElement( intn iEditOffset, intn lo_type,
                LO_Element* pLoElement );
    virtual void ResetLayoutElement( intn iEditOffset,
                LO_Element* pLoElement );

    LO_Element* GetLayoutElement();
    LO_HorizRuleStruct* GetLayoutHorizRule(){ return m_pLoHorizRule; }

    XP_Bool GetLOElementAndOffset( ElementOffset iEditOffset, XP_Bool /* bEditStickyAfter */,
                LO_Element*& pRetElement,
                int& pLayoutOffset );

    static EDT_HorizRuleData* NewData();
    static void FreeData( EDT_HorizRuleData *pData );

    //
    // Implementation
    //
    void SetData( EDT_HorizRuleData *pData );
    EDT_HorizRuleData* GetData( );
    static EDT_HorizRuleData* ParseParams( PA_Tag *pTag, int16 csid );
};

//-----------------------------------------------------------------------------
//  CEditIconElement
//-----------------------------------------------------------------------------

#define EDT_ICON_NAMED_ANCHOR           0
#define EDT_ICON_FORM_ELEMENT           1
#define EDT_ICON_UNSUPPORTED_TAG        2
#define EDT_ICON_UNSUPPORTED_END_TAG    3
#define EDT_ICON_JAVA                   4
#define EDT_ICON_PLUGIN                 5
#define EDT_ICON_OBJECT                 6
#define EDT_ICON_LAYER                  7

class CEditIconElement: public CEditLeafElement {
private:
    // States for parsing LOCALDATA tag, see ParseLocalData().
    enum {BeforeMIME,InMIME,BeforeURL,InURL,AfterURL};

    // States for ReplaceParamValue().
    enum {OutsideValue,BeforeValue,InsideValue,InsideValueQuote};

protected:
    LO_ImageStruct *m_pLoIcon;
    TagType m_originalTagType;
    int32 m_iconTag;
    XP_Bool m_bEndTag;
    char *m_pSpoofData;

public:
    // pass through constructors.
    CEditIconElement(CEditElement *pParent, int32 iconTag, PA_Tag* pTag = 0, int16 csid = 0 );
    CEditIconElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer);
    ~CEditIconElement();

    void StreamOut( IStreamOut *pOut);
    virtual EEditElementType GetElementType(){ return eIconElement; }

    static ED_TagValidateResult ValidateTag(  char *Data, XP_Bool bNoBrackets );

    //
    // CEditLeafElement Implementation
    //
    XP_Bool IsIcon() { return TRUE; }
    static CEditIconElement* Cast(CEditElement* pElement) {
        return pElement && pElement->IsIcon() ? (CEditIconElement*) pElement : 0; }

    virtual void SetLayoutElement( intn iEditOffset, intn lo_type,
                LO_Element* pLoElement );
    virtual void ResetLayoutElement( intn iEditOffset,
                LO_Element* pLoElement );

    LO_Element* GetLayoutElement(){ return (LO_Element*)m_pLoIcon; }
    LO_ImageStruct* GetLayoutIcon(){ return m_pLoIcon; }

    XP_Bool GetLOElementAndOffset( ElementOffset iEditOffset, XP_Bool /* bEditStickyAfter */,
                LO_Element*& pRetElement,
                int& pLayoutOffset );

    PA_Tag* TagOpen( int iEditOffset );
    void PrintOpen( CPrintState *pPrintState );
    void PrintEnd( CPrintState *pPrintState );

    char* GetData();

    // Creates list of the mimeType/URL pairs from the LOCALDATA parameter.
    // List of Strings.
    // mimeTypes and URLs must be freed with FreeLocalDataLists() when done.
    // Returns the length of the lists.
    int ParseLocalData(char ***mimeTypes,char ***urls);
    static void FreeLocalDataLists(char **mimeTypes,char **urls,int count);

    // Replace all occurances of pOld in parameter values with pNew.
    // Used by LOCALDATA to update tags when saved/published/mailed.
    void ReplaceParamValues(char *pOld,char *pNew);

    // Call this to set both Width and Height at same time,
    //  to avoid relayout twice if Width and Height are set separetely
    // Set either value to 0 to ignore 
    //  Do we ever need to set to 0? (change to -1 to "ignore" if we need 0)
    void SetSize(XP_Bool bWidthPercent, int32 iWidth,
                 XP_Bool bHeightPercent, int32 iHeight);

    // Not currently implemented.
    void SetData( char* );

    void MorphTag( PA_Tag *pTag );
    void SetSpoofData( PA_Tag* pTag );
    void SetSpoofData( char* pData );

    virtual XP_Bool IsUnknownHTML();
    virtual XP_Bool IsComment();
    virtual XP_Bool IsComment(char* pPrefix);

public:
    // Used by CEditSaveObject.
    intn *m_piSaveIndices;
};

//-----------------------------------------------------------------------------
//  CEditTargetElement
//-----------------------------------------------------------------------------
class CEditTargetElement: public CEditIconElement {
private:
public:
    CEditTargetElement(CEditElement *pParent, PA_Tag* pTag = 0, int16 csid = 0 );
    CEditTargetElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer);
    void StreamOut( IStreamOut *pOut);
    EEditElementType GetElementType(){ return eTargetElement; }

    PA_Tag* TagOpen( int iEditOffset );

    char *GetName();
    void SetName( char* pName, int16 csid );

    void SetData( EDT_TargetData *pData );
    EDT_TargetData* GetData();
    EDT_TargetData* GetData(int16 csid);
    static EDT_TargetData* ParseParams( PA_Tag *pTag, int16 csid );
    static EDT_TargetData* NewTargetData();
    static void FreeTargetData(EDT_TargetData* pData);
};


//-----------------------------------------------------------------------------
//  CEditBreakElement
//-----------------------------------------------------------------------------

class CEditBreakElement: public CEditLeafElement {
    LO_LinefeedStruct *m_pLoLinefeed;
public:
    // pass through constructors.
    CEditBreakElement(CEditElement *pParent, PA_Tag* pTag = 0, int16 csid = 0 );
    CEditBreakElement(IStreamIn *pStreamIn, CEditBuffer *pBuffer);
    virtual ~CEditBreakElement();

    void StreamOut( IStreamOut *pOut );
    virtual EEditElementType GetElementType();

    void PrintOpen( CPrintState *ps );

    virtual XP_Bool IsBreak() { return TRUE; }
    virtual XP_Bool CausesBreakAfter() { return TRUE;}
    static CEditBreakElement* Cast(CEditElement* pElement) {
        return pElement && pElement->IsBreak() ? (CEditBreakElement*) pElement : 0; }
    //
    // CEditLeafElement Implementation
    //
    void SetLayoutElement( intn iEditOffset, intn lo_type,
                LO_Element* pLoElement );
    virtual void ResetLayoutElement( intn iEditOffset,
                LO_Element* pLoElement );

    LO_Element* GetLayoutElement();

    XP_Bool GetLOElementAndOffset( ElementOffset iEditOffset, XP_Bool bEditStickyAfter,
                LO_Element*& pRetElement,
                int& pLayoutOffset );
};

class CEditEndElement: public CEditHorizRuleElement {
public:
    CEditEndElement(CEditElement *pParent):
            CEditHorizRuleElement(pParent){
            EDT_HorizRuleData ourParams;
            ourParams.align = ED_ALIGN_LEFT;
            ourParams.size = DEFAULT_HR_THICKNESS;
            ourParams.bNoShade = FALSE;
            ourParams.iWidth = 64;
            ourParams.bWidthPercent = FALSE;
            ourParams.pExtra = 0;
            SetData(&ourParams);
            }

    virtual XP_Bool ShouldStreamSelf( CEditSelection& /* local */, CEditSelection& /* selection */ ) { return FALSE; }
    virtual void StreamOut( IStreamOut * /* pOut */) { XP_ASSERT(FALSE); }

    void SetLayoutElement( intn /* iEditOffset */ , intn lo_type,
                LO_Element* pLoElement ){
        CEditHorizRuleElement::SetLayoutElement(-1, lo_type, pLoElement);
    }

    virtual EEditElementType GetElementType() { return eEndElement; }
    virtual void PrintOpen( CPrintState * /*pPrintState*/ ) {}
    virtual void PrintEnd( CPrintState * /*pPrintState*/ ) {}
};

class CEditEndContainerElement: public CEditContainerElement {
public:
    CEditEndContainerElement(CEditElement *pParent);
    virtual void StreamOut( IStreamOut * pOut);
    virtual XP_Bool ShouldStreamSelf( CEditSelection& local, CEditSelection& selection );
    virtual XP_Bool IsAcceptableChild(CEditElement& pChild);
    virtual void PrintOpen( CPrintState * pPrintState );
    virtual void PrintEnd( CPrintState * pPrintState );
    virtual XP_Bool IsEndContainer();
    virtual void AdjustContainers( CEditBuffer* pBuffer );
};

class CEditInternalAnchorElement: public CEditLeafElement {
public:
    CEditInternalAnchorElement(CEditElement *pParent);
    virtual ~CEditInternalAnchorElement();
    virtual XP_Bool Reduce( CEditBuffer* );

    virtual XP_Bool ShouldStreamSelf( CEditSelection& local, CEditSelection& selection);
    virtual void StreamOut( IStreamOut *pOut);

    virtual void SetLayoutElement( intn iEditOffset, intn lo_type,
                LO_Element* pLoElement );
    virtual void ResetLayoutElement( intn iEditOffset,
                LO_Element* pLoElement );
    virtual LO_Element* GetLayoutElement();

    virtual XP_Bool GetLOElementAndOffset( ElementOffset iEditOffset, XP_Bool bEditStickyAfter,
                LO_Element*& pRetElement,
                int& pLayoutOffset );

    virtual EEditElementType GetElementType();
    virtual void PrintOpen( CPrintState *pPrintState );
    virtual void PrintEnd( CPrintState *pPrintState );
};


//
// Macro to compute the next size of an TextBuffer.
//
#define GROW_TEXT(x)    ((x+0x20) & ~0x1f)   // grow buffer by 32 bytes

//-----------------------------------------------------------------------------
// CEditPosition
//-----------------------------------------------------------------------------

class CEditPosition {
private:
    CEditElement* m_pElement;
    int m_iOffset;
public:
    CEditPosition( CEditElement* pElement, int iOffset = 0 ):
            m_pElement(pElement), m_iOffset( iOffset ){}

    int Offset() { return m_iOffset; }
    CEditElement* Element() { return m_pElement; }
    IsPositioned(){ return m_pElement != 0; }
};


class CEditPositionComparable: public CEditPosition {
private:
    TXP_GrowableArray_int32 m_Array;
    void CalcPosition( TXP_GrowableArray_int32* pA, CEditPosition *pPos );
public:
    CEditPositionComparable( CEditElement* pElement, int iOffset = 0 ):
            CEditPosition( pElement, iOffset )
    {
        CalcPosition( &m_Array, this );
    }

    // return -1 if passed pos is before
    // return 0 if pos is the same
    // return 1 if pos is greater
    int Compare( CEditPosition *pPos );
};

//-----------------------------------------------------------------------------
// CEditBuffer (and others)..
//-----------------------------------------------------------------------------

class CParseState {
public:
    XP_Bool bLastWasSpace;
    XP_Bool m_bInTitle;
private:
    int m_iDocPart;
public:
    XP_Bool InBody();
    void StartBody();
    void EndBody();

    TagType m_inJavaScript; // 0 when not in any script, otherwise the tag of the kind of script
    int m_baseFontSize;
    TXP_PtrStack_ED_Alignment m_formatAlignStack;
    TXP_PtrStack_TagType m_formatTypeStack;
    TXP_PtrStack_CEditTextElement m_formatTextStack;
    CEditTextElement *m_pNextText;
    CStreamOutMemory *m_pJavaScript;
    CStreamOutMemory* m_pPostBody;
    CStreamOutMemory* GetStream(); // Gets either m_pJavaScript or m_postBody, depending upon m_iDocPart

public:
    void Reset();
    CParseState();
    ~CParseState();
private:
    void Free(CStreamOutMemory*& pStream);
};

class CPrintState {
public:
    int m_iLevel;
    int m_iCharPos;
    XP_Bool m_bTextLast;
    IStreamOut* m_pOut;
    TXP_PtrStack_ED_TextFormat m_formatStack;
    TXP_PtrStack_CEditTextElement m_elementStack;
    CEditBuffer *m_pBuffer;
    XP_Bool m_bEncodeSelectionAsComment;
    CEditSelection m_selection;
public:
    void Reset( IStreamOut *pStream, CEditBuffer *pBuffer );
    XP_Bool ShouldPrintSelectionComments(CEditLeafElement* pElement);
    XP_Bool ShouldPrintSelectionComment(CEditLeafElement* pElement, XP_Bool bEnd);
    void PrintSelectionComment(XP_Bool bEnd, XP_Bool bStickyAfter);
};


//
// This is a structure, not a class.  It cannot be instanciated with NEW and
//  should never be derived from.
//
class CEditLinkManager;

struct ED_Link {
private:
    // Should never create on of these with a constructor!  Must
    //  create it only through CEditLinkManager::Add
    ED_Link(){}
public:
    void AddRef(){ iRefCount++; }

    void Release() {} 
    // A bug, probably should call pLinkManager::Free(), not worth the work to fix 
    // it and make sure ref counting works all the time.  hardts

    EDT_HREFData* GetData();
    intn iRefCount;
    CEditLinkManager* pLinkManager;
    int linkOffset;
    char *hrefStr;
    char *pExtra;
    XP_Bool bAdjusted;
};


typedef TXP_GrowableArray_pChar ED_HREFList;
class CEditLinkManager {
private:
    TXP_GrowableArray_ED_Link m_links;

public:
    CEditLinkManager();
    ED_Link* MakeLink( char* pStr, char* pExtra, intn iRefCount = 1 );
    ED_LinkId Add( char *pHREF, char *pExtra );
    char* GetHREF( ED_LinkId id ){ return id->hrefStr; }
    int GetHREFLen( ED_LinkId id ){ return XP_STRLEN( id->hrefStr ); }
    EDT_HREFData* GetHREFData( ED_LinkId id){ return id->GetData(); }
    void Free( ED_LinkId id );
    void AdjustAllLinks( char *pOldURL, char* pNewURL, ED_HREFList *badLinks );

    // Wrapper for other AdjustLink function.
    void AdjustLink( ED_LinkId id, char *pOldURL, char* pNewURL, ED_HREFList *badLinks ) {
      if (id && id->hrefStr) AdjustLink(&id->hrefStr,pOldURL,pNewURL,badLinks);
    }

    // Used for adjusting links, images, etc.
    static void AdjustLink(char **pURL,char *pOldBase, char *pNewBase, ED_HREFList *badLinks);

    static void AddHREFUnique(ED_HREFList *badLinks,char *pURL);

    // Since we don't trust the reference counting, CEditSaveObject::FixupLinks() marks each link
    // as it adjusts it, so it doesn't adjust links twice.  HARDTS
    static void SetAdjusted(ED_LinkId id,XP_Bool bVal) { if (id) id->bAdjusted = bVal;}
    static XP_Bool GetAdjusted(ED_LinkId id) { if (id) return id->bAdjusted; else return TRUE;}
};

// Relayout Flags
#define RELAYOUT_NOCARET        1       // after relaying out, don't show the caret


// Commands

class CEditBuffer;

class CEditCommand {
public:
    CEditCommand(CEditBuffer*, intn id);
    virtual ~CEditCommand();
    virtual void Do();
    virtual void Undo();
    virtual void Redo();
    intn GetID();
#ifdef DEBUG
    virtual void Print(IStreamOut& stream);
#endif
protected:
    CEditBuffer* GetEditBuffer() { return m_editBuffer; };
private:
    CEditBuffer* m_editBuffer;
    intn m_id;
};

// CEditCommandGroup

class CEditCommandGroup : public CEditCommand {
public:
    CEditCommandGroup(CEditBuffer*, int id);
    virtual ~CEditCommandGroup();
    void AdoptAndDo(CEditCommand* pCommand);
    virtual void Undo();
    virtual void Redo();
#ifdef DEBUG
    virtual void Print(IStreamOut& stream);
#endif
    intn GetNumberOfCommands();
private:
    TXP_GrowableArray_CEditCommand m_commands;
};

/* Default */
#define EDT_CMD_LOG_MAXHISTORY 1

class CEditCommandLog;

class CGlobalHistoryGroup {
public:
    static CGlobalHistoryGroup* GetGlobalHistoryGroup();
    CGlobalHistoryGroup();
    ~CGlobalHistoryGroup();
    XP_Bool IsReload(CEditBuffer* pContext);
    void ReloadFinished(CEditBuffer* pContext);
    CEditCommandLog* CreateLog(CEditBuffer* pContext);
    CEditCommandLog* GetLog(CEditBuffer* pContext);
    void DeleteLog(CEditBuffer* pContext);
    void IgnoreNextDeleteOf(CEditBuffer* pContext);

private:
    CEditCommandLog* m_pHead; // First log
    static CGlobalHistoryGroup* g_pGlobalHistoryGroup;
};

//
// CEditDocState
//
class CEditDocState {
public:
  // Only created by CEditBuffer::RecordState().
  friend class CEditBuffer;
  virtual ~CEditDocState();

#ifdef DEBUG
    void Print(IStreamOut& stream);
#endif

private:
  CEditDocState();

  XP_HUGE_CHAR_PTR m_pBuffer;
  DocumentVersion m_version;
};


class CCommandState {
public:
    CCommandState();
    ~CCommandState();
    void SetID(intn commandID);
    intn GetID();
    void Record(CEditBuffer* pBufferToRecord);
    void Restore(CEditBuffer* pBufferToRestore);
#ifdef DEBUG
    void Print(IStreamOut& stream);
#endif

private:
    void Flush();

    intn m_commandID;
    CEditDocState *m_pState;
};

class CEditCommandLog {
public:
    void StartTyping(intn typing);
    void EndTyping();
    void AdoptAndDo(CEditCommand*);
    void Undo();
    void Redo();
    void Trim();    // Trims undo and redo command lists.
    XP_Bool InReload();
    void SetInReload(XP_Bool bInReload);
    DocumentVersion GetVersion();
    void SetVersion(DocumentVersion); // For CEditBuffer::RestoreState().
    DocumentVersion GetStoredVersion();
    void DocumentStored();
    XP_Bool IsDirty();

    intn GetCommandHistoryLimit();
    void SetCommandHistoryLimit(intn newLimit);

    intn GetNumberOfCommandsToUndo();
    intn GetNumberOfCommandsToRedo();

    // Returns kNullCommandID if out of range
    intn GetUndoCommand(intn index);
    intn GetRedoCommand(intn index);

    void BeginBatchChanges(intn batchID);
    void EndBatchChanges();

#ifdef DEBUG
    void Print(IStreamOut& stream);
#endif

#ifdef EDITOR_JAVA
    EditorPluginManager GetPlugins();
#endif

    // Temporary directory specific to the edit buffer.  In xpURL format.
    char *GetDocTempDir();
    char *CreateDocTempFilename(char *pPrefix,char *pExtension);

protected:
    // Use the global history to get the command log for a given context.
    CEditCommandLog();
    ~CEditCommandLog();

private:
    void FinishBatchCommands();
    void InternalAdoptAndDo(CEditCommand*);
    void InternalDo(intn id);
    void Trim(intn start, intn end);
    static char *GetAppTempDir();

    CEditBuffer* m_pBuffer;
    CCommandState* m_pUndo;
    CCommandState* m_pRedo;
    int32 m_iBatchLevel;
    int16 m_state;

#ifdef DEBUG
    friend class CEditCommandLogRecursionCheckEntry;
    XP_Bool m_bBusy;
#endif

    friend CGlobalHistoryGroup;
    CEditCommandLog* m_pNext;
    MWContext* m_pContext;
    XP_Bool m_bIgnoreDelete;

    DocumentVersion m_version;
    DocumentVersion m_storedVersion;
    DocumentVersion m_highestVersion;

#ifdef EDITOR_JAVA
    EditorPluginManager m_pPlugins;
#endif


    ///// All the temp directory variables are stored in CEditCommandLog
    ///// because they must persist when the CEditBuffer is deleted 
    ///// during undo/redo.    
    // The temp directory for this document.
    char *m_pDocTempDir; 
    // Used to create unique filenames in the temp directory.
    int m_iDocTempFilenameNonce;
    // used to create unique temp dir filenames.
    static int32 m_iDocTempDirNonce; 
};

// The commands

#define kNullCommandID 0
#define kTypingCommandID 1
#define kAddTextCommandID 2
#define kDeleteTextCommandID 3
#define kCutCommandID 4
#define kPasteTextCommandID 5
#define kPasteHTMLCommandID 6
#define kPasteHREFCommandID 7
#define kChangeAttributesCommandID 8
#define kIndentCommandID 9
#define kParagraphAlignCommandID 10
#define kMorphContainerCommandID 11
#define kInsertHorizRuleCommandID 12
#define kSetHorizRuleDataCommandID 13
#define kInsertImageCommandID 14
#define kSetImageDataCommandID 15
#define kInsertBreakCommandID 16
#define kChangePageDataCommandID 17
#define kSetMetaDataCommandID 18
#define kDeleteMetaDataCommandID 19
#define kInsertTargetCommandID 20
#define kSetTargetDataCommandID 21
#define kInsertUnknownTagCommandID 22
#define kSetUnknownTagDataCommandID 23
#define kGroupOfChangesCommandID 24
#define kSetListDataCommandID 25

#define kInsertTableCommandID 26
#define kDeleteTableCommandID 27
#define kSetTableDataCommandID 28

#define kInsertTableCaptionCommandID 29
#define kSetTableCaptionDataCommandID 30
#define kDeleteTableCaptionCommandID 31

#define kInsertTableRowCommandID 32
#define kSetTableRowDataCommandID 33
#define kDeleteTableRowCommandID 34

#define kInsertTableColumnCommandID 35
#define kSetTableCellDataCommandID 36
#define kDeleteTableColumnCommandID 37

#define kInsertTableCellCommandID 38
#define kDeleteTableCellCommandID 39

#define kInsertLayerCommandID 40
#define kDeleteLayerCommandID 41
#define kSetLayerDataCommandID 42

#define kSetSelectionCommandID 43

#define kPerformPluginCommandID 44

#define kCommandIDMax 44

// This class holds a chunk of html text.

class CEditText
{
public:
    CEditText();
    ~CEditText();

    void Clear();

    // The length is the number of chars
    char* GetChars();
    int32 Length();
    char** GetPChars();
    int32* GetPLength();
private:
    char* m_pChars;
    int32 m_iLength;
    ElementIndex m_iCount;
};

// Can save and restore a region of the document. Also saves and restores current
// selection

class CEditDataSaver {
public:
    CEditDataSaver(CEditBuffer* pBuffer);
    ~CEditDataSaver();
    void DoBegin(CPersistentEditSelection& original);
    void DoEnd(CPersistentEditSelection& modified);
    void Undo();
    void Redo();

    CPersistentEditSelection* GetOriginalDocumentSelection() { return &m_originalDocument; }
private:
    CEditBuffer* m_pEditBuffer;
    CPersistentEditSelection m_originalDocument;
    CPersistentEditSelection m_modifiedDocument;
    CPersistentEditSelection m_original;
    CPersistentEditSelection m_expandedOriginal;
    CPersistentEditSelection m_expandedModified;
    CEditText m_originalText;
    CEditText m_modifiedText;
    XP_Bool m_bModifiedTextHasBeenSaved;
#ifdef DEBUG
    int m_bDoState;
#endif
};

class CDeleteTableCommand
    : public CEditCommand
{
public:
    CDeleteTableCommand(CEditBuffer* buffer, intn id = kDeleteTableCommandID);
    virtual ~CDeleteTableCommand();
    virtual void Do();
//    virtual void Undo();
//    virtual void Redo();
private:
	CEditTableElement* m_pTable;
    CPersistentEditInsertPoint m_replacePoint;
    CPersistentEditSelection m_originalSelection;
};

class CInsertTableCaptionCommand
    : public CEditCommand
{
public:
    CInsertTableCaptionCommand(CEditBuffer* buffer, EDT_TableCaptionData *pData, intn id = kInsertTableCaptionCommandID);
    virtual ~CInsertTableCaptionCommand();
    virtual void Do();
//    virtual void Undo();
//    virtual void Redo();
private:
    CEditCaptionElement* m_pOldCaption;
    CPersistentEditSelection m_originalSelection;
    CPersistentEditSelection m_changedSelection;
};

class CDeleteTableCaptionCommand
    : public CEditCommand
{
public:
    CDeleteTableCaptionCommand(CEditBuffer* buffer, intn id = kDeleteTableCaptionCommandID);
    virtual ~CDeleteTableCaptionCommand();
    virtual void Do();
//    virtual void Undo();
//    virtual void Redo();
private:
    CEditCaptionElement* m_pOldCaption;
    CPersistentEditSelection m_originalSelection;
    CPersistentEditSelection m_changedSelection;
};
class CInsertTableRowCommand
    : public CEditCommand
{
public:
    CInsertTableRowCommand(CEditBuffer* buffer, EDT_TableRowData *pData, XP_Bool bAfterCurrentRow, intn number, intn id = kInsertTableRowCommandID);
    virtual ~CInsertTableRowCommand();
    virtual void Do();
//    virtual void Undo();
//    virtual void Redo();
private:
    intn m_row;
    intn m_number;
    intn m_new_row;
    CPersistentEditSelection m_originalSelection;
    CPersistentEditSelection m_changedSelection;
};

class CDeleteTableRowCommand
    : public CEditCommand
{
public:
    CDeleteTableRowCommand(CEditBuffer* buffer, intn rows, intn id = kDeleteTableRowCommandID);
    virtual ~CDeleteTableRowCommand();
    virtual void Do();
//    virtual void Undo();
//    virtual void Redo();
private:
    XP_Bool m_bDeletedWholeTable;
    intn m_row;
    intn m_rows;
	CEditTableElement m_table;    // Holds deleted rows
    CPersistentEditSelection m_originalSelection;
    CPersistentEditSelection m_changedSelection;
};

class CInsertTableColumnCommand
    : public CEditCommand
{
public:
    CInsertTableColumnCommand(CEditBuffer* buffer, EDT_TableCellData *pData, XP_Bool bAfterCurrentCell, intn number, intn id = kInsertTableColumnCommandID);
    virtual ~CInsertTableColumnCommand();
    virtual void Do();
//    virtual void Undo();
//    virtual void Redo();
private:
    intn m_number;
    //cmanske: Changed to retain actual column and new insert column
    // This is needed for proper insert given COLSPAN effects
    intn m_column;
    intn m_new_column;
    CPersistentEditSelection m_originalSelection;
    CPersistentEditSelection m_changedSelection;
};

class CDeleteTableColumnCommand
    : public CEditCommand
{
public:
    CDeleteTableColumnCommand(CEditBuffer* buffer, intn rows, intn id = kDeleteTableColumnCommandID);
    virtual ~CDeleteTableColumnCommand();
    virtual void Do();
//    virtual void Undo();
//    virtual void Redo();
private:
    XP_Bool m_bDeletedWholeTable;
    intn m_column;
    intn m_columns;
	CEditTableElement m_table;    // Holds deleted columns
    CPersistentEditSelection m_originalSelection;
    CPersistentEditSelection m_changedSelection;
};

class CInsertTableCellCommand
    : public CEditCommand
{
public:
    CInsertTableCellCommand(CEditBuffer* buffer, XP_Bool bAfterCurrentCell, intn number, intn id = kInsertTableCellCommandID);
    virtual ~CInsertTableCellCommand();
    virtual void Do();
//    virtual void Undo();
//    virtual void Redo();
private:
    intn m_column;
    intn m_new_column;
    intn m_number;
    CPersistentEditSelection m_originalSelection;
    CPersistentEditSelection m_changedSelection;
};

class CDeleteTableCellCommand
    : public CEditCommand
{
public:
    CDeleteTableCellCommand(CEditBuffer* buffer, intn rows, intn id = kDeleteTableCellCommandID);
    virtual ~CDeleteTableCellCommand();
    virtual void Do();
//    virtual void Undo();
//    virtual void Redo();
private:
    XP_Bool m_bDeletedWholeTable;
    intn m_column;
    intn m_columns;
	CEditTableRowElement m_tableRow;    // Holds deleted cells
    CPersistentEditSelection m_originalSelection;
    CPersistentEditSelection m_changedSelection;
};

class CSetListDataCommand
    : public CEditCommand
{
public:
    CSetListDataCommand(CEditBuffer*, EDT_ListData& listData, intn id = kSetListDataCommandID);
    virtual ~CSetListDataCommand();
    virtual void Do();
    virtual void Undo();
    virtual void Redo();

private:
    EDT_ListData m_newData;
    EDT_ListData* m_pOldData;
};

// CChangePageDataCommand - for commands that change the document's page data

class CChangePageDataCommand
    : public CEditCommand
{
public:
    CChangePageDataCommand(CEditBuffer*, intn id = kChangePageDataCommandID);
    virtual ~CChangePageDataCommand();
    virtual void Undo();
    virtual void Redo();

private:
    EDT_PageData* m_oldData;
    EDT_PageData* m_newData;
};

// CSetMetaDataCommand - for changes to the document's meta data

class CSetMetaDataCommand
    : public CEditCommand
{
public:
    // Note: Simply creating this command does the action -- this is so we don't have to explicitly copy the
    // meta data that's passed in.
    CSetMetaDataCommand(CEditBuffer*, EDT_MetaData *pMetaData, XP_Bool bDelete, intn id = kSetMetaDataCommandID);
    virtual ~CSetMetaDataCommand();
    virtual void Undo();
    virtual void Redo();

private:
    XP_Bool m_bNewItem;    // This command creates an item.
    XP_Bool m_bDelete;     // This command deletes an item.
    EDT_MetaData* m_pOldData;
    EDT_MetaData* m_pNewData;
};

class CSetTableDataCommand
    : public CEditCommand
{
public:
    // Note: Simply creating this command does the action -- this is so we don't have to explicitly copy the
    // data that's passed in.
    CSetTableDataCommand(CEditBuffer*, EDT_TableData *pMetaData, intn id = kSetTableDataCommandID);
    virtual ~CSetTableDataCommand();
    virtual void Undo();
    virtual void Redo();

private:
    EDT_TableData* m_pOldData;
    EDT_TableData* m_pNewData;
};

class CSetTableCaptionDataCommand
    : public CEditCommand
{
public:
    // Note: Simply creating this command does the action -- this is so we don't have to explicitly copy the
    // data that's passed in.
    CSetTableCaptionDataCommand(CEditBuffer*, EDT_TableCaptionData *pMetaData, intn id = kSetTableCaptionDataCommandID);
    virtual ~CSetTableCaptionDataCommand();
    virtual void Undo();
    virtual void Redo();

private:
    EDT_TableCaptionData* m_pOldData;
    EDT_TableCaptionData* m_pNewData;
};

class CSetTableRowDataCommand
    : public CEditCommand
{
public:
    // Note: Simply creating this command does the action -- this is so we don't have to explicitly copy the
    // data that's passed in.
    CSetTableRowDataCommand(CEditBuffer*, EDT_TableRowData *pMetaData, intn id = kSetTableRowDataCommandID);
    virtual ~CSetTableRowDataCommand();
    virtual void Undo();
    virtual void Redo();

private:
    EDT_TableRowData* m_pOldData;
    EDT_TableRowData* m_pNewData;
};

class CSetTableCellDataCommand
    : public CEditCommand
{
public:
    // Note: Simply creating this command does the action -- this is so we don't have to explicitly copy the
    // data that's passed in.
    CSetTableCellDataCommand(CEditBuffer*, EDT_TableCellData *pMetaData, intn id = kSetTableCellDataCommandID);
    virtual ~CSetTableCellDataCommand();
    virtual void Undo();
    virtual void Redo();

private:
    EDT_TableCellData* m_pOldData;
    EDT_TableCellData* m_pNewData;
};

class CSetSelectionCommand
    : public CEditCommand
{
public:
    CSetSelectionCommand(CEditBuffer*, CEditSelection& selection, intn id = kSetSelectionCommandID);
    virtual ~CSetSelectionCommand();
    virtual void Do();
    virtual void Undo();
    virtual void Redo();

private:
    CPersistentEditSelection m_OldSelection;
    CPersistentEditSelection m_NewSelection;
};

// Generic timer callback

extern "C" {
void CEditTimerCallback (void * closure);
}

class CEditTimer {
public:
    CEditTimer();
    virtual ~CEditTimer();
protected:
    void SetTimeout(uint32 msecs);
    void Callback();
    void Cancel();
    XP_Bool IsTimeoutEnabled() { return m_timeoutEnabled; }
    virtual void OnCallback();
private:
    friend void CEditTimerCallback (void * closure);
    XP_Bool m_timeoutEnabled;
    void*   m_timerID;
};

class CFinishLoadTimer : public CEditTimer {
public:
    CFinishLoadTimer();
    void FinishedLoad(CEditBuffer* pBuffer);
protected:
    virtual void OnCallback();
private:
    CEditBuffer* m_pBuffer;
};

class CRelayoutTimer : public CEditTimer {
public:
    CRelayoutTimer();
    void SetEditBuffer(CEditBuffer* pBuffer);
    void Relayout(CEditElement *pStartElement, int iOffset);
    void Flush();
protected:
    virtual void OnCallback();
private:
    CEditBuffer* m_pBuffer;
    CEditElement* m_pStartElement;
    int m_iOffset;
};

class CAutoSaveTimer : public CEditTimer {
public:
    CAutoSaveTimer();
    void SetEditBuffer(CEditBuffer* pBuffer);
    void SetPeriod(int32 minutes);
    int32 GetPeriod();
    void Restart();

    //CLM: Call before and after operations
    //     that are upset by autosave relayout
    void Suspend();
    void Resume();

protected:
    virtual void OnCallback();
private:
    CEditBuffer* m_pBuffer;
    int32 m_iMinutes;
    XP_Bool m_bSuspended;
    XP_Bool m_bCalledWhileSuspended;
};


//
// CSizingObject
//

class CSizingObject {
public:
    CSizingObject();
    ~CSizingObject();
    XP_Bool Create(CEditBuffer *pBuffer,
                   LO_Element *pLoElement,
                   int iSizingStyle,
                   int32 xVal, int32 yVal,
                   XP_Bool bLockAspect, XP_Rect *pRect);


    XP_Bool GetSizingRect(int32 xVal, int32 yVal, XP_Bool bLockAspect, XP_Rect *pRect);
    void ResizeObject();
    void EraseAddRowsOrCols();

private:
    CEditBuffer   *m_pBuffer;
    LO_Element    *m_pLoElement;
    XP_Rect        m_Rect;
    int            m_iStyle;
    int32          m_iXOrigin;
    int32          m_iYOrigin;
    int32          m_iStartWidth;
    int32          m_iStartHeight;
    int32          m_iParentWidth;
    int32          m_iViewWidth;
    int32          m_iViewHeight;
    int32          m_iAddCols;
    int32          m_iAddRows;
    XP_Bool        m_bWidthPercent;
    XP_Bool        m_bHeightPercent;
    XP_Bool        m_bPercentOriginal;
    XP_Bool        m_bFirstTime;
    XP_Bool        m_bCenterSizing;
    int            m_iWidthMsgID;
};



//
// CEditBuffer
//

// Matches LO_NA constants, plus up/down

#define EDT_NA_CHARACTER 0
#define EDT_NA_WORD 1
#define EDT_NA_LINEEDGE 2
#define EDT_NA_DOCUMENT 3
#define EDT_NA_UPDOWN 4
#define EDT_NA_COUNT 5

class CEditBuffer {
public:
    long m_lifeFlag; /* Used to detect accesses to deleted buffer. */
    MWContext *m_pContext;
    CEditRootDocElement *m_pRoot;
    // To do: replace m_pCurrent et. al. with a CEditInsertPoint
    CEditLeafElement *m_pCurrent;
    ElementOffset m_iCurrentOffset;
    XP_Bool m_bCurrentStickyAfter;

    CEditElement* m_pCreationCursor;

    ED_Color m_colorText;
    ED_Color m_colorBackground;
    ED_Color m_colorLink;
    ED_Color m_colorFollowedLink;
    ED_Color m_colorActiveLink;
    char *m_pTitle;
    char *m_pBackgroundImage;
    XP_Bool m_bBackgroundNoSave;
    char *m_pFontDefURL;
    XP_Bool m_bFontDefNoSave;
    char *m_pBaseTarget;
    char *m_pBodyExtra;
    CEditImageLoader *m_pLoadingImage;
    CFileSaveObject *m_pSaveObject;
    XP_Bool m_bMultiSpaceMode;
    
    // Save copied style here
    // By making it static, we can paste attributes
    //   into a different window
    static EDT_CharacterData *m_pCopyStyleCharacterData;

    int m_hackFontSize;             // not a real implementation, just for testing

    int32 m_iDesiredX;
    int32 m_lastTopY;
    XP_Bool m_inScroll;
    XP_Bool m_bBlocked;                // we are doing the initial layout so we
                                    //  are blocked.
    XP_Bool m_bSelecting;
    XP_Bool m_bNoRelayout;               // maybe should be a counter

    CEditElement *m_pSelectStart;
    int m_iSelectOffset;

    int m_preformatLinePos;
    XP_Bool m_bInPreformat;             // semaphore to keep us from reentering
                                    //  NormalizePreformat
    TagType m_iLastTagType; // The previous tag's type, comments are ignored.
    XP_Bool m_bLastTagIsEnd; // The previous tag's open vs. close state, comments are ignored.


    CPrintState printState;
    CEditLinkManager linkManager;
    TXP_GrowableArray_EDT_MetaData m_metaData;
    
    // Get time and save in m_FileSaveTime
    // Public so CEditFileSave can call it
    void  GetFileWriteTime();

    // Used only by CEditBuffer::Relayout()
    TXP_GrowableArray_LO_TableStruct m_RelayoutTables;

    PRBool m_bEncrypt;

private:
    CParseState*    GetParseState();
    void            PushParseState();
    void            PopParseState();
    void            ResetParseStateStack();
    TXP_PtrStack_CParseState    m_parseStateStack;

    // CFileFileSaveObject->m_status copied here
    //  to return a result even if save object is deleted
    ED_FileError m_status;

    XP_Bool IsSelectionComment(PA_Tag* pTag);
    XP_Bool HandleSelectionComment(PA_Tag* pTag, CEditElement*& pElement, intn& retVal);
    CEditCommandLog* GetCommandLog(){ XP_ASSERT(m_pCommandLog); return m_pCommandLog; };
    CEditCommandLog* m_pCommandLog;
    XP_Bool m_bTyping;
    CEditInternalAnchorElement* m_pStartSelectionAnchor;
    CEditInternalAnchorElement* m_pEndSelectionAnchor;
    XP_Bool m_bStartSelectionStickyAfter;
    XP_Bool m_bEndSelectionStickyAfter;

    XP_Bool m_bLayoutBackpointersDirty;
    //CLM: Save the current file time to
    //     check if an outside editor changed our source
    int32 m_iFileWriteTime;

    // For maintaining the selected Table, row, columns, or individual cell
    // Only 1 table can be selected, but multiple cells are selected for rows, columns
    // Array of SelectedCell structs matches LO_Elements to CEditElements
    TXP_GrowableArray_CEditTableCellElement m_SelectedEdCells;
    TXP_GrowableArray_LO_CellStruct         m_SelectedLoCells;
    CEditTableElement                      *m_pSelectedEdTable;
    LO_TableStruct                         *m_pSelectedLoTable;
    
    // Index to the next selected cell from the m_SelectedEdCells list.
    // This and m_CurrentInsertPoint are used by GetFirstCellSelection() and SelectNextSelectedCell()
    intn m_iNextSelectedCell;

    // Save the current insert point before mucking with selected cells
    // Don't need this as long as we suppress relayout for individual
    //   cell selection format changes. Just relayout entire table when done
//    CEditInsertPoint m_CurrentInsertPoint;

    // This is either m_pSelectedLoTable or last selected cell 
    //   (which will be a member of m_SelectedLoCells list)
    LO_Element     *m_pSelectedTableElement;
    ED_HitType      m_TableHitType;

    // Used with ExtentTableCellSelection:
    //  save previous cell we used to extend selection so we change
    //  selection only when we move into a different cell
    LO_Element     *m_pPrevExtendSelectionCell;

#ifdef DEBUG
    friend CEditTestManager;
    CEditTestManager* m_pTestManager;
    int16 m_iSuppressPhantomInsertPointCheck;
    XP_Bool m_bSkipValidation;
#endif

public:
    CEditBuffer( MWContext* pContext );
    ~CEditBuffer();
    CEditElement* CreateElement(PA_Tag* pTag, CEditElement *pParent);
    CEditElement* CreateFontElement(PA_Tag* pTag, CEditElement *pParent);
    intn ParseTag(pa_DocData *data_doc, PA_Tag* pTag, intn status);

private:
    XP_Bool ShouldAutoStartBody(PA_Tag* pTag, int16 csid);
    intn ParseOpenTag(pa_DocData *data_doc, PA_Tag* pTag, intn status);
    void ParseUnsupportedTag(PA_Tag* pTag, CEditElement*& pElement, intn& retVal);
    void ParseLink(PA_Tag* pTag, CEditElement*& pElement, intn& retVal);
    void ParseLinkFontDef(PA_Tag* pTag, CEditElement*& pElement, intn& retVal);
    intn ParseEndTag(PA_Tag* pTag);
    void WriteClosingScriptTag();
public:
    void FinishedLoad();
    void FinishedLoad2();
    void DummyCharacterAddedDuringLoad();

    XP_Bool IsComposeWindow(){ return m_pContext->bIsComposeWindow; }

    void PrintTree( CEditElement* m_pRoot );
    void DebugPrintTree( CEditElement* m_pRoot );
    XP_HUGE_CHAR_PTR GetPositionalText();
    void StreamToPositionalText( CEditElement* pElement, IStreamOut* pOut );

    void PrintDocumentHead( CPrintState *pPrintState );
    void PrintDocumentEnd( CPrintState *pPrintState );
    XP_Bool IsBodyTagRequired();
    static void Write(CStreamOutMemory *pSource, IStreamOut* pDest);
    void AppendTitle( char* pTitleString );

    void RepairAndSet(CEditSelection& selection);
    XP_Bool Reduce( CEditElement* pRoot );
    void NormalizeTree( );
    
    // Change Table and alls cell width data to match
    //  sizes calculated by Layout. Must do for all tables during Relayout()
    //  else generated HTML is very misleading!
    void FixupTableData();
    CEditElement* FindRelayoutStart( CEditElement *pStartElement );
    void Relayout( CEditElement *pStartElement, int iOffset,
            CEditElement *pEndElement = 0, intn relayoutFlags = 0 );

	void Reflow( CEditElement *pStartElement, int iOffset,
            CEditElement *pEndElement = 0, intn relayoutFlags = 0 );

    // Call these instead of Relayout( pTable )
    //  when we want to relayout entire table because we are changing
    //  either the table size or cell size(s)
    // bChangeWidth and bChangeHeight tell us which dimension is changing
    void ResizeTable(CEditTableElement *pTable, XP_Bool bChangeWidth = FALSE, XP_Bool bChangeHeight = FALSE);
    void ResizeTableCell(CEditTableElement *pTable, XP_Bool bChangeWidth = FALSE, XP_Bool bChangeHeight = FALSE);

    // Relayout current selected table (or parent of selected cells)
    void RelayoutSelectedTable();

    void SetCaret();
    void InternalSetCaret(XP_Bool bRevealPosition);
    LO_Position GetEffectiveCaretLOPosition(CEditElement* pElement, intn iOffset, XP_Bool bStickyAfter);
    void RevealPosition(CEditElement* pElement, int iOffset, XP_Bool bStickyAfter);
    void RevealSelection();
    XP_Bool GetLOCaretPosition(CEditElement* pElement, int iOffset, XP_Bool bStickyAfter,
        int32& targetX, int32& targetYLow, int32& targetYHigh);
    void WindowScrolled();
    void NavigateDocument(XP_Bool bSelect, XP_Bool bForward );
    /* Begin of document check preference and if move curosr is enabled, 
        we will move the cursor if not, we leave the cursor alone.
        REQUIRES: (Boolean to select or not), Valid editbuffer
        RETURNS: Nothing
        NOTE: uses
        NavigateChunk( bSelect, LO_NA_DOCUMENT, FALSE ); 
            if we move the cursor.
        and
        FE_ScrollDocTo (MWContext *context, int iLocation, int32 x,int32 y);
            if we do not
    */
    void PageUpDown(XP_Bool select, XP_Bool pagedown);
    /*PageUpDown requires:  Boolean to select the scrolled text if cursor should move
                            Boolean , true if page down, false if page up.
                  effects:  moves cursor or not depending on preferences.*/

    ED_ElementType GetCurrentElementType(); /* CM: "Current" is superfluous! */

    //
    // Doesn't allocate.  Returns the base URL of the document if it has been
    //  saved.  NULL if it hasn't.
    //
    char* GetBaseURL(){ return LO_GetBaseURL( m_pContext); }

    // editor buffer commands
    EDT_ClipboardResult InsertChar( int newChar, XP_Bool bTyping );
    EDT_ClipboardResult InsertChars( char* pNewChars, XP_Bool bTyping , XP_Bool bReduce);
    EDT_ClipboardResult DeletePreviousChar();
    EDT_ClipboardResult DeleteNextChar();
    EDT_ClipboardResult DeleteChar(XP_Bool bForward, XP_Bool bTyping = TRUE);

    // Delete key clears contents, but doesn't delete cells
    EDT_ClipboardResult ClearSelectedCells();

    CPersistentEditSelection GetEffectiveDeleteSelection();

    XP_Bool PreviousChar( XP_Bool bSelect );
    XP_Bool PrevPosition(CEditLeafElement *pEle, ElementOffset iOffset, CEditLeafElement*& pNew,
            ElementOffset& iNewOffset );
    XP_Bool NextPosition(CEditLeafElement *pEle, ElementOffset iOffset, CEditLeafElement*& pNew,
            ElementOffset& iNewOffset );
    void SelectNextChar( );
    void SelectPreviousChar( );
    void NextChar( XP_Bool bSelect );
    // Return FALSE if at the 1st or last cell
    // Option pRowCounter: if supplied, it is incremented when we move to a new row
    XP_Bool NextTableCell( XP_Bool bSelect, XP_Bool bForward, intn* pRowCounter = NULL );
    void UpDown( XP_Bool bSelect, XP_Bool bForward );
    void NavigateChunk( XP_Bool bSelect, intn chunkType, XP_Bool bForward );
    void EndOfLine( XP_Bool bSelect );
    //void BeginOfDocument( XP_Bool bSelect );
    //void EndOfDocument( XP_Bool bSelect );
    void ClearMailQuote();
    EDT_ClipboardResult ReturnKey(XP_Bool bTyping);
    EDT_ClipboardResult TabKey(XP_Bool bForward, XP_Bool bForceTabChar);
    EDT_ClipboardResult InternalReturnKey(XP_Bool bRelayout);
    void Indent();
    void IndentSelection(CEditSelection& selection);
    void IndentContainer( CEditContainerElement *pContainer,
            CEditListElement *pList );
    XP_Bool Outdent(); /* Returns TRUE if Outdent succeeded */
    XP_Bool OutdentSelection(CEditSelection& selection);
    void OutdentContainer( CEditContainerElement *pContainer,
            CEditListElement *pList );
    void MorphContainer( TagType t );
    void MorphListContainer( TagType t );

    // Added to use for selected regions and table cells
    void MorphContainerSelection( TagType t, CEditSelection& selection  );
    void MorphListContainer2( TagType t, CEditSelection& selection );
    void ToggleList(intn iTagType);
    void ToggleList2(intn iTagType, CEditSelection& selection);

    void SetParagraphAlign( ED_Alignment eAlign );
    void SetTableAlign( ED_Alignment eAlign );
    ED_Alignment GetParagraphAlign();
    void FormatCharacter( ED_TextFormat tf );

    // Format Character was modified to handle selected cells
    enum {
        ED_GET_FORMAT,
        ED_SET_FORMAT,
        ED_CLEAR_FORMAT
    };
    // This does similar scan of elements in selection, but only to find out if we should set
    //   or clear an attribute. Call this for a set of non-contiguous selected cells,
    //   then use result to set the iSetState for FormatCharacterSelection
    XP_Bool GetSetStateForCharacterSelection( ED_TextFormat tf, CEditSelection& selection );

    // iSetState default will get the set state in one pass,
    //   else it will use supplied state
    void FormatCharacterSelection( ED_TextFormat tf, CEditSelection& selection, XP_Bool bRelayout, intn iSetState = 0 );

    void SetCharacterData( EDT_CharacterData *pData );
    void SetCharacterDataSelection( EDT_CharacterData *pData, CEditSelection& selection, XP_Bool bRelayout );
    void SetCharacterDataAtOffset( EDT_CharacterData *pData,
            ED_BufferOffset iBufOffset, int32 iLen );

    EDT_CharacterData* GetCharacterData();
    // Pass in existing data to combine with previous data,
    //   used primarily for selected table cells
    EDT_CharacterData* GetCharacterDataSelection(EDT_CharacterData *pData, CEditSelection& selection);

    // Format all text contents into tab-delimeted cells,
    //  with CR/LF (or appropriate end-line for platform) at end of each row.
    // Use result to paste into spreadsheets like Excel
    char *GetTabDelimitedTextFromSelectedCells();

    // Convert Selected text into a table (put each paragraph in separate cell)
    // Number of rows is automatic - creates as many as needed
    void ConvertTextToTable(intn iColumns);

    // Convert the table into text - unravel existing paragraphs in cells
    void ConvertTableToText();

    // Save the character and paragraph style of selection or at caret
    void CopyStyle();
    
    // This is TRUE after EDT_CopyStyle is called, until the next left mouse up call
    XP_Bool CanPasteStyle() { return (m_pCopyStyleCharacterData != NULL); }

    // Apply the style to selection or at caret. Use bApplyStyle = FALSE to cancel
    void PasteStyle(XP_Bool bApplyStyle);

    // Get the index of the current font (at caret or selection)
    // Returns 0 (ED_FONT_VARIABLE), 1 (ED_FONT_FIXED), or 2 (ED_FONT_LOCAL)
    int GetFontFaceIndex();
    
    // Get the current font face for current selection or insert point
    // If the current font matches an XP font 'group',
    //    else it is the font face string from EDT_CharacterData.
    //    Use this to search your local font list
    //    in menu or listbox. DO NOT FREE RETURN VALUE (its a static string)
    char * GetFontFace();

    void ReplaceTextAtOffset( char *pText, EDT_CharacterData *pData,
            ED_BufferOffset iBufOffset, int32 iLen );

    void SetRefresh( XP_Bool bRefreshOn );

    void SetDisplayParagraphMarks(XP_Bool bDisplay);
    XP_Bool GetDisplayParagraphMarks();

    void SetDisplayTables(XP_Bool bDisplay);
    XP_Bool GetDisplayTables();

    ED_TextFormat GetCharacterFormatting();
    TagType GetParagraphFormatting();
    TagType GetParagraphFormattingSelection(CEditSelection& selection);
    int GetFontSize();
    void SetFontSize(int n);
    void SetFontSizeSelection(int n, CEditSelection& selection, XP_Bool bRelayout);
    int GetFontPointSize();
    void SetFontPointSize(int pointSize);
    ED_Color GetFontColor();
    void SetFontColor(ED_Color n);
    void SetFontColorSelection(ED_Color n, CEditSelection& selection, XP_Bool bRelayout);
    void InsertLeaf(CEditLeafElement *pLeaf);
    void InsertNonLeaf( CEditElement* pNonLeaf);
    EDT_ImageData* GetImageData();
    void SetImageData( EDT_ImageData* pData, XP_Bool bKeepImagesWithDoc );
    void InsertImage( EDT_ImageData* pData );
    int32 GetDefaultBorderWidth();
    void LoadImage( EDT_ImageData* pData, XP_Bool bKeepImagesWithDoc, XP_Bool bReplaceImage );

    // Tables

    // Like GetInsertPoint, but handles the right edge of the
    // selection differently: If the right edge of a non-empty
    // selection is at the edge of the table cell, the insert point
    // is moved inside the table cell. This gives the
    // correct behavior for table operations.
    void GetTableInsertPoint(CEditInsertPoint& ip);
    
    // Set insert point to beginning or end of cell contents
    // Returns TRUE if we found an appropriate 
    //   paragraph + text element inside cell
    XP_Bool SetTableInsertPoint(CEditTableCellElement *pCell, XP_Bool bStartOfCell = FALSE);

    XP_Bool IsInsertPointInTable();
    XP_Bool IsInsertPointInNestedTable();
    EDT_TableData* GetTableData();
    void SetTableData(EDT_TableData *pData);
    void InsertTable(EDT_TableData *pData);
    void DeleteTable();

    //cmanske: new:
    ED_MergeType GetMergeTableCellsType();
    XP_Bool CanSplitTableCell();
    void MergeTableCells();
    void SplitTableCell();

    XP_Bool IsInsertPointInTableCaption();
    EDT_TableCaptionData* GetTableCaptionData();
    void SetTableCaptionData(EDT_TableCaptionData *pData);
    void InsertTableCaption(EDT_TableCaptionData *pData);
    void DeleteTableCaption();

    XP_Bool IsInsertPointInTableRow();
    EDT_TableRowData* GetTableRowData();
    void SetTableRowData(EDT_TableRowData *pData);
    void InsertTableRows(EDT_TableRowData *pData, XP_Bool bAfterCurrentRow, intn number);
    void DeleteTableRows(intn number);

    XP_Bool IsInsertPointInTableCell();
    EDT_TableCellData* GetTableCellData();

    void ChangeTableSelection(ED_HitType iHitType, ED_MoveSelType iMoveType, EDT_TableCellData *pData = NULL);
    
    // This examines all selected cells and 
    //  returns appropriate type, including checking if
    //  all cells in a row or column are selected
    ED_HitType GetTableSelectionType();

    void SelectAndMoveToNextTableCell(XP_Bool bForward, EDT_TableCellData *pData = NULL);
    void SetTableCellData(EDT_TableCellData *pData);
    void InsertTableCells(EDT_TableCellData *pData, XP_Bool bAfterCurrentCell, intn number);
    void DeleteTableCells(intn number);
    void InsertTableColumns(EDT_TableCellData *pData, XP_Bool bAfterCurrentColumn, intn number);
    void DeleteTableColumns(intn number);

    XP_Bool IsInsertPointInLayer();
    EDT_LayerData* GetLayerData();
    void SetLayerData(EDT_LayerData *pData);
    void InsertLayer(EDT_LayerData *pData);
    void DeleteLayer();

    EDT_PageData* GetPageData();
    void SetPageData( EDT_PageData* pData );
    static void FreePageData( EDT_PageData* pData );
    void SetImageAsBackground();
    
    intn MetaDataCount( );
    intn FindMetaData( EDT_MetaData *pMetaData );
    intn FindMetaData( XP_Bool bHttpEquiv, char* pName );
    intn FindMetaDataIndex( EDT_MetaData *pMetaData );
    EDT_MetaData* MakeMetaData( XP_Bool bHttpEquiv, char *pName, char*pContent );
    EDT_MetaData* GetMetaData( intn n );
    void SetMetaData( EDT_MetaData *pMetaData );
    void DeleteMetaData( EDT_MetaData *pMetaData );
    static void FreeMetaData( EDT_MetaData *pMetaData );
    void ParseMetaTag( PA_Tag *pTag );
    void PrintMetaData( CPrintState *pPrintState );
    void PrintMetaData( CPrintState *pPrintState, int index );

    EDT_HorizRuleData* GetHorizRuleData();
    void SetHorizRuleData( EDT_HorizRuleData* pData );
    void InsertHorizRule( EDT_HorizRuleData* pData );

    char* GetTargetData();
    void SetTargetData( char* pData );
    void InsertTarget( char* pData );
    char* GetAllDocumentTargets();
    char* GetAllDocumentTargetsInFile(char *pHref);  //CLM
    char* GetAllDocumentFiles(XP_Bool **, XP_Bool);

    char* GetUnknownTagData();
    void SetUnknownTagData( char* pData );
    void InsertUnknownTag( char* pData );

    EDT_ListData* GetListData();
    void SetListData( EDT_ListData* pData );

    void InsertBreak( ED_BreakType eBreak, XP_Bool bTyping = TRUE );

    void SetInsertPoint( CEditLeafElement* pElement, int iOffset, XP_Bool bStickyAfter );
    void FixupInsertPoint();
    void FixupInsertPoint(CEditInsertPoint& ip);
    EDT_ClipboardResult DeleteSelection(XP_Bool bCopyAppendAttributes = TRUE);
    EDT_ClipboardResult DeleteSelection(CEditSelection& selection, XP_Bool bCopyAppendAttributes = TRUE);
    void DeleteBetweenPoints( CEditLeafElement* pBegin, CEditLeafElement* pEnd, XP_Bool bCopyAppendAttributes = TRUE );
    void PositionCaret( int32 x, int32 y);

    // Show where we can drop during  dragNdrop. Handles tables as well
    XP_Bool PositionDropCaret(int32 x, int32 y);
    void DeleteSelectionAndPositionCaret(int32 x, int32 y);

    // If all cells in a col or row are selected,
    //  this removes the row or col. Otherwise, just cell contents
    //  are deleted to minimize disturbing table structure
    void DeleteSelectedCells();

    // Data for table currently being dragged.  Accessible to all edit windows
    static EDT_DragTableData* m_pDragTableData;

    // Setup data for dragging table cells
    XP_Bool StartDragTable(int32 x, int32 y);
    static XP_Bool IsDraggingTable() { return m_pDragTableData != NULL; }
    static void StopDragTable();

    /*StartSelection:
        used when mouse is clicked or double clicked.  calls clearmove then done typing before 
        going on to LO_Click/LO_DoubleClick.
    */
    void StartSelection( int32 x, int32 y , XP_Bool doubleClick = FALSE);
    void MoveAndHideCaretInTable(LO_Element *pLoElement); //cmanske: Move caret without scrolling or showing caret
    // Moves caret to an existing cell -- use after deleting a cell or row, but before Relayout();
    void MoveToExistingCell(CEditTableElement *pTable, int32 X, int32 Y);

    void SelectObject( int32 x, int32 y);
    void ExtendSelection( int32 x, int32 y );
    void ExtendSelectionElement( CEditLeafElement *pElement, int iOffset, XP_Bool bStickyAfter );
    void EndSelection(int32 x, int32 y);
    void EndSelection();
    void SelectAll();
    void SelectTable();         //See below for new overloaded version

    void SelectTableCell();     //Now selects the cell border, not contents
    // Similar to the old version of SelectTableCell: selects a cells contents
    void SelectCellContents(CEditTableCellElement *pCell);

    // Very important for new Table/Cell selection strategy:
    // 1. Check if IsTableOrCellSelected() is TRUE,
    // 2. Call GetFirstCellSelection to contents of
    //     the "selected" table cell (from m_pSelectedEdTable or m_SelectedEdCells)
    // 3. Do whatever formating on returned selection data
    // 4. Call GetNextCellSelection() and repeat formatting
    //     until return value is FALSE.
    XP_Bool GetFirstCellSelection(CEditSelection& selection);
    XP_Bool GetNextCellSelection(CEditSelection& selection);

    // Similar to above, but just returns the cell pointer
    CEditTableCellElement *GetFirstSelectedCell();
    // *pRowCounter is incremented when 1st cell of next row is returned
    // Use these to build a selected table for cut/copy/paste of cells
    CEditTableCellElement *GetNextSelectedCell(intn *pRowCounter = NULL);


    void SelectRegion(CEditLeafElement *pBegin, intn iBeginPos,
            CEditLeafElement* pEnd, intn iEndPos, XP_Bool bFromStart = FALSE,
            XP_Bool bForward = FALSE );
    void SetSelection(CEditSelection& selection);
    void SetInsertPoint(CEditInsertPoint& insertPoint);
    void SetInsertPoint(CPersistentEditInsertPoint& insertPoint);

    void GetInsertPoint( CEditLeafElement** ppLeaf, ElementOffset *pOffset, XP_Bool * pbStickyAfter);
    XP_Bool GetPropertyPoint( CEditLeafElement** ppLeaf, ElementOffset *pOffset);
    CEditElement *GetSelectedElement();
    void SelectCurrentElement();
    void ClearSelection( XP_Bool bResyncInsertPoint = TRUE, XP_Bool bKeepLeft = FALSE );
    void BeginSelection( XP_Bool bExtend = FALSE, XP_Bool bFromStart = FALSE );
    void MakeSelectionEndPoints( CEditLeafElement*& pBegin, CEditLeafElement*& pEnd );
    void MakeSelectionEndPoints( CEditSelection& selection, CEditLeafElement*& pBegin, CEditLeafElement*& pEnd );
    
    void GetSelection( CEditLeafElement*& pStartElement, ElementOffset& iStartOffset,
                CEditLeafElement*& pEndElement, ElementOffset& iEndOffset, XP_Bool& bFromStart );
    
    // Use data from supplied selection, or get from current selection if supplied is empty
    // Used were above used to be used -- allows using "selected" cell data
    void GetSelection( CEditSelection& selection, CEditLeafElement*& pStartElement, ElementOffset& iStartOffset,
                CEditLeafElement*& pEndElement, ElementOffset& iEndOffset, XP_Bool& bFromStart );

    int Compare( CEditElement *p1, int i1Offset, CEditElement *p2, int i2Offset );

    void FileFetchComplete(ED_FileError m_status = ED_ERROR_NONE);

    void AutoSaveCallback();
    void SetAutoSavePeriod(int32 minutes);
    int32 GetAutoSavePeriod();
    void SuspendAutoSave(); //CLM
    void ResumeAutoSave();

    void ClearMove(XP_Bool bFlushRelayout = TRUE);
    XP_Bool IsSelected(){ return LO_IsSelected( m_pContext ); }
    XP_Bool IsSelecting(){ return m_bSelecting; }
    XP_Bool IsBlocked(){ return this == 0 || m_bBlocked; }
    XP_Bool IsPhantomInsertPoint();
    XP_Bool IsPhantomInsertPoint(CEditInsertPoint& ip);
    void ClearPhantomInsertPoint();
    XP_Bool GetDirtyFlag();
    void DocumentStored();

    EDT_ClipboardResult CanCut(XP_Bool bStrictChecking);
    EDT_ClipboardResult CanCut(CEditSelection& selection, XP_Bool bStrictChecking);
    EDT_ClipboardResult CanCopy(XP_Bool bStrictChecking);
    EDT_ClipboardResult CanCopy(CEditSelection& selection, XP_Bool bStrictChecking);
    EDT_ClipboardResult CanPaste(XP_Bool bStrictChecking);
    EDT_ClipboardResult CanPaste(CEditSelection& selection, XP_Bool bStrictChecking);

    XP_Bool CanSetHREF();
    char *GetHREF();
    ED_LinkId GetHREFLinkID();
    char *GetHREFText();
    void GetHREFData( EDT_HREFData *pData );
    void SetHREFData( EDT_HREFData *pData );
    void SetHREF( char *pHREF, char *pExtra );
    void SetHREFSelection( ED_LinkId eId );

    // Accessor functions

    ED_FileError SaveFile( ED_SaveFinishedOption finishedOpt,
                            char * pSourceURL, // used to resolve links in editor document.
                            ITapeFileSystem *tapeFS,  /* char *pDestURL, */
                            XP_Bool   bSaveAs,
                            XP_Bool   bKeepImagesWithDoc,
                            XP_Bool   bAutoAdjustLinks,
                            XP_Bool   bAutoSave,
                            char **ppIncludedFiles,
                            CEditSaveToTempData *pData = NULL);
    // Does actual save, from plugin callback.
    void SaveFileReal( CEditSaveData *pData);

    // Returns the desired on-disk, published output character set.
    int16 GetDocCharSetID();

    // Returns the current in-memory character set ID.
    // Will be the same as the wincsid, except without the AUTODETECT bits,
    // and except when the user has just changed the view encoding.
    int16 GetRAMCharSetID();

    void ForceDocCharSetID(int16 csid);

    ED_FileError PublishFile( ED_SaveFinishedOption finishedOpt,
                           char *pSourceURL,
                           char **ppIncludedFiles,
                           char *pDestURL, /* Must have trailing slash, ie after HEAD call */
                           char *pUsername,
                           char *pPassword,
                           XP_Bool   bKeepImagesWithDoc,
                           XP_Bool   bAutoAdjustLinks );

////    ED_FileError WriteBufferToFile( char* pFileName );

    //CLM: Get current time and return TRUE if it changed
    XP_Bool IsFileModified();

    // saves the edit buffer to a file.
    //CM: was char *pFileName, added return value 0 for success, -1 if fail
    int  WriteToFile( XP_File hFile );
    // returns length of pBuffer. (You can't use XP_STRLEN on a XP_HUGE_CHAR_PTR.)
    int32 WriteToBuffer( XP_HUGE_CHAR_PTR* pBuffer, XP_Bool bEncodeSelectionAsComment );

    // Warning! This will delete the CEditBuffer.
    void ReadFromBuffer(XP_HUGE_CHAR_PTR pBuffer);

    void WriteToStream( IStreamOut *stream );

    // Delegates to WriteToBuffer() and ReadFromBuffer().
    CEditDocState *RecordState();
    void RestoreState(CEditDocState *);

#ifdef DEBUG
    void DebugPrintState(IStreamOut& stream);
    void ValidateTree();
    void SuppressPhantomInsertPointCheck(XP_Bool bSuppress);
#endif
    void DisplaySource();
    void InitEscapes();
    char* NormalizeText( char* pSrc );
    intn NormalizePreformatText( pa_DocData *pData, PA_Tag *pTag, intn status );
    MWContext *GetContext(){ return m_pContext; }
    EDT_ClipboardResult PasteQuoteBegin(XP_Bool bHTML);
    EDT_ClipboardResult PasteQuote(char *pText);
    EDT_ClipboardResult PasteQuoteINTL(char *pText, int16 csid);
    EDT_ClipboardResult PasteQuoteEnd();
    void PasteHTMLHook(CPrintState* pPrintState); // For CPrintState.

    EDT_ClipboardResult PasteText(char *pText, XP_Bool bMailQuote, XP_Bool bIsContinueTyping, XP_Bool bRelayout, XP_Bool bReduce); /* Deprecated. */
    EDT_ClipboardResult PasteText(char *pText, XP_Bool bMailQuote, XP_Bool bIsContinueTyping, int16 csid,  XP_Bool bRelayout, XP_Bool bReduce);
    EDT_ClipboardResult PasteHTML(char *pText, XP_Bool bUndoRedo);
    EDT_ClipboardResult PasteHTML(IStreamIn& stream, XP_Bool bUndoRedo);
    EDT_ClipboardResult PasteCellsIntoTable(IStreamIn& stream, EEditCopyType iCopyType);
    EDT_ClipboardResult PasteHREF( char **ppHref, char **ppTitle, int iCount);
    EDT_ClipboardResult CopySelection( char **ppText, int32* pTextLen,
            char **ppHtml, int32* pHtmlLen);

    XP_Bool CopyBetweenPoints( CEditElement *pBegin,
                    CEditElement *pEnd, char **ppText, int32* pTextLen,
                    char **ppHtml, int32* pHtmlLen );
    XP_Bool CopySelectionContents( CEditSelection& selection,
                    char **ppHtml, int32* pHtmlLen, EEditCopyType iCopyType = eCopyNormal );
    //cmanske: Added flags param to pass info about copied table elements
    XP_Bool CopySelectionContents( CEditSelection& selection,
                    IStreamOut& stream, EEditCopyType iCopyType = eCopyNormal );
    int32 GetClipboardSignature();
    int32 GetClipboardVersion();
    EDT_ClipboardResult CutSelection( char **ppText, int32* pTextLen,
            char **ppHtml, int32* pHtmlLen);
    XP_Bool CutSelectionContents( CEditSelection& selection,
                    char **ppHtml, int32* pHtmlLen );

    // cmanske: New Table-related cut/paste functions
    // Returns FALSE if number of cells in each row are not all the same
    XP_Bool CountRowsAndColsInPasteText(char *pText, intn* pRows, intn* pCols);
    // Supply Rows, Cols if known, else they will be calculated from text 
    EDT_ClipboardResult PasteTextAsNewTable(char *pText, intn iRows = 0, intn iCols = 0);
    EDT_ClipboardResult PasteTextIntoTable(char *pText, XP_Bool bReplace = FALSE, intn iRows = 0, intn iCols = 0);

    void RefreshLayout();
    void FixupSpace( XP_Bool bTyping = FALSE);
    ED_Alignment GetCurrentAlignment();
    int32 GetDesiredX(CEditLeafElement* pEle, intn iOffset, XP_Bool bStickyAfter);

    // Selection utility methods
    void GetInsertPoint(CEditInsertPoint& insertPoint);
    void GetInsertPoint(CPersistentEditInsertPoint& insertPoint);
    void GetSelection(CEditSelection& selection);
    void GetSelection(CPersistentEditSelection& persistentSelection);
    void SetSelection(CPersistentEditSelection& persistentSelection);

    // Navigation
    XP_Bool Move(CEditInsertPoint& pt, XP_Bool forward);
    XP_Bool Move(CPersistentEditInsertPoint& pt, XP_Bool forward);
    XP_Bool CanMove(CEditInsertPoint& pt, XP_Bool forward);
    XP_Bool CanMove(CPersistentEditInsertPoint& pt, XP_Bool forward);

    void CopyEditText(CPersistentEditSelection& selection, CEditText& text);
    void CopyEditText(CEditText& text);
    void CutEditText(CEditText& text);
    void PasteEditText(CEditText& text);

    // Persistent to regular selection conversion routines
    CEditInsertPoint PersistentToEphemeral(CPersistentEditInsertPoint& persistentInsertPoint);
    CPersistentEditInsertPoint EphemeralToPersistent(CEditInsertPoint& insertPoint);
    CEditSelection PersistentToEphemeral(CPersistentEditSelection& persistentInsertPoint);
    CPersistentEditSelection EphemeralToPersistent(CEditSelection& insertPoint);

    // Command methods
    void AdoptAndDo(CEditCommand*);
    void Undo();
    void Redo();
    void Trim();

    // History limits
    intn GetCommandHistoryLimit();
    void SetCommandHistoryLimit(intn newLimit);

    // Returns NULL if out of range
    intn GetUndoCommand(intn);
    intn GetRedoCommand(intn);
    void BeginBatchChanges(intn commandID);
    void EndBatchChanges();

    XP_Bool IsWritable(); // FALSE while a plugin is executing.

    // Typing command methods.
    void DoneTyping();
    void StartTyping(XP_Bool bTyping);

//cmanske - no longer using column/row indexes
//    void SyncCursor(CEditTableElement* pTable, intn iColumn, intn iRow);
//    CEditSelection ComputeCursor(CEditTableElement* pTable, intn iColumn, intn iRow);
    void SyncCursor(CEditLayerElement* pLayer);

    void ReplaceText( char *pReplaceText, XP_Bool bReplaceAll,
					  char *pTextToLookFor, XP_Bool bCaseless, XP_Bool bBackward, XP_Bool bDoWrap );

    /* function for finding and replacing text once */
    XP_Bool ReplaceOnce(char *pReplaceText, XP_Bool bRelayout, XP_Bool bReduce);

    void ReplaceLoop(char *pReplaceText, XP_Bool bReplaceAll,
                         char *pTextToLookFor, XP_Bool bCaseless,
                         XP_Bool bBackward, XP_Bool bDoWrap );

#ifdef FIND_REPLACE
    XP_Bool FindAndReplace( EDT_FindAndReplaceData *pData );
#endif

    XP_Bool IsMultiSpaceMode(){ return m_bMultiSpaceMode; }

private:
    // CLM: Dynamic object sizing
    CSizingObject     *m_pSizingObject;

public:
    // Table sizing, selection, add row or col interface
    
    // Get the hit type from current mouse location in the doc
    //  (See ED_HitType enum in include/edttypes.h)
    // Optional: Returns the table or cell element clicked on
    // bModifierKeyPressed is applicable if selecting table (upper left corner),
    //  or moving mouse over cell to extend selection.
    //  If TRUE, returns ED_HIT_SEL_ALL_CELLS instead of ED_HIT_SEL_TABLE
    ED_HitType GetTableHitRegion(int32 xVal, int32 yVal, LO_Element **ppElement, XP_Bool bModifierKeyPressed = FALSE);

    // Tells us where to insert or replace cell (cell we are over is in *ppElement)
    ED_DropType GetTableDropRegion(int32 *pX, int32 *pY, int32 *pWidth, int32 *pHeight, LO_Element **ppElement);

    // Used within editor - use supplied cell's X and Y and get LO_CellStruct from it
    //   to pass to following version.
    // iHitType can be set to just 1 cell, or entire row or column
    XP_Bool SelectTableElement(CEditTableCellElement *pCell, ED_HitType iHitType);

    // This is only table select function exposed to FEs via EDT_SelectTableElement
    // Select a table, cell, row, or column and redraw table to show new selection
    // If selecting cells and bAppendSelection is TRUE, then current selected items will not be cleared
    //  (ignored if selecting a table - cell selection will be cleared)
    // All uses ignore x and y except when ED_HitType = ED_HIT_SEL_COL or ED_HIT_SEL_ROW,
    //   which need those coordinates to find the REAL column or row 
    //   when ROWSPAN or COLSPAN is used.
    // If pLoElement == NULL, then the current caret location is used
    //   to locate element by calling GetCurrentLoTableElement
    XP_Bool SelectTableElement(int32, int32 y, LO_Element *pLoElement, ED_HitType iHitType, 
                               XP_Bool bModifierKeyPressed = FALSE,
                               XP_Bool bExtendSelection = FALSE );

    // Helper for SelectTableElement - find appropriate element at current caret location
    LO_Element* GetCurrentLoTableElement(ED_HitType iHitType, int32 *pX, int32 *pY);

       
    // Called on mouse-move message after selection was started
    // on a row, column, or cell element
    // Returns the hit type: ED_HIT_SEL_ROW, ED_HIT_SEL_COL, ED_HIT_SEL_CELL, or ED_HIT_NONE
    //   reflecting what type of block we are extended,
    //     or ED_HIT_NONE if mouse is outside of the table
    //   Use this to set the type of cursor by the FEs
    ED_HitType ExtendTableCellSelection(int32 x, int32 y);

    // Used by ExtendTableCellSelection
    //  and SelectTableElement when bExtendSelection is TRUE,
    // Select all cells whose top and left are within a rect formed by
    //   left and top last selected cell (m_pSelectedTableElement) and pLastCell
    void SelectBlockOfCells(LO_CellStruct *pLastCell);

    // Get corresponding layout cell struct containing the given edit element
    // Null if not in a table, of course
    LO_CellStruct* GetLoCell(CEditElement *pEdElement);

    // Set or clear the two table pointers (m_pSelectedEdTable, m_pSelectedLoTable)
    // pLoTable must be supplied if bSelect is TRUE. 
    //   If FALSE, existing selected table is unselected
    // Table selection is redisplayed if changed from existing state
    //   pEdTable is optional - it will be found from pLoTable if pEdTable = NULL
    //   and returned
    CEditTableElement* SelectTable(XP_Bool bSelect, LO_TableStruct *pLoTable = NULL, CEditTableElement *pEdTable = NULL);

    // Helper to select a single cell - both Lo and Edit arrays updated
    //   and cells are redisplayed. pLoCell must be supplied, but
    //   pEdCell is optional - it will be found from pLoCell if pEdCell = NULL
    //   and returned
    CEditTableCellElement* SelectCell(XP_Bool bSelect, LO_CellStruct *pLoCell, CEditTableCellElement *pEdCell = NULL);
    
    // Set the special selection attribute used for all currently-selected cells,
    //  except for the supplied "focus" cell.
    // Front ends should always display the "special" attribute
    //  if that bit (LO_ELE_SELECTED_SPECIAL) even if LO_ELE_SELECTED ) is also set
    void DisplaySpecialCellSelection( CEditTableCellElement *pFocusCell = NULL,  EDT_TableCellData *pCellData = NULL );
    
    // Select current cell and change other currently-selected cells
    //  to LO_ELE_SELECTED_SPECIAL so user can tell difference between
    //  the "focus" cell (current, where caret is) and other selected cells
    //  Call just before Table Cell properties dialog is created
    // Supply pCellData to update iSelectionType and iSelectedCount values
    void StartSpecialCellSelection( EDT_TableCellData *pCellData = NULL );

    // Remove LO_ELE_SELECTED_SPECIAL from all currently-selected cells
    // Call after closing the Table Cell properties dialog
    void ClearSpecialCellSelection();

    // Rearrange order of cells in selected cell lists as they
    //   appear from upper left to lower right (rows first) 
    void SortSelectedCells();
    
    // Remove selection from all tables and cells (both Editor and LO_Element elements affected)
    void ClearTableAndCellSelection();
    
    // Clear any existing cells selected if current edit element is not inside selection
    void ClearCellSelectionIfNotInside();

    // If the supplied element is in our Table or Selection list,
    //  clear the selection. Call when deleting an element
    void ClearTableIfContainsElement(CEditElement *pElement);
    
    XP_Bool IsTableSelected() {return m_pSelectedEdTable != NULL; }
    XP_Bool IsTableOrCellSelected() { return (XP_Bool)(m_pSelectedEdTable ? TRUE : m_SelectedEdCells.Size()); }
    int     GetSelectedCellCount() { return m_SelectedEdCells.Size(); }


    // For special cell highlighting during drag and drop
    // pLoElement can be the table or any cell that is starting cell,
    //  all cells from starting cell will have attribute removed
//NOT USED???
//    void ClearCellAttrmask(LO_Element *pLoElement, uint16 attrmask );

    // Start Column or Row search from current element
    // This works on geometric location data, not "logical" row or column
    CEditTableCellElement* GetFirstCellInCurrentColumn();
    CEditTableCellElement* GetFirstCellInCurrentRow();

    intn GetNumberOfSelectedColumns();
    intn GetNumberOfSelectedRows();
    void MoveToFirstSelectedCell();
    void MoveToLastSelectedCell();

    // Dynamic object sizing interface
    XP_Bool       IsSizing() { return (m_pSizingObject != 0); }
    ED_SizeStyle  CanSizeObject(LO_Element *pLoElement, int32 xVal, int32 yVal);
    ED_SizeStyle  StartSizing(LO_Element *pLoElement, int32 xVal, int32 yVal, XP_Bool bLockAspect, XP_Rect *pRect);
    XP_Bool       GetSizingRect(int32 xVal, int32 yVal, XP_Bool bLockAspect, XP_Rect *pRect);
    void          EndSizing();
    void          CancelSizing();
    
    XP_Bool       IsReady();

    // Spellcheck api
    XP_Bool FindNextMisspelledWord( XP_Bool bFirst, XP_Bool bSelect,
            CEditLeafElement **ppWordStart );

    // Iterate over mispelled words, replace or ignore them.
    enum EMSW_FUNC { EMSW_IGNORE, EMSW_REPLACE };
    void IterateMisspelledWords( EMSW_FUNC eFunc, char* pOldWord, char* 
            pNewWord, XP_Bool bAll );

    LO_Element* FirstElementOnLine(LO_Element* pTarget, int32* pLineNum);

    void ChangeEncoding(int16 csid);
    void SetEncoding(int16 csid);
    XP_Bool HasEncoding();

    // Used for QA only - Ctrl+Alt+Shift+N accelerator for automated testing
    void SelectNextNonTextObject();
    CEditTableElement *m_pNonTextSelectedTable;

#ifdef EDITOR_JAVA
    EditorPluginManager GetPlugins();
#endif

public:
    static XP_Bool IsAlive(CEditBuffer* pBuffer); // Tells you if the pointer is valid.

private:
    void CheckAndPrintComment(CEditLeafElement* pElement, CEditSelection& selection, XP_Bool bEnd);
    void CheckAndPrintComment2(const CEditInsertPoint& where, CEditSelection& selection, XP_Bool bEnd);

    void SetSelectionInNewDocument();
    void ParseBodyTag(PA_Tag *pTag);
    void RecordTag(PA_Tag* pTag, XP_Bool bWithLinefeed);
    void RecordJavaScriptAsUnknownTag(CStreamOutMemory* pOut);

    void SetBaseTarget(char* pTarget);
    char* GetBaseTarget(); /* NULL if no BASE TARGET */

    CFinishLoadTimer m_finishLoadTimer;
    CRelayoutTimer m_relayoutTimer;
    CAutoSaveTimer m_autoSaveTimer;
    XP_Bool m_bDisplayTables;
    XP_Bool m_bDummyCharacterAddedDuringLoad;
    static XP_Bool m_bAutoSaving;
    XP_Bool m_bReady;
    XP_Bool m_bPasteQuoteMode;
    XP_Bool m_bPasteHTML;
    XP_Bool m_bPasteHTMLWhenSavingDocument;
    CStreamOutMemory* m_pPasteHTMLModeText;
    CConvertCSIDStreamOut* m_pPasteTranscoder;
    XP_Bool m_bAbortPasteQuote;
    // In case the user sets the encoding
    int16 m_originalWinCSID;
    XP_Bool m_bForceDocCSID;
    int16 m_forceDocCSID;

//preference information
    static XP_Bool m_bMoveCursor; //true = move cursor when pageup/down false, just move scrollbar
    static XP_Bool m_bEdtBufPrefInitialized; //are the preferences initialized
    static int PrefCallback(const char *,void *);//callback for preferences
    static void InitializePrefs();//call this function to force initialization of preferences
};


//
// CEditTagCursor -
//
class CEditTagCursor {
private: // types
    enum TagPosition { tagOpen, tagEnd };

private: // data

    CEditBuffer *m_pEditBuffer;
    CEditElement *m_pCurrentElement;
    CEditPositionComparable m_endPos;
    TagPosition m_tagPosition;
    int m_stateDepth;
    int m_currentStateDepth;
    int m_iEditOffset;
    XP_Bool m_bClearRelayoutState;
    MWContext *m_pContext;
    PA_Tag* m_pStateTags;


public:  // routines
    CEditTagCursor( CEditBuffer* pEditBuffer, CEditElement *pElement,
            int iEditOffset, CEditElement* pEndElement );
    ~CEditTagCursor();

    PA_Tag* GetNextTag();
    PA_Tag* GetNextTagState();
    XP_Bool AtBreak(XP_Bool* pEndTag);
    int32 CurrentLine();
    XP_Bool ClearRelayoutState() { return m_bClearRelayoutState; }
    CEditTagCursor* Clone();
};

//-----------------------------------------------------------------------------
//  CEditImageLoader
//-----------------------------------------------------------------------------
class CEditImageLoader {
private:
    CEditBuffer *m_pBuffer;
    EDT_ImageData *m_pImageData;
    LO_ImageStruct *m_pLoImage;
    XP_Bool m_bReplaceImage;

public:
    CEditImageLoader( CEditBuffer *pBuffer, EDT_ImageData *pImageData,
            XP_Bool bReplaceImage );
    ~CEditImageLoader();

    void LoadImage();
    void SetImageInfo(int32 ele_id, int32 width, int32 height);
};

//-----------------------------------------------------------------------------
// CEditSaveToTempData: used to call back to FE after saving document to temp
// file.  Could be expanded to be a general callback mechanism for reporting
// when saving is complete.
//-----------------------------------------------------------------------------
class CEditSaveToTempData {
public:
  CEditSaveToTempData() : doneFn(NULL), hook(NULL), pFileURL(NULL) {}
  ~CEditSaveToTempData() {XP_FREEIF(pFileURL);}

  EDT_SaveToTempCallbackFn doneFn;
  void *hook;
  char *pFileURL;
};

//-----------------------------------------------------------------------------
//  CFileSaveObject
//-----------------------------------------------------------------------------
// Takes care of FE interaction, does not know that there is anything
// special about the first file (the root HTML document for CEditSaveObject).
class CFileSaveObject {
    friend void edt_CFileSaveObjectDone(XP_Bool,void *); // A callback.

private:
    XP_Bool m_bOverwriteAll;
    XP_Bool m_bDontOverwriteAll;
    XP_Bool m_bDontOverwrite;
    int m_iCurFile; // 1-based
    IStreamOut *m_pOutStream;
    XP_Bool m_bOpenOutputHandledError;
    CEditSaveToTempData *m_pSaveToTempData;

protected:
    // Override this if the child of CFileSaveObject needs to do anything special to save the
    // first file.  O.w. it will just be copied from source to dest like everything else.
    // Return TRUE if the child took care of saving the first file.
    virtual XP_Bool SaveFirstFile() {return FALSE;}

    // can be a full file path.  Grabs the path part automatically.
    // Return the path part of pFileURL, alloced with XP_STRDUP.
    char *GetPathURL( char* pFileURL );

    // Check to see if we should call FE_FinishedSave() to tell
    // the front end that a new file has been created.
    void CheckFinishedSave(intn iFileIndex,ED_FileError iError);

    MWContext *m_pContext;
    ITapeFileSystem *m_tapeFS;
    ED_FileError m_status;
    XP_Bool m_bFromAutoSave; // This save was initiated from the auto save callback.

public:
    //
    // inherited interface.
    //
    CFileSaveObject( MWContext *pContext, char *pSrcURL, ITapeFileSystem *tapeFS, XP_Bool bAutoSave,
                     CEditSaveToTempData * ) ;
    virtual ~CFileSaveObject();

    // Return values like ITapeFileSystem::AddFile().
    intn AddFile( char *pSrcURL, char *pMIMEType, int16 iDocCharSetID );

    ED_FileError SaveFiles();

    char *GetDestName( intn index );
    char *GetDestAbsoluteURL( intn index );
    char *GetSrcName( intn index );
    // Return newly allocated string.

    // Inherit from CFileSaveObject and implement FileFetchComplete.
    //
    virtual ED_FileError FileFetchComplete();

    void Cancel();

	// Communicate content type and other information to the tape file system.
	// This function is called from edt_MakeFileSaveStream 
	// after OpenOutputFile() and before FileFetchComplete().
	// Necessary so that libmime can convey the mime type of an inferior URL
	// that represents, for example, part of a mail or news message.
	void CopyURLInfo(const URL_Struct *pURL);
	
private:
    ED_FileError FetchNextFile();
/*    intn OpenOutputFile( int iFile ); */

    // Report error given by iError on the current file being processed.
    // Return whether we sould continue.  
    // Replaces FE_SaveErrorContinueDialog().
    XP_Bool SaveErrorContinueDialog(ED_FileError iError);

// Netlib interface
public:
    // Made Public so we can access from file save stream
    //  (We don't open file until
    //   libnet succeeds in finding source data)
    intn OpenOutputFile(/*int iFile*/);
    // Net Callbacks
    int NetStreamWrite( const char *block, int32 length );
    void NetFetchDone( URL_Struct *pUrl, int status, MWContext *pContext );

};


//-----------------------------------------------------------------------------
//  CEditSaveObject
//-----------------------------------------------------------------------------
class CEditSaveObject: public CFileSaveObject{
private:
    enum {IgnoreImage = -1, AttemptAdjust = -2};

    // Can return IgnoreImage or AttemptAdjust
    // If ppIncludedFiles is non-NULL, image will only be added if it is in
    // the list.
    // If ppIncludeFiles is NULL, use bNoSave to decide whether to add it.
    intn CheckAddFile( char *pSrc, char *pMIMEType, char **ppIncludedFiles, XP_Bool bNoSave );

    ////XP_Bool IsSameURL( char* pSrcURL, char *pLocalName );

   // ppList is a NULL-terminated list of absolute URLs.
   XP_Bool URLInList (char **ppList, char *pURL);

    // helper for FixupLinks().
    void FixupLink(intn iIndex, // index to tape file system
                   char **ppImageURL,   // Value to fixup.
                   char *pDestPathURL,
                   ED_HREFList *badLinks);

    char *FixupLinks();

    CEditBuffer *m_pBuffer;
    CEditDocState *m_pDocStateSave; // Backup of m_pBuffer before doing anything.
    char *m_pSrcURL;
    //XP_Bool m_bSaveAs;
    ED_SaveFinishedOption m_finishedOpt; // Should we be editing the newly saved doc after saving is done.
    XP_Bool m_bKeepImagesWithDoc;
    XP_Bool m_bAutoAdjustLinks;
    intn m_backgroundIndex;
    intn m_fontDefIndex;

public:
    CEditSaveObject( CEditBuffer *pBuffer, 
                            ED_SaveFinishedOption finishedOpt, char *pSrcURL, ITapeFileSystem *tapeFS,
                            XP_Bool bSaveAs,
                            XP_Bool bKeepImagesWithDoc, 
                            XP_Bool bAutoAdjustLinks,
                            XP_Bool bAutoSave,
                            CEditSaveToTempData *pSaveToTempData ); 
    ~CEditSaveObject();

    // Add HTML document, then find and add all images in document.
    // Returns success or not.
    // If pIncludedFiles in non-NULL, it is a NULL-terminated list of absolute
    // URLs telling which images may be published..  
    // Free all memory pointed to by ppIncludedFiles when done.
    XP_Bool AddAllFiles(char **ppIncludedFiles);

    virtual ED_FileError FileFetchComplete();

    // If list is non-null, free it element-wise, then free the list itself.
    // list is NULL-terminated if it exists.
    static void FreeList( char **list );

    // Save the source URL when we fail to publish
    // Used by EDT_GetDefaultPublishUrl to return the previous failed destination
    static char * m_pFailedPublishUrl;

protected:
    virtual XP_Bool SaveFirstFile();
};


#if 0 //// Now obsolete
//-----------------------------------------------------------------------------
//  CEditImageSaveObject
//-----------------------------------------------------------------------------
class CEditImageSaveObject: public CFileSaveObject{
private:
    CEditBuffer *m_pBuffer;
    EDT_ImageData *m_pData;
    XP_Bool m_bReplaceImage;

public:
    intn m_srcIndex;
    intn m_lowSrcIndex;

    CEditImageSaveObject( CEditBuffer *pBuffer, EDT_ImageData *pData,
                        XP_Bool bReplaceImage );
    ~CEditImageSaveObject();

    virtual ED_FileError FileFetchComplete();
};
#endif

#if 0
//-----------------------------------------------------------------------------
//  CEditBackgroundImageSaveObject
//-----------------------------------------------------------------------------
class CEditBackgroundImageSaveObject: public CFileSaveObject{
private:
    CEditBuffer *m_pBuffer;

public:
    CEditBackgroundImageSaveObject( CEditBuffer *pBuffer );
    ~CEditBackgroundImageSaveObject();

    virtual ED_FileError FileFetchComplete();
};
#endif

extern CBitArray *edt_setNoEndTag;
extern CBitArray *edt_setWriteEndTag;
extern CBitArray *edt_setHeadTags;
extern CBitArray *edt_setSoloTags;
extern CBitArray *edt_setBlockFormat;
extern CBitArray *edt_setCharFormat;
extern CBitArray *edt_setList;
extern CBitArray *edt_setUnsupported;
extern CBitArray *edt_setAutoStartBody;
extern CBitArray *edt_setTextContainer;
extern CBitArray *edt_setListContainer;
extern CBitArray *edt_setParagraphBreak;
extern CBitArray *edt_setFormattedText;
extern CBitArray *edt_setContainerSupportsAlign;
extern CBitArray *edt_setIgnoreWhiteSpace;
extern CBitArray *edt_setSuppressNewlineBefore;
extern CBitArray *edt_setRequireNewlineAfter;
extern CBitArray *edt_setContainerBreakConvert;
extern CBitArray *edt_setContainerHasLineAfter;
extern CBitArray *edt_setIgnoreBreakAfterClose;

inline XP_Bool BitSet( CBitArray* pBitArray, int i ){ if ( i < 0 ) return FALSE;
    else return (*pBitArray)[i]; }

inline int IsListContainer( TagType i ){ return BitSet( edt_setListContainer, i ); }

inline int TagHasClose( TagType i ){ return !BitSet( edt_setNoEndTag, i ); }
inline int WriteTagClose( TagType i ){ return (BitSet( edt_setWriteEndTag, i ) ||
                TagHasClose(i)); }

//
// Utility functions.
//
char *EDT_TagString(int32 tagType);
ED_TextFormat edt_TagType2TextFormat( TagType t );
char *edt_WorkBuf( int iSize );
char *edt_QuoteString( char* pString );
char *edt_MakeParamString( char* pString );

char *edt_FetchParamString( PA_Tag *pTag, char* param, int16 win_csid );
XP_Bool edt_FetchParamBoolExist( PA_Tag *pTag, char* param, int16 win_csid );
XP_Bool edt_FetchDimension( PA_Tag *pTag, char* param,
                         int32 *pValue, XP_Bool *pPercent,
                         int32 nDefaultValue, XP_Bool bDefaultPercent, int16 win_csid );
// Old defaults: ED_ALIGN_BASELINE, FALSE
ED_Alignment edt_FetchParamAlignment( PA_Tag* pTag,
                ED_Alignment eDefault, XP_Bool bVAlign, int16 win_csid );
int32 edt_FetchParamInt( PA_Tag *pTag, char* param, int32 defValue, int16 win_csid );
int32 edt_FetchParamInt( PA_Tag *pTag, char* param, int32 defValue, int32 defValueIfParamButNoValue, int16 win_csid );
ED_Color edt_FetchParamColor( PA_Tag *pTag, char* param, int16 win_csid );

// Preserves the old value of data if there's no parameter.
void edt_FetchParamString2(PA_Tag* pTag, char* param, char*& data, int16 win_csid);
void edt_FetchParamColor2( PA_Tag *pTag, char* param, ED_Color& data, int16 win_csid );

// Concatenates onto the old value
void edt_FetchParamExtras2( PA_Tag *pTag, char**ppKnownParams, char*& data, int16 win_csid );

LO_Color* edt_MakeLoColor( ED_Color c );
void edt_SetLoColor( ED_Color c, LO_Color *pColor );

void edt_InitEscapes(int16 csid, XP_Bool bQuoteHiBits);
void edt_PrintWithEscapes( CPrintState *ps, char *p, XP_Bool bBreakLines);
char *edt_LocalName( char *pURL );
PA_Block PA_strdup( char* s );

inline char *edt_StrDup( char *pStr );
EDT_ImageData* edt_NewImageData();
EDT_ImageData* edt_DupImageData( EDT_ImageData *pOldData );
void edt_FreeImageData( EDT_ImageData *pData );

void edt_SetTagData( PA_Tag* pTag, char* pTagData);
void edt_AddTag( PA_Tag*& pStart, PA_Tag*& pEnd, TagType t, XP_Bool bIsEnd,
        char *pTagData = 0 );

void edt_InitBitArrays();

// Given an absolute URL, return the path portion of it, allocated with XP_STRDUP.
char *edt_GetPathURL(char *);

//
// Convert NBSPs into spaces.
//
void edt_RemoveNBSP( int16 csid, char *pString );

//
// Retreive extra parameters and return them in the form:
//
//  ' foo=bar zoo mombo="fds ifds ifds">'
//
char* edt_FetchParamExtras( PA_Tag *pTag, char**ppKnownParams, int16 win_csid );

// Replace the Value of a Param, rebuilding the Tag data within given tag
// Returns TRUE if param NAME was already in the list. If it wasn't,
//   it is added to end of tag data. If pValue is NULL, pName is removed from tag
XP_Bool edt_ReplaceParamValue( PA_Tag *pTag, char * pName, char * pValue, int16 csid );

// Copies text out of a Huge pointer. Truncates it if it's too large for the platform.
// Returns a copy of the text. Caller owns the copy.
// csid is the character set id of the text. Used to make sure truncation occurs on a
// character boundary.
// pOutLength is the output length in bytes. Pass NULL if you don't care about the new length.
// The result is always zero terminated, so that if the input does not contain nulls,
// XP_STRLEN(result) == *pOutLenth.

char* edt_CopyFromHuge(int16 csid, XP_HUGE_CHAR_PTR text, int32 length, int32* pOutLength);


// Return dup'd string that is the same as pUrl except that any username/password info is removed.
char *edt_StripUsernamePassword(char *pUrl);

// Terminate string in place at "#" or "?"
void edt_StripAtHashOrQuestionMark(char *pUrl);

// Kills directory pName and, recursively, everything under it.
// pName is in xpURL format.
void edt_RemoveDirectoryR(char *pName);

// Just wrappers around the XP_* functions.  The only difference is that
// these functions will work if the directory ends in a trailing slash,
// avoiding obscure bugs on Win16.
XP_Dir edt_OpenDir(const char * name, XP_FileType type);
int edt_MakeDirectory(const char* name, XP_FileType type);
int edt_RemoveDirectory(const char *name, XP_FileType type);

// Returns the URL that should be used as the base for relative URLs in 
// the document.  Similar to just calling LO_GetBaseURL(), except returns
// the document temp directory for untitled documents.
// Must free returned memory.
char *edt_GetDocRelativeBaseURL(MWContext *pContext);

// Platform-specific end-of-line character(s) added to string,
//  returning in possibly-reallocated buffer that caller must free
char *edt_AppendEndOfLine(char *pText);
//
// Parse return values.
//
#define OK_IGNORE       2
#define OK_CONTINUE     1
#define OK_STOP         0
#define NOT_OK          -1

// Inline function implementations

inline char *edt_StrDup( char *pStr ){
    if( pStr ){ 
        return XP_STRDUP( pStr );
    }
    else {
        return 0;
    }
}

#ifdef DEBUG

class CEditTestManager {
public:
    CEditTestManager(CEditBuffer* pBuffer);
    XP_Bool Key(char key);
    XP_Bool Backspace();
    XP_Bool ReturnKey();

protected:

    void PowerOnTest();

    void DumpLoElements();
    void VerifyLoElements();
    void DumpDocumentContents();

    void PasteINTL();
    void PasteINTLText();

    void CopyBufferToDocument();
    void CopyDocumentToBuffer();

    XP_Bool ArrowTest();
    XP_Bool LArrowTest(XP_Bool bSelect);
    XP_Bool RArrowTest(XP_Bool bSelect);
    XP_Bool UArrowTest(XP_Bool bSelect);
    XP_Bool DArrowTest(XP_Bool bSelect);
    XP_Bool EveryNavigationKeyEverywhereTest();
    XP_Bool NavigateChunkCrashTest();
    XP_Bool NavChunkCrashTest(XP_Bool bSelect, int chunk, XP_Bool bDirection);
    XP_Bool DeleteKeyTest();
    XP_Bool BackspaceKeyTest();
    XP_Bool ZeroDocumentCursorTest();
    XP_Bool OneCharDocumentCursorTest();
    XP_Bool OneDayTest(int32 rounds);
    XP_Bool BoldTest(int32 rounds);
    XP_Bool TextFETest(); // Not really a test.

    void GetWholeDocumentSelection(CPersistentEditSelection& selection);
    void    DumpMemoryDelta();
#ifdef EDITOR_JAVA
    void DumpPlugins();
    void PerformFirstPlugin();
    void PerformPluginByName();
    void PerformFirstEncoder();
    void PerformPreOpen();
#endif
    void TestHTMLPaste();
    void SaveToTempFile();
    static void SaveToTempFileCB(char *pFileURL,void *hook);
    void RemoveTempFile();
    // Create the document-specific temporary directory.
    void GetDocTempDir();
private:
    CEditBuffer* m_pBuffer;
    XP_Bool m_bTesting;
    int m_iTriggerIndex;
    static const char* m_kTrigger;

    XP_HUGE_CHAR_PTR m_pSaveBuffer;

#ifdef XP_WIN32
    _CrtMemState m_state;
#endif
    char *m_pTempFileURL; // for SaveToTempFile() and RemoveTempFile().
};

#endif  //DEBUG

// Semantic sugar for debugging. Validates the tree on construction and
// destruction. Place in every method that munges the tree.
//
// VALIDATE_TREE(this);

#ifdef DEBUG
class CTreeValidater {
    CEditBuffer* m_pBuffer;
public:
    CTreeValidater(CEditBuffer* pBuffer)
     : m_pBuffer(pBuffer)
    {
        m_pBuffer->ValidateTree();
    }
    ~CTreeValidater() {
        m_pBuffer->ValidateTree();
    }
};

class CSuppressPhantomInsertPointCheck {
    CEditBuffer* m_pBuffer;
public:
    CSuppressPhantomInsertPointCheck(CEditBuffer* pBuffer)
     : m_pBuffer(pBuffer)
    {
        m_pBuffer->SuppressPhantomInsertPointCheck(TRUE);
    }
    ~CSuppressPhantomInsertPointCheck() {
        m_pBuffer->SuppressPhantomInsertPointCheck(FALSE);
    }
};

// The local variable names will cause problems if you try to
// use these macros twice in the same scope. However,
// they are nescessary to ensure that some compilers (MS VC 2.2)
// keep the object alive for the duration of the scope, instead
// of constructing and destructing it at the point of declaration.
// I think this may be a bug in the compiler -- but I'd have
// to check the ANSI standard to make sure.

#define VALIDATE_TREE(BUFFER) CTreeValidater CTreeValidater_x_zzx1(BUFFER)
#define SUPPRESS_PHANTOMINSERTPOINTCHECK(BUFFER) \
    CSuppressPhantomInsertPointCheck CSuppressPhantomInsertPointCheck_x_zzx2(BUFFER)
#else

#define VALIDATE_TREE(BUFFER)
#define SUPPRESS_PHANTOMINSERTPOINTCHECK(BUFFER)

#endif

// Find the current context's URL in the cached history data and update the
//   corresponding Title. Used by CEditBuffer::SetPageData()
void edt_UpdateEditHistoryTitle(MWContext * pMWContext, char * pTitle);

// Get the corresponding CEditTableElement if LoElementType == LO_TABLE,
//   or CEditTableCellElement if LoElementType = LO_CELL
CEditElement* edt_GetTableElementFromLO_Element(LO_Element *pLoElement, int16 LoElementType);

#endif  // _EDITOR_H
#endif  // EDITOR


