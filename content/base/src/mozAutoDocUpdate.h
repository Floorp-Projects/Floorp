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
 * Boris Zbarsky <bzbarsky@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsContentUtils.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"

/**
 * Helper class to automatically handle batching of document updates.  This
 * class will call BeginUpdate on construction and EndUpdate on destruction on
 * the given document with the given update type.  The document could be null,
 * in which case no updates will be called.  The constructor also takes a
 * boolean that can be set to false to prevent notifications.
 */
class NS_STACK_CLASS mozAutoDocUpdate
{
public:
  mozAutoDocUpdate(nsIDocument* aDocument, nsUpdateType aUpdateType,
                   PRBool aNotify) :
    mDocument(aNotify ? aDocument : nsnull),
    mUpdateType(aUpdateType)
  {
    if (mDocument) {
      mDocument->BeginUpdate(mUpdateType);
    }
    else {
      nsContentUtils::AddScriptBlocker();
    }
  }

  ~mozAutoDocUpdate()
  {
    if (mDocument) {
      mDocument->EndUpdate(mUpdateType);
    }
    else {
      nsContentUtils::RemoveScriptBlocker();
    }
  }

private:
  nsCOMPtr<nsIDocument> mDocument;
  nsUpdateType mUpdateType;
};

#define MOZ_AUTO_DOC_UPDATE_PASTE2(tok,line) tok##line
#define MOZ_AUTO_DOC_UPDATE_PASTE(tok,line) \
  MOZ_AUTO_DOC_UPDATE_PASTE2(tok,line)
#define MOZ_AUTO_DOC_UPDATE(doc,type,notify) \
  mozAutoDocUpdate MOZ_AUTO_DOC_UPDATE_PASTE(_autoDocUpdater_, __LINE__) \
  (doc,type,notify)


/**
 * Creates an update batch only under certain conditions.
 * Use this rather than mozAutoDocUpdate when you expect inner updates
 * to notify but you don't always want to spec cycles creating a batch.
 * This is needed to avoid having this batch always create a blocker,
 * but then have inner mozAutoDocUpdate call the last EndUpdate before.
 * we remove that blocker. See bug 423269.
 */
class NS_STACK_CLASS mozAutoDocConditionalContentUpdateBatch
{
public:
  mozAutoDocConditionalContentUpdateBatch(nsIDocument* aDocument,
                                          PRBool aNotify) :
    mDocument(aNotify ? aDocument : nsnull)
  {
    if (mDocument) {
      mDocument->BeginUpdate(UPDATE_CONTENT_MODEL);
    }
  }

  ~mozAutoDocConditionalContentUpdateBatch()
  {
    if (mDocument) {
      mDocument->EndUpdate(UPDATE_CONTENT_MODEL);
    }
  }

private:
  nsCOMPtr<nsIDocument> mDocument;
};
