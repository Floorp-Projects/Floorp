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
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsIObjectFrame.h"
#include "nsIPluginInstance.h"
#include "nsIDocShellTreeItem.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentPolicyUtils.h"
#include "nsIPropertyBag2.h"

class nsPluginDocument : public nsMediaDocument,
                         public nsIPluginDocument
{
public:
  nsPluginDocument();
  virtual ~nsPluginDocument();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPLUGINDOCUMENT

  virtual nsresult StartDocumentLoad(const char*         aCommand,
                                     nsIChannel*         aChannel,
                                     nsILoadGroup*       aLoadGroup,
                                     nsISupports*        aContainer,
                                     nsIStreamListener** aDocListener,
                                     PRBool              aReset = PR_TRUE,
                                     nsIContentSink*     aSink = nsnull);

  virtual void SetScriptGlobalObject(nsIScriptGlobalObject* aScriptGlobalObject);
  virtual PRBool CanSavePresentation(nsIRequest *aNewRequest);

  const nsCString& GetType() const { return mMimeType; }
  nsIContent*      GetPluginContent() { return mPluginContent; }

  void SkippedInstantiation() {
    mWillHandleInstantiation = PR_FALSE;
  }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsPluginDocument, nsMediaDocument)
protected:
  nsresult CreateSyntheticPluginDocument();

  nsCOMPtr<nsIContent>                     mPluginContent;
  nsRefPtr<nsMediaDocumentStreamListener>  mStreamListener;
  nsCString                                mMimeType;

  // Hack to handle the fact that plug-in loading lives in frames and that the
  // frames may not be around when we need to instantiate.  Once plug-in
  // loading moves to content, this can all go away.
  PRBool                                   mWillHandleInstantiation;
};

class nsPluginStreamListener : public nsMediaDocumentStreamListener
{
  public:
    nsPluginStreamListener(nsPluginDocument* doc) :
       nsMediaDocumentStreamListener(doc),  mPluginDoc(doc) {}
    NS_IMETHOD OnStartRequest(nsIRequest* request, nsISupports *ctxt);
  private:
    nsRefPtr<nsPluginDocument> mPluginDoc;
};


NS_IMETHODIMP
nsPluginStreamListener::OnStartRequest(nsIRequest* request, nsISupports *ctxt)
{
  nsresult rv = nsMediaDocumentStreamListener::OnStartRequest(request, ctxt);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIContent> embed = mPluginDoc->GetPluginContent();

  // Now we have a frame for our <embed>, start the load
  nsIPresShell* shell = mDocument->GetPrimaryShell();
  if (!shell) {
    // Can't instantiate w/o a shell
    mPluginDoc->SkippedInstantiation();
    return NS_BINDING_ABORTED;
  }

  // Flush out layout before we go to instantiate, because some
  // plug-ins depend on NPP_SetWindow() being called early enough and
  // nsObjectFrame does that at the end of reflow.
  shell->FlushPendingNotifications(Flush_Layout);

  nsIFrame* frame = shell->GetPrimaryFrameFor(embed);
  if (!frame) {
    mPluginDoc->SkippedInstantiation();
    return rv;
  }

  nsIObjectFrame* objFrame;
  CallQueryInterface(frame, &objFrame);
  if (!objFrame) {
    mPluginDoc->SkippedInstantiation();
    return NS_ERROR_UNEXPECTED;
  }

  rv = objFrame->Instantiate(mPluginDoc->GetType().get(),
                             mDocument->nsIDocument::GetDocumentURI());
  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_ASSERTION(mNextStream, "We should have a listener by now");
  return mNextStream->OnStartRequest(request, ctxt);
}


  // NOTE! nsDocument::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

nsPluginDocument::nsPluginDocument()
  : mWillHandleInstantiation(PR_TRUE)
{
}

nsPluginDocument::~nsPluginDocument()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsPluginDocument)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsPluginDocument, nsMediaDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mPluginContent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsPluginDocument, nsMediaDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mPluginContent)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ISUPPORTS_INHERITED1(nsPluginDocument, nsMediaDocument,
                             nsIPluginDocument)

