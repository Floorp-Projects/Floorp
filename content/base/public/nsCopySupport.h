/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCopySupport_h__
#define nsCopySupport_h__

#include "nscore.h"
#include "nsINode.h"

class nsISelection;
class nsIDocument;
class nsIImageLoadingContent;
class nsIContent;
class nsITransferable;
class nsACString;
class nsAString;
class nsIPresShell;
class nsILoadContext;

class nsCopySupport
{
  // class of static helper functions for copy support
  public:
    static nsresult HTMLCopy(nsISelection *aSel, nsIDocument *aDoc, int16_t aClipboardID);
    static nsresult DoHooks(nsIDocument *aDoc, nsITransferable *aTrans,
                            bool *aDoPutOnClipboard);

    // Get the selection, or entire document, in the format specified by the mime type
    // (text/html or text/plain). If aSel is non-null, use it, otherwise get the entire
    // doc.
    static nsresult GetContents(const nsACString& aMimeType, uint32_t aFlags, nsISelection *aSel, nsIDocument *aDoc, nsAString& outdata);
    
    static nsresult ImageCopy(nsIImageLoadingContent* aImageElement,
                              nsILoadContext* aLoadContext,
                              int32_t aCopyFlags);

    // Get the selection as a transferable. Similar to HTMLCopy except does
    // not deal with the clipboard.
    static nsresult GetTransferableForSelection(nsISelection* aSelection,
                                                nsIDocument* aDocument,
                                                nsITransferable** aTransferable);

    // Same as GetTransferableForSelection, but doesn't skip invisible content.
    static nsresult GetTransferableForNode(nsINode* aNode,
                                           nsIDocument* aDoc,
                                           nsITransferable** aTransferable);
    /**
     * Retrieve the selection for the given document. If the current focus
     * within the document has its own selection, aSelection will be set to it
     * and this focused content node returned. Otherwise, aSelection will be
     * set to the document's selection and null will be returned.
     */
    static nsIContent* GetSelectionForCopy(nsIDocument* aDocument,
                                           nsISelection** aSelection);

    /**
     * Returns true if a copy operation is currently permitted based on the
     * current focus and selection within the specified document.
     */
    static bool CanCopy(nsIDocument* aDocument);

    /**
     * Fires a cut, copy or paste event, on the given presshell, depending
     * on the value of aType, which should be either NS_CUT, NS_COPY or
     * NS_PASTE, and perform the default copy action if the event was not
     * cancelled.
     *
     * If aSelection is specified, then this selection is used as the target
     * of the operation. Otherwise, GetSelectionForCopy is used to retrieve
     * the current selection.
     *
     * This will fire a cut, copy or paste event at the node at the start
     * point of the selection. If a cut or copy event is not cancelled, the
     * selection is copied to the clipboard and true is returned. Paste events
     * have no default behaviour but true will be returned. It is expected
     * that the caller will execute any needed default paste behaviour. Also,
     * note that this method only copies text to the clipboard, the caller is
     * responsible for removing the content during a cut operation if true is
     * returned.
     *
     * If the event is cancelled or an error occurs, false will be returned.
     */
    static bool FireClipboardEvent(int32_t aType,
                                     nsIPresShell* aPresShell,
                                     nsISelection* aSelection);
};

#endif
