/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#ifndef _edt_h_
#define _edt_h_

#ifdef EDITOR

#ifndef _XP_Core_
#include "xp_core.h"
#endif

#ifndef _edttypes_h_
#include "edttypes.h"
#endif

XP_BEGIN_PROTOS

struct java_lang_Object;

/*****************************************************************************
 * Layout Interface
 *****************************************************************************/

/*
 *  Create an edit buffer, ready to be pared into.
 */
ED_Buffer* EDT_MakeEditBuffer(MWContext *pContext);

/*
 *  Destroy an edit buffer.
 */
void EDT_DestroyEditBuffer(MWContext * pContext);

/* CLM:
 * Front end can call this to prevent doing
 * bad stuff when we failed to load URL
 * and don't have a buffer
 */
XP_Bool EDT_HaveEditBuffer(MWContext * pContext);

/*
 * Call this to get a special string to feed the editor when you're
 * creating an empty document.
 * Returns an alias to a special empty document string.
 * (You don't own the storage, so don't free it.)
 */

char* EDT_GetEmptyDocumentString(void);

/*
 *  Parse a tag into an edit buffer.
 */
ED_Element* EDT_ParseTag(ED_Buffer* pEditBuffer, PA_Tag* pTag);

/*
 * Called instead of LO_ProcessTag.  This routine allows the Edit enginge to
 *  build up an HTML tree.  Inputs and outputs are the same as LO_ProcessTag.
*/
intn EDT_ProcessTag(void *data_object, PA_Tag *tag, intn status);

/*
 * Tells the edit engine to position the insert point.
*/
void EDT_SetInsertPoint( ED_Buffer *pBuffer, ED_Element* pElement, int iPosition, XP_Bool bStickyAfter );

/*
 * Assocates the layout element with a given edit element.  Called from by the
 *  layout engine onces a layout element has been has been created.
*/
void EDT_SetLayoutElement( ED_Element* pElement, intn iEditOffset, intn lo_type,
            LO_Element *pLayoutElement );

/* Breaks the association between a layout element and a given edit element.
 * Called by the layout engine when a layout element is being destroyed.
 */

void EDT_ResetLayoutElement( ED_Element* pElement, intn iEditOffset,
                            LO_Element* pLayoutElement);

/*
 * Delete all tags in the chain beginning with pTag.
*/
void EDT_DeleteTagChain( PA_Tag* pTag );

/*
 * Retrieve the next tag element from the editor.  Returns NULL when there are
 *  no more tags.
*/
PA_Tag* EDT_TagCursorGetNext( ED_TagCursor* pCursor );

/*
 * enclosing html for a given point in the document
*/
PA_Tag* EDT_TagCursorGetNextState( ED_TagCursor* pCursor );

/*
 * returns true if at the beginning of a paragraph or break.
*/
XP_Bool EDT_TagCursorAtBreak( ED_TagCursor *pCursor, XP_Bool* pEndTag );

/*
 * Get the current line number for the cursor
*/
int32 EDT_TagCursorCurrentLine( ED_TagCursor* pCursor );

/*
 * Get the current line number for the cursor
*/
XP_Bool EDT_TagCursorClearRelayoutState( ED_TagCursor* pCursor );


/*
 * Clone a cursor.  Used to save position state.
*/
ED_TagCursor* EDT_TagCursorClone( ED_TagCursor *pCursor );

/*
 * Delete a cloned (or otherwise) cursor
*/
void EDT_TagCursorDelete( ED_TagCursor* pCursor );

/*
 * called when LO finishes (only the first time)
*/
void EDT_FinishedLayout( MWContext *pContext );

/*
 * Call back from image library giving the height and width of an image.
*/
void EDT_SetImageInfo(MWContext *context, int32 ele_id, int32 width, int32 height);

/*
 * Test to see if the editor is currently loading a file.
*/
XP_Bool EDT_IsBlocked( MWContext *pContext );

/*****************************************************************************
 * FE Interface
 *****************************************************************************/

void EDT_SaveToBuffer ( MWContext * pContext, XP_HUGE_CHAR_PTR* pBuffer );

void EDT_ReadFromBuffer ( MWContext * pContext, XP_HUGE_CHAR_PTR pBuffer );

/*
 * Returns ED_ERROR_NONE if files saved OK, else returns error code.
*/
ED_FileError EDT_SaveFile( MWContext * pContext,
                           char * pSourceURL,
                           char * pDestURL,
                           XP_Bool   bSaveAs,
                           XP_Bool   bKeepImagesWithDoc,
                           XP_Bool   bAutoAdjustLinks );
/*
 * Returns ED_ERROR_NONE if files saved OK, else returns error code.
 */
ED_FileError EDT_PublishFile( MWContext * pContext,
                           ED_SaveFinishedOption finishedOpt,
                           char * pSourceURL,
                           char **pIncludedFiles, /* list of files to publish */
                           char * pDestURL, /* Should be http:// or ftp:// */
                           XP_Bool   bKeepImagesWithDoc,
                           XP_Bool   bAutoAdjustLinks,
                           XP_Bool   bSavePassword);  /* Tied to user checkbox in dialog */

/* 
 * Check the URL that will be passed into EDT_PublishFile. 
 * TRUE if pURL should be used, or FALSE if the user should
 * make another choice. 
 */
XP_Bool EDT_CheckPublishURL( MWContext * pContext, char * pURL);

/*
 * Save editor and images to an abstract file system.
 * Used by libmsg to write MHTML mail messages.
 */
ED_FileError EDT_SaveFileTo( MWContext * pContext,
                           ED_SaveFinishedOption finishedOpt,
                           char * pSourceURL,
                           void *tapeFS,    /* Really a (ITapeFileSystem *) */
                           XP_Bool   bKeepImagesWithDoc,
                           XP_Bool   bAutoAdjustLinks );

/*
 * Create a temporary file with the contents of the edit buffer, images and links
 * will be adjusted to work from the new location.  Call doneFn with the file:// URL
 * of the temp file or NULL if failure.  doneFn may be called before EDT_SaveToTempFile
 * returns.  Use EDT_RemoveTempFile when file is no longer needed.
 */
typedef void (*EDT_SaveToTempCallbackFn)(char *pFileURL,void *hook);
void EDT_SaveToTempFile(MWContext *pContext,EDT_SaveToTempCallbackFn doneFn, void *hook);
void EDT_RemoveTempFile(MWContext *pContext,char *pFileURL);

#if defined(MOZ_MAIL_COMPOSE) || defined(MOZ_MAIL_NEWS)
/*
 * Like MSG_MailDocument, but will deal properly with dirty or unsaved documents.
 * I.e. May save document to a temp file before attaching it to the new mail 
 * message. 
 */
void EDT_MailDocument(MWContext *pContext);
#endif

/*
 * Returns the temporary directory (in xpURL format) for all files associated with this document.
 * Contents of this directory will be deleted when this edit buffer is destroyed.
 */
char *EDT_GetDocTempDir(MWContext *pContext);

/* 
 * Return a unique filename (in xpURL format) for a potential file in the documents temp directory.  Does not actually 
 * create the file. 
 * If prefix is non-NULL, it will give the first few characters of the temp filename.
 * Only the first three characters of prefix will be used.
 * If extension is non-NULL, it will be the file extension, keep it <= 3 characters for windows,
 * default is "TMP".
 */
char *EDT_CreateDocTempFilename(MWContext *pContext,char *prefix,char *extension);


