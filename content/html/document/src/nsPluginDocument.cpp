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
 * The Original Code is Mozilla Communicator client code.
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

#include "nsMediaDocument.h"
#include "nsIPluginDocument.h"
#include "nsHTMLAtoms.h"
#include "nsIPresShell.h"
#include "nsIObjectFrame.h"
#include "nsIPluginInstance.h"
#include "nsIDocShellTreeItem.h"

class nsPluginDocument : public nsMediaDocument,
                         public nsIPluginDocument
{
public:
  nsPluginDocument();
  virtual ~nsPluginDocument();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPLUGINDOCUMENT

  virtual nsresult StartDocumentLoad(const char*         aCommand,
                                     nsIChannel*         aChannel,
                                     nsILoadGroup*       aLoadGroup,
                                     nsISupports*        aContainer,
                                     nsIStreamListener** aDocListener,
                                     PRBool              aReset = PR_TRUE,
                                     nsIContentSink*     aSink = nsnull);

  virtual void SetScriptGlobalObject(nsIScriptGlobalObject* aScriptGlobalObject);

protected:
  nsresult CreateSyntheticPluginDocument();

  nsCOMPtr<nsIHTMLContent>                 mPluginContent;
  nsRefPtr<nsMediaDocumentStreamListener>  mStreamListener;
  nsCString                                mMimeType;
};


  // NOTE! nsDocument::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

nsPluginDocument::nsPluginDocument()
{
}

nsPluginDocument::~nsPluginDocument()
{
}

NS_IMPL_ADDREF_INHERITED(nsPluginDocument, nsMediaDocument)
NS_IMPL_RELEASE_INHERITED(nsPluginDocument, nsMediaDocument)

NS_INTERFACE_MAP_BEGIN(nsPluginDocument)
  NS_INTERFACE_MAP_ENTRY(nsIPluginDocument)
NS_INTERFACE_MAP_END_INHERITING(nsMediaDocument)


void
nsPluginDocument::SetScriptGlobalObject(nsIScriptGlobalObject* aScriptGlobalObject)
{
  if (!aScriptGlobalObject) {
    mStreamListener = nsnull;
  }

  nsMediaDocument::SetScriptGlobalObject(aScriptGlobalObject);
}


nsresult
nsPluginDocument::StartDocumentLoad(const char*         aCommand,
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
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aChannel->GetContentType(mMimeType);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Create synthetic document
  rv = CreateSyntheticPluginDocument();
  if (NS_FAILED(rv)) {
    return rv;
  }

  mStreamListener = new nsMediaDocumentStreamListener(this);
  if (!mStreamListener)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ASSERTION(aDocListener, "null aDocListener");
  NS_ADDREF(*aDocListener = mStreamListener);

  return rv;
}

nsresult
nsPluginDocument::CreateSyntheticPluginDocument()
{
  // do not allow message panes to host full-page plugins
  // returning an error causes helper apps to take over
  nsCOMPtr<nsIDocShellTreeItem> dsti (do_QueryReferent(mDocumentContainer));
  if (dsti) {
    PRBool isMsgPane = PR_FALSE;
    dsti->NameEquals(NS_LITERAL_STRING("messagepane").get(), &isMsgPane);
    if (isMsgPane)
      return NS_ERROR_FAILURE;
  }

  // make our generic document
  nsresult rv = nsMediaDocument::CreateSyntheticDocument();
  NS_ENSURE_SUCCESS(rv, rv);
  // then attach our plugin

  nsCOMPtr<nsIContent> body = do_QueryInterface(mBodyContent);
  if (!body) {
    NS_WARNING("no body on plugin document!");
    return NS_ERROR_FAILURE;
  }

  // remove margins from body
  NS_NAMED_LITERAL_STRING(zero, "0");
  body->SetAttr(kNameSpaceID_None, nsHTMLAtoms::marginwidth, zero, PR_FALSE);
  body->SetAttr(kNameSpaceID_None, nsHTMLAtoms::marginheight, zero, PR_FALSE);


  // make plugin content
  nsCOMPtr<nsINodeInfo> nodeInfo;
  rv = mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::embed, nsnull,
                                     kNameSpaceID_None,
                                    getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);
  mPluginContent = NS_NewHTMLSharedElement(nodeInfo);
  if (!mPluginContent) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mPluginContent->SetDocument(this, PR_FALSE, PR_TRUE);

  // make it a named element
  mPluginContent->SetAttr(kNameSpaceID_None, nsHTMLAtoms::name,
                          NS_LITERAL_STRING("plugin"), PR_FALSE);

  // fill viewport and auto-resize
  NS_NAMED_LITERAL_STRING(percent100, "100%");
  mPluginContent->SetAttr(kNameSpaceID_None, nsHTMLAtoms::width, percent100,
                          PR_FALSE);
  mPluginContent->SetAttr(kNameSpaceID_None, nsHTMLAtoms::height, percent100,
                          PR_FALSE);

  // set URL
  nsCAutoString src;
  mDocumentURI->GetSpec(src);
  mPluginContent->SetAttr(kNameSpaceID_None, nsHTMLAtoms::src,
                          NS_ConvertUTF8toUTF16(src), PR_FALSE);

  // set mime type
  mPluginContent->SetAttr(kNameSpaceID_None, nsHTMLAtoms::type,
                          NS_ConvertUTF8toUTF16(mMimeType), PR_FALSE);

  body->AppendChildTo(mPluginContent, PR_FALSE, PR_FALSE);

  return NS_OK;


}

NS_IMETHODIMP
nsPluginDocument::SetStreamListener(nsIStreamListener *aListener)
{
  if (mStreamListener)
    mStreamListener->SetStreamListener(aListener);

  nsMediaDocument::UpdateTitleAndCharset(mMimeType);

  return NS_OK;
}

NS_IMETHODIMP
nsPluginDocument::Print()
{
  NS_ENSURE_TRUE(mPluginContent, NS_ERROR_FAILURE);

  nsIPresShell *shell = GetShellAt(0);
  if (!shell) {
    return NS_OK;
  }

  nsIFrame* frame = nsnull;
  shell->GetPrimaryFrameFor(mPluginContent, &frame);

  nsIObjectFrame* objectFrame = nsnull;
  CallQueryInterface(frame, &objectFrame);

  if (objectFrame) {
    nsCOMPtr<nsIPluginInstance> pi;
    objectFrame->GetPluginInstance(*getter_AddRefs(pi));

    if (pi) {
      nsPluginPrint npprint;
      npprint.mode = nsPluginMode_Full;
      npprint.print.fullPrint.pluginPrinted = PR_FALSE;
      npprint.print.fullPrint.printOne = PR_FALSE;
      npprint.print.fullPrint.platformPrint = nsnull;

      pi->Print(&npprint);
    }
  }

  return NS_OK;
}

nsresult
NS_NewPluginDocument(nsIDocument** aResult)
{
  nsPluginDocument* doc = new nsPluginDocument();
  if (!doc) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = doc->Init();

  if (NS_FAILED(rv)) {
    delete doc;
    return rv;
  }

  NS_ADDREF(*aResult = doc);

  return NS_OK;
}
