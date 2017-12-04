/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsITextServicesDocument_h__
#define nsITextServicesDocument_h__

#include "nsISupports.h"
#include "nsStringFwd.h"

class nsIDOMDocument;
class nsIEditor;
class nsITextServicesFilter;
class nsRange;

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
  NS_IMETHOD SetExtent(nsRange* aDOMRange) = 0;

  /**
   * Expands the end points of the range so that it spans complete words.
   * This call does not change any internal state of the text services document.
   *
   * @param aDOMRange the range to be expanded/adjusted.
   */
  NS_IMETHOD ExpandRangeToWordBoundaries(nsRange* aRange) = 0;

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

  NS_IMETHOD LastSelectedBlock(TSDBlockSelectionStatus *aSelectionStatus, int32_t *aSelectionOffset, int32_t *aSelectionLength) = 0;

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
   * IsDone() will always set aIsDone == false unless
   * the document contains no text, PrevBlock() was called
   * while the document was already pointing to the first
   * text block in the document, or NextBlock() was called
   * while the document was already pointing to the last
   * text block in the document.
   * @param aIsDone will contain the result.
   */

  NS_IMETHOD IsDone(bool *aIsDone) = 0;

  /**
   * SetSelection() allows the caller to set the selection
   * based on an offset into the string returned by
   * GetCurrentTextBlock(). A length of zero places the cursor
   * at that offset. A positive non-zero length "n" selects
   * n characters in the string.
   * @param aOffset offset into string returned by GetCurrentTextBlock().
   * @param aLength number characters selected.
   */

  NS_IMETHOD SetSelection(int32_t aOffset, int32_t aLength) = 0;

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