/*
 * cancel a load in progress
*/
void EDT_SaveCancel( MWContext *pContext );

/*
 * Enable and disable autosave. A value of zero disables autosave.
 * AutoSave doesn't start until the document has a file.
 */

void EDT_SetAutoSavePeriod(MWContext *pContext, int32 minutes);
int32 EDT_GetAutoSavePeriod(MWContext *pContext);

void EDT_DisplaySource( MWContext *pContext );

/*
 * Edit Navagation prototypes
*/
void EDT_PreviousChar( MWContext *context, XP_Bool bSelect );
void EDT_NextChar( MWContext *context, XP_Bool bSelect );
void EDT_BeginOfLine( MWContext *context, XP_Bool bSelect );
void EDT_EndOfLine( MWContext *context, XP_Bool bSelect );
void EDT_BeginOfDocument( MWContext *context, XP_Bool bSelect );
void EDT_EndOfDocument( MWContext *context, XP_Bool bSelect );
void EDT_Up( MWContext *context, XP_Bool bSelect );
void EDT_Down( MWContext *context, XP_Bool bSelect );
void EDT_PageUp( MWContext *context, XP_Bool bSelect );
void EDT_PageDown( MWContext *context, XP_Bool bSelect );
void EDT_PreviousWord( MWContext *context, XP_Bool bSelect );
void EDT_NextWord( MWContext *context, XP_Bool bSelect );
void EDT_NextTableCell( MWContext *context, XP_Bool bSelect );
void EDT_PreviousTableCell( MWContext *context, XP_Bool bSelect );

void EDT_WindowScrolled( MWContext *context );

/*
 * Edit functional stuff...
*/
EDT_ClipboardResult EDT_DeletePreviousChar( MWContext *context );
EDT_ClipboardResult EDT_DeleteChar( MWContext *context );
void EDT_PositionCaret( MWContext *context, int32 x, int32 y );

/* Delete current selection and move caret to the insert point 
 *  closest to supplied doc coordinates.
 *  Use when moving dragged data within the same window.
*/
void EDT_DeleteSelectionAndPositionCaret( MWContext *pContext, int32 x, int32 y );

/* Converts ("snaps") input X, Y (doc coordinates) to X, Y needed for drop caret 
 *  and calls appropriate front-end FE_Display<Text|Generic|Image>Caret to use show where
 *  a drop would occur. It does NOT change current selection or internal caret position
 * Also handles dragging table/cells and will return FALSE if mouse is
 *  not over a good drop location within a target table
 * (Always returns TRUE if not dragging table or cells since
 *   cursor is always snapped to a valid position)
 * Call during mouse moving over a window where drop can occur
*/
XP_Bool EDT_PositionDropCaret( MWContext *pContext, int32 x, int32 y );

/* Call to initialize global static data used for pasting table cells into existing table 
 * Returns TRUE if we are dragging table or table cells
*/
XP_Bool EDT_StartDragTable( MWContext *pContext,  int32 x, int32 y );

/* Clear the XP data for dragging tables and cells */
void EDT_StopDragTable( MWContext *pContext );
XP_Bool EDT_IsDraggingTable( MWContext *pContext );

void EDT_DoubleClick( MWContext *context, int32 x, int32 y );
void EDT_SelectObject( MWContext *context, int32 x, int32 y);
EDT_ClipboardResult EDT_ReturnKey( MWContext *context );

/* Do what the TAB key should do
 * Moves from cell to cell in Tables (in direction given by bForward)
 *  unless 3rd param is TRUE, which forces regular tab insert in table text
*/
EDT_ClipboardResult EDT_TabKey( MWContext *context, XP_Bool bForward, XP_Bool bForceTabChar );

void EDT_Indent( MWContext *context );
void EDT_Outdent( MWContext *context );
void EDT_RemoveList( MWContext *context );

void EDT_Reload( MWContext *pContext );

/*
 * Kludge, supports only the windows front end now.
*/
EDT_ClipboardResult EDT_KeyDown( MWContext *context, uint16 nChar, uint16 b, uint16 c );

/*
 * Insert a nonbreaking space character.  Usually wired to shift spacebar
*/
EDT_ClipboardResult EDT_InsertNonbreakingSpace( MWContext *context );

/*
 * Undo/Redo. Usage:
 * To tell if there's a command to undo:
 *  if ( EDT_GetUndoCommandID(context, 0 ) != CEDITCOMMAND_ID_NULL )
 *
 * To undo the most recent command:
 *  EDT_Undo( context );
 *
 * (Similarly for redo.)
 *
 * Use the CommandID to look up a string for saying "Undo <type of command>..."
 * on the menu.
 */

#define CEDITCOMMAND_ID_NULL 0
#define CEDITCOMMAND_ID_GENERICCOMMAND 1

void EDT_Undo( MWContext *pContext );
void EDT_Redo( MWContext *pContext );
intn EDT_GetUndoCommandID( MWContext *pContext, intn index );
intn EDT_GetRedoCommandID( MWContext *pContext, intn index );

/* Lets the front end specify the command history limit. This is used to limit the total of commands on
 * both the undo and the redo lists.
 */

intn EDT_GetCommandHistoryLimit(void);
void EDT_SetCommandHistoryLimit(intn limit); /* Must be greater than or equal to zero. Ignored if less than zero. */

/* Call from dialogs to batch changes for undo.  To use, call EDT_BeginBatchChanges,
 * make the changes you want, and then call EDT_EndBatchChanges. It's OK to
 * not make any changes -- in that case the undo history won't be affected.
 */

void EDT_BeginBatchChanges(MWContext *pContext);
void EDT_EndBatchChanges(MWContext *pContext);

/*
 * Used to control display of paragraph marks.
 */

void EDT_SetDisplayParagraphMarks(MWContext *pContext, XP_Bool display);
XP_Bool EDT_GetDisplayParagraphMarks(MWContext *pContext);

/*
 * Used to control display of tables in either wysiwyg or flat mode.
 */

void EDT_SetDisplayTables(MWContext *pContext, XP_Bool display);
XP_Bool EDT_GetDisplayTables(MWContext *pContext);

/*
 * need to figure out how to handle tags and parameters.
*/
void EDT_MorphContainer( MWContext *pContext, TagType t );
TagType EDT_GetParagraphFormatting( MWContext *pContext );
ED_Alignment EDT_GetParagraphAlign( MWContext *pContext);
void EDT_SetParagraphAlign( MWContext* pContext, ED_Alignment eAlign );

/* Set Table alignment if caret is inside table or table is selected
 * EDT_SetParagraphAlign will also to table if it is selected,
 * so use this to force table alignment even if its not selected
*/
void EDT_SetTableAlign( MWContext* pContext, ED_Alignment eAlign );


/*
 * Find out what's under the cursor
*/
ED_ElementType EDT_GetCurrentElementType( MWContext *pContext );

/*
 * Character Formatting (DEPRECIATED)
 * Routines should only be called when EDT_GetCurrentElementType() returns
 *  ED_ELEMENT_TEXT.
*/
ED_TextFormat EDT_GetCharacterFormatting( MWContext *pContext );
void EDT_FormatCharacter( MWContext *pContext, ED_TextFormat p);
/* These use the older "relative" scale of 1 to 7 */
int EDT_GetFontSize( MWContext *pContext );
void EDT_SetFontSize( MWContext *pContext, int );

