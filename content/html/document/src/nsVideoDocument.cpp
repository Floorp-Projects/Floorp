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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
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

#include "nsMediaDocument.h"
#include "nsGkAtoms.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsHTMLMediaElement.h"
#include "nsIDocShellTreeItem.h"
#include "mozilla/dom/Element.h"

using namespace mozilla::dom;

class nsVideoDocument : public nsMediaDocument
{
public:
  virtual nsresult StartDocumentLoad(const char*         aCommand,
                                     nsIChannel*         aChannel,
                                     nsILoadGroup*       aLoadGroup,
                                     nsISupports*        aContainer,
                                     nsIStreamListener** aDocListener,
                                     PRBool              aReset = PR_TRUE,
                                     nsIContentSink*     aSink = nsnull);

protected:

  // Sets document <title> to reflect the file name and description.
  void UpdateTitle(nsIChannel* aChannel);

  nsresult CreateSyntheticVideoDocument(nsIChannel* aChannel,
                                        nsIStreamListener** aListener);

  nsRefPtr<nsMediaDocumentStreamListener> mStreamListener;
};

nsresult
nsVideoDocument::StartDocumentLoad(const char*         aCommand,
                                   nsIChannel*         aChannel,
                                   nsILoadGroup*       aLoadGroup,
                                   nsISupports*        aContainer,
                                   nsIStreamListener** aDocListener,
                                   PRBool              aReset,
                                   nsIContentSink*     aSink)
{
  nsresult rv =
    nsMediaDocument::StartDocumentLoad(aCommand, aChannel, aLoadGroup,
                                       aContainer, aDocListener, aReset,
                                       aSink);
  NS_ENSURE_SUCCESS(rv, rv);

  mStreamListener = new nsMediaDocumentStreamListener(this);
  if (!mStreamListener)
    return NS_ERROR_OUT_OF_MEMORY;

  // Create synthetic document
  rv = CreateSyntheticVideoDocument(aChannel,
      getter_AddRefs(mStreamListener->mNextStream));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*aDocListener = mStreamListener);
  return rv;
}

nsresult
nsVideoDocument::CreateSyntheticVideoDocument(nsIChannel* aChannel,
                                              nsIStreamListener** aListener)
{
  // make our generic document
  nsresult rv = nsMediaDocument::CreateSyntheticDocument();
  NS_ENSURE_SUCCESS(rv, rv);

  Element* body = GetBodyElement();
  if (!body) {
    NS_WARNING("no body on video document!");
    return NS_ERROR_FAILURE;
  }

  // make content
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = mNodeInfoManager->GetNodeInfo(nsGkAtoms::video, nsnull,
                                           kNameSpaceID_XHTML);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_FAILURE);

  nsRefPtr<nsHTMLMediaElement> element =
    static_cast<nsHTMLMediaElement*>(NS_NewHTMLVideoElement(nodeInfo.forget(),
                                                            NOT_FROM_PARSER));
  if (!element)
    return NS_ERROR_OUT_OF_MEMORY;
  element->SetAutoplay(PR_TRUE);
  element->SetControls(PR_TRUE);
  element->LoadWithChannel(aChannel, aListener);
  UpdateTitle(aChannel);

  if (nsContentUtils::IsChildOfSameType(this)) {
    // Video documents that aren't toplevel should fill their frames and
    // not have margins
    element->SetAttr(kNameSpaceID_None, nsGkAtoms::style,
        NS_LITERAL_STRING("position:absolute; top:0; left:0; width:100%; height:100%"),
        PR_TRUE);
  }

  return body->AppendChildTo(element, PR_FALSE);
}

void
nsVideoDocument::UpdateTitle(nsIChannel* aChannel)
{
  if (!aChannel)
    return;

  nsAutoString fileName;
  GetFileName(fileName);
  SetTitle(fileName);
}

nsresult
NS_NewVideoDocument(nsIDocument** aResult)
{
  nsVideoDocument* doc = new nsVideoDocument();
  if (!doc) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(doc);
  nsresult rv = doc->Init();

  if (NS_FAILED(rv)) {
    NS_RELEASE(doc);
  }

  *aResult = doc;

  return rv;
}