void
nsPluginDocument::SetScriptGlobalObject(nsIScriptGlobalObject* aScriptGlobalObject)
{
  if (!aScriptGlobalObject) {
    mStreamListener = nsnull;
  }

  nsMediaDocument::SetScriptGlobalObject(aScriptGlobalObject);
}


PRBool
nsPluginDocument::CanSavePresentation(nsIRequest *aNewRequest)
{
  // Full-page plugins cannot be cached, currently, because we don't have
  // the stream listener data to feed to the plugin instance.
  return PR_FALSE;
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

  mStreamListener = new nsPluginStreamListener(this);
  if (!mStreamListener) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
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
    if (isMsgPane) {
      return NS_ERROR_FAILURE;
    }
  }

  // make our generic document
  nsresult rv = nsMediaDocument::CreateSyntheticDocument();
  NS_ENSURE_SUCCESS(rv, rv);
  // then attach our plugin

  nsIContent* body = GetBodyContent();
  if (!body) {
    NS_WARNING("no body on plugin document!");
    return NS_ERROR_FAILURE;
  }

  // remove margins from body
  NS_NAMED_LITERAL_STRING(zero, "0");
  body->SetAttr(kNameSpaceID_None, nsGkAtoms::marginwidth, zero, PR_FALSE);
  body->SetAttr(kNameSpaceID_None, nsGkAtoms::marginheight, zero, PR_FALSE);


  // make plugin content
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = mNodeInfoManager->GetNodeInfo(nsGkAtoms::embed, nsnull,
                                           kNameSpaceID_None);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);
  rv = NS_NewHTMLElement(getter_AddRefs(mPluginContent), nodeInfo, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  // make it a named element
  mPluginContent->SetAttr(kNameSpaceID_None, nsGkAtoms::name,
                          NS_LITERAL_STRING("plugin"), PR_FALSE);

  // fill viewport and auto-resize
  NS_NAMED_LITERAL_STRING(percent100, "100%");
  mPluginContent->SetAttr(kNameSpaceID_None, nsGkAtoms::width, percent100,
                          PR_FALSE);
  mPluginContent->SetAttr(kNameSpaceID_None, nsGkAtoms::height, percent100,
                          PR_FALSE);

  // set URL
  nsCAutoString src;
  mDocumentURI->GetSpec(src);
  mPluginContent->SetAttr(kNameSpaceID_None, nsGkAtoms::src,
                          NS_ConvertUTF8toUTF16(src), PR_FALSE);

  // set mime type
  mPluginContent->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                          NS_ConvertUTF8toUTF16(mMimeType), PR_FALSE);

  // This will not start the load because nsObjectLoadingContent checks whether
  // its document is an nsIPluginDocument
  body->AppendChildTo(mPluginContent, PR_FALSE);

  return NS_OK;


}

NS_IMETHODIMP
nsPluginDocument::SetStreamListener(nsIStreamListener *aListener)
{
  if (mStreamListener) {
    mStreamListener->SetStreamListener(aListener);
  }

  nsMediaDocument::UpdateTitleAndCharset(mMimeType);

  return NS_OK;
}

NS_IMETHODIMP
nsPluginDocument::Print()
{
  NS_ENSURE_TRUE(mPluginContent, NS_ERROR_FAILURE);

  nsIPresShell *shell = GetPrimaryShell();
  if (!shell) {
    return NS_OK;
  }

  nsIFrame* frame = shell->GetPrimaryFrameFor(mPluginContent);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

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

NS_IMETHODIMP
nsPluginDocument::GetWillHandleInstantiation(PRBool* aWillHandle)
{
  *aWillHandle = mWillHandleInstantiation;
  return NS_OK;
}

nsresult
NS_NewPluginDocument(nsIDocument** aResult)
{
  nsPluginDocument* doc = new nsPluginDocument();
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