/* These use the new absolute point size*/
void EDT_SetFontPointSize( MWContext *pContext, int iPoints );
int EDT_GetFontPointSize( MWContext *pContext );

/* List of Font faces - concatenated strings ending in '\0\0'
 * Note: First 2 strings are for Default Proportional and Default Fixed width,
 *       the rest are the "NS Fonts" used to set the FONT FACE tag
 * Use these to construct a list of fonts for the user
 * We cache 1 static list of faces (implemented in EDTUTIL.CPP)
*/
char *EDT_GetFontFaces(void);

/* Similar to above, but gets a list of the Tag strings
 *  (what is written in FONT FACE tag that corresponds to a NS Font face name)
*/
char *EDT_GetFontFaceTags(void);

/* Set the Font Face - encapsulates complexity of EDT_CharacterData mangling
 * If pCharacterData is suppled, then it is used to set fontface data
 *    (Note: pCharacterData is not freed).
 * If pCharacterData is NULL, then data is obtained from current selection or at caret
 * If pMWContext is supplied, then EDT_SetCharacterData() will be called
 *    with either the supplied struct or the internally-obtained data
 *
 * Supply either iFontIndex or pFontFace:
 *  iFontIndex is index within the font list returned by EDT_GetFontFaces(),
 *      (we will lookup the appropriate string to use for Font Face Tag)
 *  If pFontFace is supplied, use it exactly as is (iFontIndex is ignored)
*/
void EDT_SetFontFace(MWContext * pMWContext, EDT_CharacterData * pCharacterData,
                     int iFontIndex, char * pFontFace );

/* Get the index into the FontFace list for the current caret location or selection
 * Returns 0 (ED_FONT_VARIABLE), 1 (ED_FONT_FIXED), or 2 (ED_FONT_LOCAL)
*/
int EDT_GetFontFaceIndex(MWContext *pContext);

/* Get the current font face for current selection or insert point
 * If the current font matches an XP font 'group', 
 *   this is the platform-specific font matching the group,
 *   else it is the font face string from EDT_CharacterData or
 *   the appropriate XP string for the Variable and Fixed Width states.
 *   Use this to search your local font list in menu or listbox.
 *
     An empty string is returned when selection is mixed (> 1 face in selection)
 *   DO NOT FREE RETURN VALUE (it is a static string), but use it quickly, 
 *      because it will change as caret moves throught text
*/
char * EDT_GetFontFace(MWContext *pContext);

/* Find the pFontFace within each set of comma-delimeted FontFace groups
 * Return pointer to entire group if pFontFace matches any single font in the group
 * If not found, return pFontFace, thus the
 * result can be passed directly as the last param of EDT_SetFontFace()
 * Comparing pFontFace to result pointer can be used as test that XP font was found
 * DO NOT free the resulting string pointer
*/
char * EDT_TranslateToXPFontFace( char * pFontFace );

XP_Bool EDT_GetFontColor( MWContext *pContext, LO_Color *pDestColor );
void EDT_SetFontColor( MWContext *pContext, LO_Color *pColor);

/* Parse a font colors string in the format: "r,g,b,ColorName" where colors for r,g,b
 *  are decimal strings in range 0-255
 * Returns pointer (within supplied string) to the Color Name 
 *   and converts color strings into integers in supplied the LO_Color struct
*/
char * EDT_ParseColorString(LO_Color * pLoColor, char * pColorString);

/* Get the Netscape solid color. This replaces EDT_GetFontColorFromList */
void EDT_GetNSColor(intn iIndex, LO_Color * pLoColor);

/* Scan our list of colors and return index of matching color
 *   or -1 if no match found. We NEVER return 0 (default color)
*/
int EDT_GetMatchingFontColorIndex(LO_Color * pLOColor);

/*
 * Get and set character formatting.
*/

/*
 * EDT_GetCharacterData
 *      returns the current character formatting for the current selection
 *      or insert point.  Should only be called when GetCurrentElementType is
 *      ED_ELEMENT_TEXT or ED_ELEMENT_SELECTION.
 *
 *  returns and EDT_CharacterData structure.  Caller is responsible for
 *      destroying the structure by calling EDT_FreeCharacterData().
 *
 *  CharacterData contain on return
 *      pRet->mask      // the that were deterministic (all in insertpoint case
 *                      //      and those that were consistant across the selection
 *      pRet->values    // if in the mask, 0 means clear across the selection
 *                      //                 1 means set across the selection
*/
EDT_CharacterData* EDT_GetCharacterData( MWContext *pContext );

/*
 * EDT_SetCharacterData
 *      sets the character charistics for the current insert point.
 *
 *  the mask contains the bits to set
 *      values contains the values to set them to
 *
 *  for example if you wanted to just change the font size to 10 and leave
 *      the rest alone:
 *
 *      pData = EDT_NewCharacterData();
 *      pData->mask = TF_FONT_SIZE
 *      pData->value = TF_FONT_SIZE
 *      pData->iSize = 5;
 *      EDT_SetCharacterData( context ,pData );
 *      EDT_FreeCharacterData(pData);
 *
*/
void EDT_SetCharacterData( MWContext *pContext, EDT_CharacterData *pData );
EDT_CharacterData* EDT_NewCharacterData(void);
void EDT_FreeCharacterData( EDT_CharacterData *pData );

/*
 * Set character data for a given offset in the buffer.
*/
void EDT_SetCharacterDataAtOffset( MWContext *pContext, EDT_CharacterData *pData,
        ED_BufferOffset iBufOffset, int32 iLen );

/*
 * Returns colors of all the different fonts.
 * Must call XP_FREE( pDest ) after use.
*/
int EDT_GetExtraColors( MWContext *pContext, LO_Color **pDest );

/*
 * Selection
*/
void EDT_StartSelection(MWContext *context, int32 x, int32 y);
void EDT_ExtendSelection(MWContext *context, int32 x, int32 y);
void EDT_EndSelection(MWContext *context, int32 x, int32 y);    /* cm */
void EDT_ClearSelection(MWContext *context);

void EDT_SelectAll(MWContext *context);
void EDT_SelectTable(MWContext *context);
void EDT_SelectTableCell(MWContext *context);

XP_Block EDT_GetSelectionText(MWContext *context);
XP_Bool EDT_IsSelected( MWContext *pContext );
XP_Bool EDT_SelectionContainsLink( MWContext *pContext );

/* Format all text contents into tab-delimited cells,
**  with CR at end of each row.
** Use result to paste into Excell spreadsheets
*/
char *EDT_GetTabDelimitedTextFromSelectedCells( MWContext *pContext );

/* Convert Selected text into a table (put each paragraph in separate cell)
 * Number of rows is automatic - creates as many as needed
*/
void EDT_ConvertTextToTable(MWContext *pMWContext, intn iColumns);

/* Convert the table into text - unravel existing paragraphs in cells */
void EDT_ConvertTableToText(MWContext *pMWContext);

/* Save the character and paragraph style of selection or at caret */
void EDT_CopyStyle(MWContext *pMWContext);

/* This is TRUE after EDT_CopyStyle is called, until the next left mouse up call 
 *  or user cancels with ESC key, or ??? (any suggestions?)
*/
XP_Bool EDT_CanPasteStyle(MWContext *pMWContext);

/* Apply the style to selection or at caret. Use bApplyStyle = FALSE to cancel */
void EDT_PasteStyle(MWContext *pMWContext, XP_Bool bApplyStyle);


