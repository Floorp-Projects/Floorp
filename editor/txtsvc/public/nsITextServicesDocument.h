/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsITextServicesDocument_h__
#define nsITextServicesDocument_h__

#include "nsISupports.h"

class nsIDOMDocument;
class nsIDOMRange;
class nsIPresShell;
class nsIEditor;
class nsString;
class nsITextServicesFilter;

/*
TextServicesDocument interface to outside world
*/

#define NS_ITEXTSERVICESDOCUMENT_IID            \
{ /* 019718E1-CDB5-11d2-8D3C-000000000000 */    \
0x019718e1, 0xcdb5, 0x11d2,                     \
{ 0x8d, 0x3c, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }


/**
 * The nsITextServicesDocument presents the document in as a
 * bunch of flattened text blocks. Each text block can be retrieved
 * as an nsString (array of characters).
 */
class nsITextServicesDocument  : public nsISupports{
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ITEXTSERVICESDOCUMENT_IID)

  typedef enum { eDSNormal=0, eDSUndlerline } TSDDisplayStyle;

  typedef enum { eBlockNotFound=0, // There is no text block (TB) in or before the selection (S).
                 eBlockOutside,    // No TB in S, but found one before/after S.
                 eBlockInside,     // S extends beyond the start and end of TB.
                 eBlockContains,   // TB contains entire S.
                 eBlockPartial     // S begins or ends in TB but extends outside of TB.
  } TSDBlockSelectionStatus;

  /**
   * Get the DOM document for the document in use.
   * @return aDocument the dom document [OUT]
   */
  NS_IMETHOD GetDocument(nsIDOMDocument **aDocument) = 0;

  /**
   * Initializes the text services document to use a particular
   * editor. The text services document will use the DOM document
   * and presentation shell used by the editor.
   * @param aEditor is the editor to use. The editor is AddRef'd
   * by this method.
   */
  NS_IMETHOD InitWithEditor(nsIEditor *aEditor) = 0;

  /**
   * Sets the range/extent over which the text services document
   * will iterate. Note that InitWithEditor() should have been called prior to
   * calling this method. If this method is never called, the text services
   * defaults to iterating over the entire document.
   *
   * @param aDOMRange is the range to use. aDOMRange must point to a
   * valid range object.
   */
  NS_IMETHOD SetExtent(nsIDOMRange* aDOMRange) = 0;

  /**
   * Expands the end points of the range so that it spans complete words.
   * This call does not change any internal state of the text services document.
   *
   * @param aDOMRange the range to be expanded/adjusted.
   */
  NS_IMETHOD ExpandRangeToWordBoundaries(nsIDOMRange *aRange) = 0;

  /**
   * Sets the filter to be used while iterating over content.
   * @param aFilter filter to be used while iterating over content.
   */
  NS_IMETHOD SetFilter(nsITextServicesFilter *aFilter) = 0;

  /**
   * Returns the text in the current text block.
   * @param aStr will contain the text.
   */

  NS_IMETHOD GetCurrentTextBlock(nsString *aStr) = 0;

  /**
   * Tells the document to point to the first text block
   * in the document. This method does not adjust the current
   * cursor position or selection.
   */

  NS_IMETHOD FirstBlock() = 0;

  /**
   * Tells the document to point to the last text block that
   * contains the current selection or caret.
   * @param aSelectionStatus will contain the text block selection status
   * @param aSelectionOffset will contain the offset into the
   * string returned by GetCurrentTextBlock() where the selection
   * begins.
   * @param aLength will contain the number of characters that are
   * selected in the string.
   */

  NS_IMETHOD LastSelectedBlock(TSDBlockSelectionStatus *aSelectionStatus, PRInt32 *aSelectionOffset, PRInt32 *aSelectionLength) = 0;

  /**
   * Tells the document to point to the text block before
   * the current one. This method will return NS_OK, even
   * if there is no previous block. Callers should call IsDone()
   * to check if we have gone beyond the first text block in
   * the document.
   */

  NS_IMETHOD PrevBlock() = 0;

  /**
   * Tells the document to point to the text block after
   * the current one. This method will return NS_OK, even
   * if there is no next block. Callers should call IsDone()
   * to check if we have gone beyond the last text block
   * in the document.
   */

  NS_IMETHOD NextBlock() = 0;

  /**
   * IsDone() will always set aIsDone == PR_FALSE unless
   * the document contains no text, PrevBlock() was called
   * while the document was already pointing to the first
   * text block in the document, or NextBlock() was called
   * while the document was already pointing to the last
   * text block in the document.
   * @param aIsDone will contain the result.
   */

  NS_IMETHOD IsDone(PRBool *aIsDone) = 0;

  /**
   * SetSelection() allows the caller to set the selection
   * based on an offset into the string returned by
   * GetCurrentTextBlock(). A length of zero places the cursor
   * at that offset. A positive non-zero length "n" selects
   * n characters in the string.
   * @param aOffset offset into string returned by GetCurrentTextBlock().
   * @param aLength number characters selected.
   */

  NS_IMETHOD SetSelection(PRInt32 aOffset, PRInt32 aLength) = 0;

  /**
   * Scrolls the document so that the current selection is visible.
   */

  NS_IMETHOD ScrollSelectionIntoView() = 0;

  /**
   * Deletes the text selected by SetSelection(). Calling
   * DeleteSelection with nothing selected, or with a collapsed
   * selection (cursor) does nothing and returns NS_OK.
   */

  NS_IMETHOD DeleteSelection() = 0;

  /**
   * Inserts the given text at the current cursor position.
   * If there is a selection, it will be deleted before the
   * text is inserted.
   */

  NS_IMETHOD InsertText(const nsString *aText) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsITextServicesDocument,
                              NS_ITEXTSERVICESDOCUMENT_IID)

#endif // nsITextServicesDocument_h__

