/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCopySupport_h__
#define nsCopySupport_h__

#include "nsError.h"
#include "mozilla/dom/Document.h"
#include "nsStringFwd.h"
#include "mozilla/EventForwards.h"

class nsINode;
class nsIImageLoadingContent;
class nsIContent;
class nsITransferable;
class nsILoadContext;

namespace mozilla {
class PresShell;
namespace dom {
class Document;
class Selection;
}  // namespace dom
}  // namespace mozilla

class nsCopySupport {
  // class of static helper functions for copy support
 public:
  static nsresult ClearSelectionCache();

  /**
   * @param aDoc Needs to be not nullptr.
   */
  static nsresult HTMLCopy(mozilla::dom::Selection* aSel,
                           mozilla::dom::Document* aDoc, int16_t aClipboardID,
                           bool aWithRubyAnnotation);

  // Get the selection, or entire document, in the format specified by the mime
  // type (text/html or text/plain). If aSel is non-null, use it, otherwise get
  // the entire doc.
  static nsresult GetContents(const nsACString& aMimeType, uint32_t aFlags,
                              mozilla::dom::Selection* aSel,
                              mozilla::dom::Document* aDoc, nsAString& outdata);

  static nsresult ImageCopy(nsIImageLoadingContent* aImageElement,
                            nsILoadContext* aLoadContext, int32_t aCopyFlags);

  // Get the selection as a transferable. Similar to HTMLCopy except does
  // not deal with the clipboard.
  // @param aSelection Can be nullptr.
  // @param aDocument Needs to be not nullptr.
  // @param aTransferable Needs to be not nullptr.
  static nsresult GetTransferableForSelection(
      mozilla::dom::Selection* aSelection, mozilla::dom::Document* aDocument,
      nsITransferable** aTransferable);

  // Same as GetTransferableForSelection, but doesn't skip invisible content.
  // @param aNode Needs to be not nullptr.
  // @param aDoc Needs to be not nullptr.
  // @param aTransferable Needs to be not nullptr.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  static nsresult GetTransferableForNode(nsINode* aNode,
                                         mozilla::dom::Document* aDoc,
                                         nsITransferable** aTransferable);
  /**
   * Retrieve the selection for the given document. If the current focus
   * within the document has its own selection, aSelection will be set to it
   * and this focused content node returned. Otherwise, aSelection will be
   * set to the document's selection and null will be returned.
   */
  static nsIContent* GetSelectionForCopy(mozilla::dom::Document* aDocument,
                                         mozilla::dom::Selection** aSelection);

  /**
   * Returns true if a copy operation is currently permitted based on the
   * current focus and selection within the specified document.
   */
  static bool CanCopy(mozilla::dom::Document* aDocument);

  /**
   * Fires a cut, copy or paste event, on the given presshell, depending
   * on the value of aEventMessage, which should be either eCut, eCopy or
   * ePaste, and perform the default copy action if the event was not
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
   * aClipboardType specifies which clipboard to use, from nsIClipboard.
   *
   * If aActionTaken is non-NULL, it will be set to true if an action was
   * taken, whether it be the default action or the default being prevented.
   *
   * If the event is cancelled or an error occurs, false will be returned.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  static bool FireClipboardEvent(mozilla::EventMessage aEventMessage,
                                 int32_t aClipboardType,
                                 mozilla::PresShell* aPresShell,
                                 mozilla::dom::Selection* aSelection,
                                 bool* aActionTaken = nullptr);
};

#endif