XP_Bool EDT_DirtyFlag( MWContext *pContext );
void EDT_SetDirtyFlag( MWContext *pContext, XP_Bool bValue );

/*
 * Clipboard stuff
*/

#define EDT_COP_OK 0
#define EDT_COP_DOCUMENT_BUSY 1
#define EDT_COP_SELECTION_EMPTY 2
#define EDT_COP_SELECTION_CROSSES_TABLE_DATA_CELL 3

EDT_ClipboardResult EDT_InsertText( MWContext *pContext, char *pText );
EDT_ClipboardResult EDT_PasteText( MWContext *pContext, char *pText );
EDT_ClipboardResult EDT_PasteHTML( MWContext *pContext, char *pHtml );

/** API for pasting quoted text into the editor.
 * Call EDT_PasteQuoteBegin, then call EDT_PasteQuoteINTL zero or more times,
 * then call EDT_PasteQuoteEnd.
 * If EDT_PasteQuoteBegin returns EDT_COP_OK then you must eventually call EDT_PasteQuoteEnd.
 * If EDT_PasteQuoteBegin returns an error condition, then you must not call EDT_PasteQuoteEnd.
 *
 * The "quote" is something of a misnomer. The text isn't quoted. If you want it to be
 * quoted, then you have to quote it yourself, before calling EDT_PasteQuoteINTL.
 */

EDT_ClipboardResult EDT_PasteQuoteBegin( MWContext *pContext, XP_Bool isHTML );

/* Pass in the csid of the pasted text. The editor will transcode
 * the pasted text to the document's wincsid if nescessary. */
EDT_ClipboardResult EDT_PasteQuoteINTL( MWContext *pContext, char *pText, int16 csid );
/* The non-INTL version is only around for backwards compatability. */
EDT_ClipboardResult EDT_PasteQuote( MWContext *pContext, char *pText );

EDT_ClipboardResult EDT_PasteQuoteEnd(MWContext *pContext);

/*
 * Can paste a singe or multiple HREFs.  Pointer to array of HREF pointers and
 *  title pointers.
 * Title pointers can be NULL
*/
EDT_ClipboardResult EDT_PasteHREF( MWContext *pContext, char **ppHref, char **ppTitle, int iCount);

/* Depreciated */
void EDT_DropHREF( MWContext *pContext, char *pHref, char* pTitle, int32 x,
            int32 y );

/* Depreciated */
XP_Bool EDT_CanDropHREF( MWContext *pContext, int32 x, int32 y );


/*
 * The text buffer should also be huge, it isn't
*/
EDT_ClipboardResult EDT_CopySelection( MWContext *pContext, char** ppText,
            int32* pTextLen, XP_HUGE_CHAR_PTR* ppHtml, int32* pHtmlLen);

EDT_ClipboardResult EDT_CutSelection( MWContext *pContext,
        char** ppText, int32* pTextLen,
        XP_HUGE_CHAR_PTR* ppHtml, int32* pHtmlLen);

/*
 * Use to enable or disable the UI for cut/copy/paste.
 * If bStrictChecking is true, will check for all conditions,
 * Otherwise, will just check if the selection is empty.
 */

EDT_ClipboardResult EDT_CanCut(MWContext *pContext, XP_Bool bStrictChecking);
EDT_ClipboardResult EDT_CanCopy(MWContext *pContext, XP_Bool bStrictChecking);
EDT_ClipboardResult EDT_CanPaste(MWContext *pContext, XP_Bool bStrictChecking);

/*
 * returns true if we can set an HREF
 */
XP_Bool EDT_CanSetHREF( MWContext *pContext );

/*
 * Get the current link under the cursor or inside the current selection.
 */
char *EDT_GetHREF( MWContext *pContext );

/*
 * Get the anchor text of the current link under the cursor
 */
char *EDT_GetHREFText( MWContext *pContext );

/*
 * Use HREF structure
*/
EDT_HREFData *EDT_GetHREFData( MWContext *pContext );
void EDT_SetHREFData( MWContext *pContext, EDT_HREFData *pData );
EDT_HREFData *EDT_NewHREFData( void );
EDT_HREFData *EDT_DupHREFData( EDT_HREFData *pData );
void EDT_FreeHREFData(  EDT_HREFData *pData );

/*
 * This routine can only be called when 'EDT_CanSetHREF()'
 */
void EDT_SetHREF(MWContext *pContext, char *pHREF );

/*
 * Refresh the entire document.
*/
void EDT_RefreshLayout( MWContext *pContext );

/*
 * Getting and setting properties of images.  After Getting an image's properties
 *  the EDT_ImageData must be freed with EDT_FreeImageData().  Routines should
 *  only be called when EDT_GetCurrentElementType() returns ED_ELEMENT_IMAGE.
*/
EDT_ImageData *EDT_GetImageData( MWContext *pContext );
void EDT_SetImageData( MWContext *pContext, EDT_ImageData *pData, XP_Bool bKeepImagesWithDoc );
EDT_ImageData *EDT_NewImageData(void);
void EDT_FreeImageData(  EDT_ImageData *pData );
void EDT_InsertImage( MWContext *pContext, EDT_ImageData *pData, XP_Bool bKeepImageWithdoc );
/* Value to display if EDT_ImageData::iBorder == -1. Different depending on whether the
   image has a link or not. */
int32 EDT_GetDefaultBorderWidth( MWContext *pContext );

EDT_HorizRuleData *EDT_GetHorizRuleData( MWContext *pContext );
void EDT_SetHorizRuleData( MWContext *pContext, EDT_HorizRuleData *pData );
EDT_HorizRuleData *EDT_NewHorizRuleData( void );
void EDT_FreeHorizRuleData(  EDT_HorizRuleData *pData );
void EDT_InsertHorizRule( MWContext *pContext, EDT_HorizRuleData *pData );

void EDT_ToggleList( MWContext *pContext, intn iTagType); /* All-in-one-call to create numbered or unnumbered lists. */
XP_Bool EDT_GetToggleListState( MWContext *pContext, intn iTagType);

EDT_ListData *EDT_GetListData( MWContext *pContext );
void EDT_SetListData( MWContext *pContext, EDT_ListData *pData );
void EDT_FreeListData(  EDT_ListData *pData );

void EDT_InsertBreak( MWContext *pContext, ED_BreakType eBreak );

XP_Bool EDT_IsInsertPointInTable(MWContext *pContext );
XP_Bool EDT_IsInsertPointInNestedTable(MWContext *pContext );
EDT_TableData* EDT_GetTableData( MWContext *pContext );

/*  If bCellParent = TRUE:
 *     Get width and height table enclosing current cell, 
 *     (minus border and cell spacing)
 * If bCell = FALSE: 
 *     Get the width/height of current page or 
 *     size the parent cell if insert point is in a nested table,
 */
void EDT_GetTableParentSize( MWContext *pContext, XP_Bool bCell, int32 *pWidth, int32 *pHeight );
void EDT_SetTableData( MWContext *pContext, EDT_TableData *pData );
EDT_TableData* EDT_NewTableData( void );
void EDT_FreeTableData(  EDT_TableData *pData );
void EDT_InsertTable( MWContext *pContext, EDT_TableData *pData);
void EDT_DeleteTable( MWContext *pContext);

XP_Bool EDT_IsInsertPointInTableCaption(MWContext *pContext );
EDT_TableCaptionData* EDT_GetTableCaptionData( MWContext *pContext );
void EDT_SetTableCaptionData( MWContext *pContext, EDT_TableCaptionData *pData );
EDT_TableCaptionData* EDT_NewTableCaptionData( void );
void EDT_FreeTableCaptionData(  EDT_TableCaptionData *pData );
void EDT_InsertTableCaption( MWContext *pContext, EDT_TableCaptionData *pData);
void EDT_DeleteTableCaption( MWContext *pContext);

XP_Bool EDT_IsInsertPointInTableRow(MWContext *pContext );
EDT_TableRowData* EDT_GetTableRowData( MWContext *pContext );
void EDT_SetTableRowData( MWContext *pContext, EDT_TableRowData *pData );
EDT_TableRowData* EDT_NewTableRowData(void);
void EDT_FreeTableRowData(  EDT_TableRowData *pData );
void EDT_InsertTableRows( MWContext *pContext, EDT_TableRowData *pData, XP_Bool bAfterCurrentRow, intn number);
void EDT_DeleteTableRows( MWContext *pContext, intn number);

XP_Bool EDT_IsInsertPointInTableCell(MWContext *pContext );
EDT_TableCellData* EDT_GetTableCellData( MWContext *pContext );

/* Change the table selection and/or move to previous/next cell, row, or column
 *   depending on iMoveType. 
 * To just change the selection, set the iHitType to new type 
 *  (ED_HIT_SEL_ROW,  ED_HIT_SEL_COL, or ED_HIT_SEL_CELL) and set iMoveType to ED_MOVE_NONE
 * To change to another row, col, or cell, set iHitType to same as that in pData or ED_HIT_NONE,
 *  and set iMoveType to ED_MOVE_PREV or ED_MOVE_NEXT
 * If pData is not NULL, it is filled with data for the selected cells
 * This will wrap around appropriately when end of selection or table is reached
*/
void EDT_ChangeTableSelection(MWContext *pContext, ED_HitType iHitType, ED_MoveSelType iMoveType, EDT_TableCellData *pData);

void EDT_SetTableCellData( MWContext *pContext, EDT_TableCellData *pData );
EDT_TableCellData* EDT_NewTableCellData( void );

void EDT_FreeTableCellData(  EDT_TableCellData *pData );
void EDT_InsertTableCells( MWContext *pContext, EDT_TableCellData *pData, XP_Bool bAfterCurrentCell, intn number);
void EDT_DeleteTableCells( MWContext *pContext, intn number);
void EDT_InsertTableColumns( MWContext *pContext, EDT_TableCellData *pData, XP_Bool bAfterCurrentCell, intn number);
void EDT_DeleteTableColumns( MWContext *pContext, intn number);

XP_Bool EDT_IsInsertPointInLayer(MWContext *pContext );
EDT_LayerData* EDT_GetLayerData( MWContext *pContext );
void EDT_SetLayerData( MWContext *pContext, EDT_LayerData *pData );
EDT_LayerData* EDT_NewLayerData(void);
void EDT_FreeLayerData(  EDT_LayerData *pData );
void EDT_InsertLayer( MWContext *pContext, EDT_LayerData *pData);
void EDT_DeleteLayer( MWContext *pContext);

EDT_PageData *EDT_GetPageData( MWContext *pContext );
EDT_PageData *EDT_NewPageData(void);
void EDT_SetPageData( MWContext *pContext, EDT_PageData *pData );
void EDT_FreePageData(  EDT_PageData *pData );

/* Use to enable/disable Merge Cells feature. 
 * Selected cells can be merged only if in a continuous set
 * within the same row or column.
 * A single cell can be merged with cell to the right
 * Returns 0 if neither conditions holds
*/
ED_MergeType EDT_GetMergeTableCellsType( MWContext *pContext );

/* Use to enable/disable Split Cell feature. 
 * Current cell (containing caret) can be split 
 * only if it has COLSPAN or ROWSPAN
*/
XP_Bool EDT_CanSplitTableCell( MWContext *pContext );

/* Set appropriate COLSPAN or ROWSPAN and move all
 * cell contents into first cell of set
 */
void EDT_MergeTableCells( MWContext *pContext );

/* Separate paragraphs into sepaparate cells,
 * removing COLSPAN or ROWSPAN
*/
void EDT_SplitTableCell( MWContext *pContext );

/* Take a single, selected image and turn it into the
 *  background image of page, deleting it from the page
 */
void EDT_SetImageAsBackground( MWContext *pContext );

/* Get and Set MetaData.
 *  Get the count of meta data objects.
 *  enumerate through them (0 based).
 *  If the name changes, a new meta data is created or may overwrite a different
 *     name value pair.
 */
intn EDT_MetaDataCount( MWContext *pContext );
EDT_MetaData* EDT_GetMetaData( MWContext *pContext, intn n );
EDT_MetaData* EDT_NewMetaData( void );
void EDT_SetMetaData( MWContext *pContext, EDT_MetaData *pMetaData );
void EDT_DeleteMetaData( MWContext *pContext, EDT_MetaData *pMetaData );
void EDT_FreeMetaData( EDT_MetaData *pMetaData );

/*******************************************************
 * CLM: JAVA, PLUG-IN interface
*/

EDT_JavaData *EDT_GetJavaData( MWContext *pContext );
EDT_JavaData *EDT_NewJavaData(void);
void EDT_SetJavaData( MWContext *pContext, EDT_JavaData *pData );
void EDT_FreeJavaData(  EDT_JavaData *pData );

EDT_PlugInData *EDT_GetPlugInData( MWContext *pContext );
EDT_PlugInData *EDT_NewPlugInData(void);
void EDT_SetPlugInData( MWContext *pContext, EDT_PlugInData *pData );
void EDT_FreePlugInData(  EDT_PlugInData *pData );

/* Parameters: NAME=VALUE pairs used by both Java and PlugIns */
EDT_ParamData* EDT_GetParamData( MWContext *pContext, intn n );
EDT_ParamData* EDT_NewParamData(void);
void EDT_SetParamData( MWContext *pContext, EDT_ParamData *pParamData );
void EDT_DeleteParamData( MWContext *pContext, EDT_ParamData *pParamData );
void EDT_FreeParamData( EDT_ParamData *pParamData );

/*
 * Get and Set Named Anchors (Targets)
*/
char *EDT_GetTargetData( MWContext *pContext );
void EDT_SetTargetData( MWContext *pContext, char *pTargetName );
void EDT_InsertTarget( MWContext *pContext, char* pTargetName );
char *EDT_GetAllDocumentTargets( MWContext *pContext );

/*CLM: Check current file-update time and
 *     return TRUE if it is different
 *     Save the newly-found time in edit buffer class
*/
XP_Bool EDT_IsFileModified( MWContext* pContext );

/* CLM: Read a file and build a targets list just like the current doc list */
char *EDT_GetAllDocumentTargetsInFile( MWContext *pContext, char *pHref);

/*
 * Returns a list of all local documents associated with
 *  the current buffer.
*/
char* EDT_GetAllDocumentFiles( MWContext *pContext );
/*
 * Also sets ppSelected to be a list of whether each local document should be 
 * saved/published/sent by default.  Must pass in the preference for whether images
 * are sent along with the document. 
 */
char* EDT_GetAllDocumentFilesSelected( MWContext *pContext, XP_Bool **ppSelected, 
      XP_Bool bKeepImagesWithDoc );

/*
 * Get and Set UnknownTags
*/
char *EDT_GetUnknownTagData( MWContext *pContext );
void EDT_SetUnknownTagData( MWContext *pContext, char *pUnknownTagData );
void EDT_InsertUnknownTag( MWContext *pContext, char* pUnknownTagData );
/* CLM: Validate: check for matching quotes, "<" and ">" if bNoBrackets is FALSE
 *      Skip (and strip out) <> if bNoBrackets is TRUE,
 *      used for Attributes-only, such as MOCHA string in HREF
*/
ED_TagValidateResult EDT_ValidateTag( char *pData, XP_Bool bNoBrackets );

/*
 * Called by the front end when the user presses the cancel button on the
 *  Modal dialog
*/
void EDT_ImageLoadCancel( MWContext *pContext );

/*****************************************************************************
 * Property Dialogs
 *****************************************************************************/

#if 0
/*
 * String allocation functions for parameters passed to EDT_Property functions
*/
char *EDT_StringDup(char *pDupString);
void EDT_StringFree(char* pString);


void EDT_GetPageProperties( ED_PageProperties *pProps );
void EDT_SetPageProperties( ED_PageProperties *pProps );

void EDT_GetParagraphProperties( ED_ParagraphProperties *pProps );
void EDT_SetParagraphProperties( ED_ParagraphProperties *pProps );

#endif

/*****************************************************************************
 * Utility stuff.
 *****************************************************************************/

/* for debug purposes. */
char *EDT_TagString(int32 tagType);

#ifdef DEBUG
void EDT_VerifyLayoutElement( MWContext *pContext, LO_Element *pLoElement,
        XP_Bool bPrint );
#endif

/* Cross platform macros */
/* we may changed how we define editor status */
#define EDT_IS_NEW_DOCUMENT(context) (context != NULL && context->is_editor && context->is_new_document)
#define EDT_NEW_DOCUMENT(context,b)  if(context != NULL) context->is_new_document=(context->is_editor&&b)

/* Helper to gray UI items not allowed when inside Java Script
 * Note that the return value is TRUE if mixed selection,
 *   allowing the non-script text to be changed.
 * Current Font Size, Color, and Character attributes will suppress
 *   setting other attributes, so it is OK to call these when mixed
*/
XP_Bool EDT_IsJavaScript(MWContext *pContext);

/* Helper to use for enabling Character properties
 * (Bold, Italic, etc., but DONT use for clearing (TF_NONE)
 *  or setting Java Script (Server or Client)
 * Tests for:
 *   1. Good edit buffer and not blocked because of some action,
 *   2. Caret or selection is NOT entirely within Java Script,
 *   3. Caret or selection has some text or is mixed selection
 *      (thus FALSE if single non-text object is selected)
*/
XP_Bool EDT_CanSetCharacterAttribute(MWContext *pContext);

/* Replace the current selection with supplied text */
/* if bReplaceAll is true then pTextToLookFor and the 3 subsequent Boolean need to be set */
void EDT_ReplaceText(MWContext *pContext, char * pReplaceText, XP_Bool bReplaceAll,
					char *pTextToLookFor, XP_Bool bCaseless, XP_Bool bBackward, XP_Bool bDoWrap);

#ifdef FIND_REPLACE
/* Currently (12/3/97) not used */
XP_Bool EDT_FindAndReplace(MWContext *pContext, EDT_FindAndReplaceData *pData );

#endif
/* Dynamic Object Sizing
 * Note:xVal and yVal for all functions are in Document coordinates
 * After calling EDT_StartSizing, either EDT_EndSizing or EDT_CancelSizing
 *  must be called.
*/

/* How close cursor must be to border to start sizing  */
#define ED_SIZING_BORDER 6

/* Get the sizing, selection, or add row/column type define (0 if not at a hit region)
 *  where xVal, yVal is Cursor position we are over 
 * This ignores anything inside of tables or cells, so don't call it
 *   if you want to size an image contained within a cell (use EDT_CanSizeObject instead)
 * ppElement is optional: If supplied, the relevant table or cell element is returned
 *   (a cell ptr is returned for the row/col operations - use this to get col. or row members)
 * bModifierKeyPressed is applicable if selecting table (upper left corner)
 *    or extending selection to multiple cells
 * If TRUE (Ctrl key is pressed in Windows), returns 
 *    ED_HIT_SEL_ALL_CELLS instead of ED_HIT_SEL_TABLE
*/
ED_HitType EDT_GetTableHitRegion(MWContext *pContext, int32 xVal, int32 yVal, 
                                 LO_Element **ppElement, XP_Bool bModifierKeyPressed);

/* Wrapper for ease of use -- gets last-selected table object's hit type and element */
ED_HitType EDT_GetSelectedTableElement(MWContext *pContext, LO_Element **ppElement);

/* Select a Table, Row, Column, or Cell 
 * iHitType is a define returned by EDT_GetTableHitRegion()
 * Returns TRUE if selection was done - it will fail if
 *   missing pointers or iHitType doesn't match pLoElement type or 
 *   is not an allowable type for doing selection.
 * Selection rules:
 *   Table:  Any corner is OK (ED_HIT_SEL_TABLE, ED_HIT_SIZE_TABLE, ED_HIT_ADD_ROWS, or ED_HIT_ADD_COLS)
 *   Column: Top of table above desired column (ED_HIT_SEL_COL)
 *   Row:    Left edge of table next to desired row (ED_HIT_SEL_ROW)
 *   Cell:   Left or top edge of cell, including upper right corner (ED_HIT_SEL_CELL)
 *  
 *   x and y values are used only when selecting rows or columns
 *
 *   if pLoElement == NULL, we find it from current edit element
 *     (use it this way to select items from a menu when caret is inside a table)
 *
 *   If bAppendSelection is TRUE, new cells will be added to current selection
 *    (ignored if selecting a table - all cell selection is cleared for that)
 *
 *   If bExtendSelection is TRUE (Shift key is pressed),
 *    select all cells withing smallest rect from first-selected to supplied cell
*/
XP_Bool EDT_SelectTableElement(MWContext *pMWContext, int32 x, int32 y,
                               LO_Element *pLoElement, 
                               ED_HitType iHitType, 
                               XP_Bool bModifierKeyPressed, XP_Bool bExtendSelection);

/* Called on mouse-move message after selection was started
 * on a row, column, or cell element
 * Returns the hit type: ED_HIT_SEL_ROW, ED_HIT_SEL_COL, ED_HIT_SEL_CELL, or ED_HIT_NONE
 *   reflecting what type of block we are extended,
 *     or ED_HIT_NONE if mouse is outside of the table
 *   Use this to set the type of cursor by the FEs
*/
ED_HitType EDT_ExtendTableCellSelection(MWContext *pMWContext,  int32 x, int32 y);

/* Clear the all selected table or cells */
void EDT_ClearTableAndCellSelection(MWContext *pMWContext);

XP_Bool EDT_IsTableSelected(MWContext *pMWContext);
int     EDT_GetSelectedCellCount(MWContext *pMWContext);

/* Clear any existing cells selected if current edit element is not inside selection
 * Call before poping up context menu inside table
*/
void EDT_ClearCellSelectionIfNotInside(MWContext *pMWContext);

/* Call just before bringing up the Table Properties dialog 
 * Supply cell data struct so the iSelectionType and iSelectedCount are filled-in
*/
void EDT_StartSpecialCellSelection(MWContext *pMWContext, EDT_TableCellData *pCellData);
/* Call after closing the Table Properties dialog */
void EDT_ClearSpecialCellSelection(MWContext *pMWContext);

/* Called from lo_EndTable (laytable.c) to build list of tables being redrawn */
void EDT_AddToRelayoutTables(MWContext *pMWContext, LO_TableStruct *pLoTable );

/* Called after every table layout so editor size data is accurate */
void EDT_FixupTableData(MWContext *pMWContext);

/* Get the sizing type define (0 if not at a sizing location)
 *  where xVal, yVal is Cursor position and pElement is the element
 *  we are over -- if NULL, we will find it (param used for efficiency)
*/
ED_SizeStyle EDT_CanSizeObject(MWContext *pContext, LO_Element *pLoElement, int32 xVal, int32 yVal);

/* Return TRUE if we are currently sizing
*/
XP_Bool EDT_IsSizing(MWContext *pContext);

/*  If bLockAspect is TRUE, constrain rect to keep original aspect ratio
 *  This returns the rect to draw sizing feedback in View's coordinate system
 *  Returns sizing style if sizing was started OK
*/
ED_SizeStyle EDT_StartSizing(MWContext *pContext, LO_Element *pLoElement, int32 xVal, int32 yVal,
                             XP_Bool bLockAspect, XP_Rect *pRect);

/* Get the rect in View coordinates, so you can use it
 * directly for drawing "selection feedback"
 * Returns TRUE if this rect is different from the last
 *  one calculated by EDT_StartSizing or EDT_GetSizingRect,
 *  so you need to do sizing feedback only if we return TRUE;
*/
XP_Bool EDT_GetSizingRect(MWContext *pContext, int32 xVal, int32 yVal,
                          XP_Bool bLockAspect, XP_Rect *pRect);

/* Uses rect (xVal and yVal) from last mouse move to
 * get new width and height and change current object size
*/
void EDT_EndSizing(MWContext *pContext);

/* Call this to abort sizing
*/
void EDT_CancelSizing(MWContext *pContext);

/*
 * Editor plugin interface
 * The strings returned by this interface are from the Java heap. This means that
 * they will automaticly be garbage collected once there is no thread that
 * is refering to them. So XP_STRDUP them if you want to hold onto them.
 */
void EDT_RegisterPlugin(char* csFileSpec); /* Called by front end to register a plugin file. */

/* The following calls are cheap enough to be called as often as you like. */
int32 EDT_NumberOfPluginCategories(void);
int32 EDT_NumberOfPlugins(int32 category);

/* The result strings are garbage collected by the Java runtime. They are in UTF8 encoding. */
char* EDT_GetPluginCategoryName(int32 category);
char* EDT_GetPluginName(int32 category, int32 index);
char* EDT_GetPluginMenuHelp(int32 category, int32 index);

/*
 * Perform a Composer Plugin by category and index. Returns TRUE if the Plug-in was launched
 * successfully. Returns FALSE if the Plug-in failed to run. If this method returns true, then
 * the actual Plug-in runs asynchronously. When the plugin completes, doneFunction is called
 * in the main UI thread. Alternatively, you can poll EDT_IsPluginActive to find out when the
 * Plug-in is done.
 * If you don't want to be called back when the plugin completes, pass NULL for doneFunction.
 * "hook" is a variable for you to use any way you want. It is passed back to your doneFunction.
 */

XP_Bool EDT_PerformPlugin(MWContext *pContext, int32 category, int32 index, EDT_ImageEncoderCallbackFn doneFunction, void* hook);

/*
 * Just like EDT_PerformPlugin, except that the Plugin is identified by className rather than by
 * category and index. The className is a fully qualified Java class name in UTF8 encoding.
 * doneFunction can be NULL, in which case it won't be called.
 * Under some circumstances, the doneFunction may be called immediately.
 */

XP_Bool EDT_PerformPluginByClassName(MWContext *pContext, char* className, EDT_ImageEncoderCallbackFn doneFunction, void* hook);

/*
 * Just like EDT_PerformPlugin, except that all event handlers are called for
 * a particular event.
 * doneFunction can be NULL, in which case it won't be called.
 * Under some circumstances, the doneFunction may be called immediately.
 * pDocURL can be null, or empty
 */

void EDT_PerformEvent(MWContext *pContext, char* pEvent, char* pDocURL, XP_Bool bCanChangeDocument, XP_Bool bCanCancel,
                      EDT_ImageEncoderCallbackFn doneFunction, void* hook);

XP_Bool EDT_IsPluginActive(MWContext* pContext);
void EDT_StopPlugin(MWContext* pContext);

/* Used internally by the Composer Plug-in implementation.
 */
void EDT_ComposerPluginCallback(MWContext* pContext, int32 action,
    struct java_lang_Object* pArg);

/* Image encoder interface.
 * The strings returned by this interface are from the Java heap. This means that
 * they will automaticly be garbage collected once there is no thread that
 * is refering to them. So XP_STRDUP them if you want to hold onto them.
 */
int32 EDT_NumberOfEncoders(void);
char* EDT_GetEncoderName(int32 index); /* The human name of the encoding. e.g. JPEG */
char* EDT_GetEncoderFileExtension(int32 index); /* The file extension, without the period. e.g. jpg */
char* EDT_GetEncoderFileType(int32 index); /* The Macintosh FileType field. An array of 4 characters. */
char* EDT_GetEncoderMenuHelp(int32 index);  /* A sentence describing the encoding. e.g. Joint Picture Encoding Group */
XP_Bool EDT_GetEncoderNeedsQuantizedSource(int32 index); /* TRUE if the encoder needs 256 distinct colors or less. */
/* Returns FALSE if there was a problem, or TRUE if the encoder was started successfully.
 * The pixels are copied -- you can dispose of the pixels as soon as EDT_StartEncoder returns.
 *
 */
XP_Bool EDT_StartEncoder(MWContext* pContext, int32 index, int32 width, int32 height, char** pixels,
    char* csFileName, EDT_ImageEncoderCallbackFn doneFunction, void* hook);

/* Will stop the encoder, and will cause the doneFunction to be called. */
void EDT_StopEncoder(MWContext* pContext);

/* Save and restore the cursor position or selection
*/
ED_BufferOffset EDT_GetInsertPointOffset( MWContext *pContext );
void EDT_SetInsertPointToOffset( MWContext *pContext, ED_BufferOffset i,
            int32 iLen );

/* Given a character offset, return the layout element and offset for that character
 */
 
void EDT_OffsetToLayoutElement( MWContext *pContext, ED_BufferOffset i,
		LO_Element * *element, int32 *caretPos);
ED_BufferOffset EDT_LayoutElementToOffset( MWContext *pContext,
		LO_Element *element, int32 caretPos);

/* Return the layout textblock for a text element
 */
LO_TextBlock* EDT_GetTextBlock(MWContext *pContext, LO_Element *le);

/* Spell check API
*/
XP_Bool EDT_SelectFirstMisspelledWord( MWContext *pContext );
XP_Bool EDT_SelectNextMisspelledWord( MWContext *pContext );
XP_HUGE_CHAR_PTR EDT_GetPositionalText( MWContext* pContext );
void EDT_SetRefresh( MWContext *pContext, XP_Bool bRefreshOn );
void EDT_ReplaceMisspelledWord( MWContext *pContext, char* pOldWord, 
        char*pNewWord, XP_Bool bAll );
/* Ignore a misspelled word. 
 * If pWord != NULL and bAll == TRUE. ignore all instances of the specified word
 * If pWord == NULL, ignore all misspelled words 
 */
void EDT_IgnoreMisspelledWord( MWContext *pContext, char* pWord, 
        XP_Bool bAll );

/* The selection is 1/2 open -- if the start and end are the same value, then
 * the selection is an insertion point.
 */
void EDT_GetSelectionOffsets(MWContext *pContext, ED_BufferOffset* pStart, ED_BufferOffset* pEnd);

/*
 * Set the document encoding. Returns TRUE if the encoding
 * was changed, or FALSE if it was not. (An encoding change
 * may require a save of the current document. The user may
 * cancel the save.)
 *
 */

XP_Bool EDT_SetEncoding(MWContext* pContext, int16 csid);

/* Take the document for current contextand change appropriate
 *   params to make it look like a "Untitled" new document
 * Allows loading any document as a "Template"
 */
void EDT_ConvertCurrentDocToNewDoc(MWContext * pMWContext);

/* Assemble the filename part from supplied URL to
 * the base URL, replacing any filename in the latter
 * pURL can be most anything, full URL or local filename
 * If NULL, pBaseURL is returned without its filename
 *   (Use this to strip off filename from a base URL);
 * Caller must free returned string
 */
char * EDT_ReplaceFilename(char * pBaseURL, char * pURL, XP_Bool bMustHaveExtension);

/* Simply returns the filename part of a URL or local filespec
 * Stuff starting with "#" or "?" is ommited
 * Used with EDT_ReplaceFilenam(pURL, NULL) parse out a URL
 *   into separate "directory" and "filename" parts
 * If bMustHaveExtension is TRUE, then name after last "/" or "\"
 *   must have a period indicating it has an extension
 * Note: Assumes bMustHaveExtension = TRUE when getting filename
 * Caller must free returned string
 */
char * EDT_GetFilename(char * pURL, XP_Bool bMustHaveExtension);


/* Return a destination URL for publishing:
 * If last-failed-published URL (saved globally) is same as URL about to be published,
 *    then always returns the location, Username, and password last attempted,
 *    obtained from prefs: "editor.publish_last_loc" and "editor.publish_last_password"
 * else: 
 * If current page URL is
 * 1. Remote URL: Return current page name as is, with anything after filename stripped
 * 2. Local File: Return last-good location (from pref: "editor.publish_history_0")
 *                or default (if no last-location) from "editor.publish_location"
 *                *ppFilename is set to filename portion of current page
 *                and UserName is parsed out the pref string and returned in ppUsername
 * 3. New (unsaved) page: Return pref URL as above, but *ppFilename is set to NULL
 *
 * Note: Assumes bMustHaveExtension = TRUE when getting filename
 * If available, the last-used password is also supplied
 * Caller must free returned strings
 */
char * EDT_GetDefaultPublishURL(MWContext * pMWContext, char **ppFilename, char **ppUserName, char **ppPassword);

/* The maximum number of history items shown */
#define MAX_EDIT_HISTORY_LOCATIONS 10 

/*
 *    Get the location, username, and password (plain text) for the Nth
 *    publish history location. Caller must XP_FREE the returned location,
 *    username, passwd.
 */
XP_Bool EDT_GetPublishingHistory(unsigned n, char** loc, char** u, char** p);

/*
 *    Copy the last publish location to the 0th history, move everyone up one.
 */
void   EDT_SyncPublishingHistory(void);

/* Get URL and TITLE from the recently-edited history list kept in preferences
 * Similar to Publish history list, except caller should NOT XP_FREE the returned strings
 *   because they are cached in local string arrays for quicker access
 *
 * Supply MWContext pointer to check if the current document is
 *   in the list before or at the requested URL.
 *   If it is, the next item is returned instead, thus the current doc
 *     never shows up in the list
 *     Because of this behavior, this function should be used with MWContext*
 *        both to build the menu items and also to get the URL when the menu item is executed
 *
 * Note: If there is no Title, *pTitle is set to NULL
 * (Front ends should use a compressed version of pUrl instead)
*/
XP_Bool EDT_GetEditHistory(MWContext * pMWContext, unsigned n, char** pUrl, char** pTitle);

/* Call this after opening a file or URL to make it
 *   the most-recently-edited document in the URL history in preferences
 */
void EDT_SyncEditHistory(MWContext * pMWContext);

/* Construct a page title from supplied filename,
 * Extracts the filename part WITHOUT extension
 * Stuff starting with "#" or "?" is ommited
 * Caller must free returned string
 */
char * EDT_GetPageTitleFromFilename(char * pFilename);

/*
 * Call EDT_PreOpen when the user requests that you open an existing HTML document.
 * You will get called back with status that indicates whether or not to open the
 * document. You can pass anything you want as the hook argument -- it will be
 * passed back to you.
 * 
 * The MWContext passed to EDT_PreOpen is only used for reporting errors.
 *
 * Use the pURL that's passed back to you as the URL to open -- it may be different
 * than the URL you passed in.
 *
 * Note: Unlike other similar calls, in EDT_PreOpen the doneFunction will always be
 * called. Under certain circumstances, however, the doneFunction will be called
 * immediately, before EDT_PreOpen even returns.
 *
 */

typedef void (*EDT_PreOpenCallbackFn)(XP_Bool bUserCancled, char* pURL, void* hook);
void EDT_PreOpen(MWContext *pErrorContext,char* pURL, EDT_PreOpenCallbackFn doneFunction, void* hook);

/*
 * Call EDT_PreClose when an editor context is about to be closed.  Don't actually 
 * close the window until the passed in callback function is called.
 *
 * Similar to EDT_PreOpen, the doneFunction will always be called, and may be called
 * befre EDT_PreClose returns.
 *
 */

typedef void (*EDT_PreCloseCallbackFn)(void* hook);
void EDT_PreClose(MWContext * pMWContext,char* pURL, EDT_PreCloseCallbackFn doneFunction, void* hook);

/*
 * True if both urls are the same, ignores any username/password
 * information.  Does caseless comparison for file:// URLs 
 * on windows and mac.
 * url1 and url2 are relative to base1 and base2, respectively.
 * If url1 or url2 is already absolute, base1 or base2 can 
 * be passed in as NULL.
 */
XP_Bool EDT_IsSameURL(char *url1,char *url2,char *base1,char *base2);

/*
 * Extract the Extra HTML string from the ED_Element pointer in an image struct
 * (ED_Element is a void* to external users
 * Implemented in edtutil.cpp
 * Caller must XP_FREE result
 */
char * EDT_GetExtraHTML_FromImage(LO_ImageStruct *pImage);

XP_Bool EDT_IsWritableBuffer(MWContext *pContext);

/*
 * Hack in pre-encrypted files into the editor. This function lets us
 * treat the editor's TapeFS as a net stream without all the baggage associated
 * with the regular edt_Stream functions.
 */
NET_StreamClass *EDT_NetToTape(void *);

/*
 * Several functions to handle the encrypt/no-encrypt flag
 */
void EDT_EncryptToggle(MWContext *pContext);
void EDT_EncryptSet(MWContext *pContext);
void EDT_EncryptReset(MWContext *pContext);
PRBool EDT_EncryptState(MWContext *pContext);

/* Used for QA only - Ctrl+Alt+Shift+N accelerator for automated testing */
void EDT_SelectNextNonTextObject(MWContext *pContext);

XP_END_PROTOS

#endif  /* EDITOR   */
#endif  /* _edt_h_  */

