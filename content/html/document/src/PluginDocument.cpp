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

#include "MediaDocument.h"
#include "nsIPluginDocument.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsIObjectFrame.h"
#include "nsNPAPIPluginInstance.h"
#include "nsIDocShellTreeItem.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentPolicyUtils.h"
#include "nsIPropertyBag2.h"
#include "mozilla/dom/Element.h"

namespace mozilla {
namespace dom {

class PluginDocument : public MediaDocument
                     , public nsIPluginDocument
{
public:
  PluginDocument();
  virtual ~PluginDocument();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPLUGINDOCUMENT

  virtual nsresult StartDocumentLoad(const char*         aCommand,
                                     nsIChannel*         aChannel,
                                     nsILoadGroup*       aLoadGroup,
                                     nsISupports*        aContainer,
                                     nsIStreamListener** aDocListener,
                                     bool                aReset = true,
                                     nsIContentSink*     aSink = nsnull);

  virtual void SetScriptGlobalObject(nsIScriptGlobalObject* aScriptGlobalObject);
  virtual bool CanSavePresentation(nsIRequest *aNewRequest);

  const nsCString& GetType() const { return mMimeType; }
  nsIContent*      GetPluginContent() { return mPluginContent; }

  void AllowNormalInstantiation() {
    mWillHandleInstantiation = PR_FALSE;
  }

  void StartLayout() { MediaDocument::StartLayout(); }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PluginDocument, MediaDocument)
protected:
  nsresult CreateSyntheticPluginDocument();

  nsCOMPtr<nsIContent>                     mPluginContent;
  nsRefPtr<MediaDocumentStreamListener>    mStreamListener;
  nsCString                                mMimeType;

  // Hack to handle the fact that plug-in loading lives in frames and that the
  // frames may not be around when we need to instantiate.  Once plug-in
  // loading moves to content, this can all go away.
  bool                                     mWillHandleInstantiation;
};

class PluginStreamListener : public MediaDocumentStreamListener
{
public:
  PluginStreamListener(PluginDocument* doc)
    : MediaDocumentStreamListener(doc)
    , mPluginDoc(doc)
  {}
  NS_IMETHOD OnStartRequest(nsIRequest* request, nsISupports *ctxt);
private:
  nsresult SetupPlugin();

  nsRefPtr<PluginDocument> mPluginDoc;
};


NS_IMETHODIMP
PluginStreamListener::OnStartRequest(nsIRequest* request, nsISupports *ctxt)
{
  // Have to set up our plugin stuff before we call OnStartRequest, so
  // that the plugin listener can get that call.
  nsresult rv = SetupPlugin();

  NS_ASSERTION(NS_FAILED(rv) || mNextStream,
               "We should have a listener by now");
  nsresult rv2 = MediaDocumentStreamListener::OnStartRequest(request, ctxt);
  return NS_SUCCEEDED(rv) ? rv2 : rv;
}

nsresult
PluginStreamListener::SetupPlugin()
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);
  mPluginDoc->StartLayout();

  nsCOMPtr<nsIContent> embed = mPluginDoc->GetPluginContent();

  // Now we have a frame for our <embed>, start the load
  nsCOMPtr<nsIPresShell> shell = mDocument->GetShell();
  if (!shell) {
    // Can't instantiate w/o a shell
    mPluginDoc->AllowNormalInstantiation();
    return NS_BINDING_ABORTED;
  }

  // Flush out layout before we go to instantiate, because some
  // plug-ins depend on NPP_SetWindow() being called early enough and
  // nsObjectFrame does that at the end of reflow.
  shell->FlushPendingNotifications(Flush_Layout);

  nsIFrame* frame = embed->GetPrimaryFrame();
  if (!frame) {
    mPluginDoc->AllowNormalInstantiation();
    return NS_OK;
  }

  nsIObjectFrame* objFrame = do_QueryFrame(frame);
  if (!objFrame) {
    mPluginDoc->AllowNormalInstantiation();
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv = objFrame->Instantiate(mPluginDoc->GetType().get(),
                                      mDocument->nsIDocument::GetDocumentURI());
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Now that we're done, allow normal instantiation in the future
  // (say if there's a reframe of this entire presentation).
  mPluginDoc->AllowNormalInstantiation();

  return NS_OK;
}


  // NOTE! nsDocument::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

PluginDocument::PluginDocument()
  : mWillHandleInstantiation(PR_TRUE)
{
}

PluginDocument::~PluginDocument()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(PluginDocument)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PluginDocument, MediaDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mPluginContent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PluginDocument, MediaDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mPluginContent)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(PluginDocument, MediaDocument)
NS_IMPL_RELEASE_INHERITED(PluginDocument, MediaDocument)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(PluginDocument)
  NS_INTERFACE_TABLE_INHERITED1(PluginDocument, nsIPluginDocument)
NS_INTERFACE_TABLE_TAIL_INHERITING(MediaDocument)

void
PluginDocument::SetScriptGlobalObject(nsIScriptGlobalObject* aScriptGlobalObject)
{
  // Set the script global object on the superclass before doing
  // anything that might require it....
  MediaDocument::SetScriptGlobalObject(aScriptGlobalObject);

  if (aScriptGlobalObject) {
    if (!mPluginContent) {
      // Create synthetic document
#ifdef DEBUG
      nsresult rv =
#endif
        CreateSyntheticPluginDocument();
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to create synthetic document");
    }
  } else {
    mStreamListener = nsnull;
  }
}


bool
PluginDocument::CanSavePresentation(nsIRequest *aNewRequest)
{
  // Full-page plugins cannot be cached, currently, because we don't have
  // the stream listener data to feed to the plugin instance.
  return PR_FALSE;
}


nsresult
PluginDocument::StartDocumentLoad(const char*         aCommand,
                                  nsIChannel*         aChannel,
                                  nsILoadGroup*       aLoadGroup,
                                  nsISupports*        aContainer,
                                  nsIStreamListener** aDocListener,
                                  bool                aReset,
                                  nsIContentSink*     aSink)
{
  // do not allow message panes to host full-page plugins
  // returning an error causes helper apps to take over
  nsCOMPtr<nsIDocShellTreeItem> dsti (do_QueryInterface(aContainer));
  if (dsti) {
    bool isMsgPane = false;
    dsti->NameEquals(NS_LITERAL_STRING("messagepane").get(), &isMsgPane);
    if (isMsgPane) {
      return NS_ERROR_FAILURE;
    }
  }

  nsresult rv =
    MediaDocument::StartDocumentLoad(aCommand, aChannel, aLoadGroup, aContainer,
                                     aDocListener, aReset, aSink);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aChannel->GetContentType(mMimeType);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mStreamListener = new PluginStreamListener(this);
  if (!mStreamListener) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ASSERTION(aDocListener, "null aDocListener");
  NS_ADDREF(*aDocListener = mStreamListener);

  return rv;
}

nsresult
PluginDocument::CreateSyntheticPluginDocument()
{
  NS_ASSERTION(!GetShell() || !GetShell()->DidInitialReflow(),
               "Creating synthetic plugin document content too late");

  // make our generic document
  nsresult rv = MediaDocument::CreateSyntheticDocument();
  NS_ENSURE_SUCCESS(rv, rv);
  // then attach our plugin

  Element* body = GetBodyElement();
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
                                           kNameSpaceID_XHTML,
                                           nsIDOMNode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);
  rv = NS_NewHTMLElement(getter_AddRefs(mPluginContent), nodeInfo.forget(),
                         NOT_FROM_PARSER);
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
PluginDocument::SetStreamListener(nsIStreamListener *aListener)
{
  if (mStreamListener) {
    mStreamListener->SetStreamListener(aListener);
  }

  MediaDocument::UpdateTitleAndCharset(mMimeType);

  return NS_OK;
}

NS_IMETHODIMP
PluginDocument::Print()
{
  NS_ENSURE_TRUE(mPluginContent, NS_ERROR_FAILURE);

  nsIObjectFrame* objectFrame =
    do_QueryFrame(mPluginContent->GetPrimaryFrame());
  if (objectFrame) {
    nsCOMPtr<nsNPAPIPluginInstance> pi;
    objectFrame->GetPluginInstance(getter_AddRefs(pi));
    if (pi) {
      NPPrint npprint;
      npprint.mode = NP_FULL;
      npprint.print.fullPrint.pluginPrinted = PR_FALSE;
      npprint.print.fullPrint.printOne = PR_FALSE;
      npprint.print.fullPrint.platformPrint = nsnull;

      pi->Print(&npprint);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
PluginDocument::GetWillHandleInstantiation(bool* aWillHandle)
{
  *aWillHandle = mWillHandleInstantiation;
  return NS_OK;
}

} // namespace dom
} // namespace mozilla

nsresult
NS_NewPluginDocument(nsIDocument** aResult)
{
  mozilla::dom::PluginDocument* doc = new mozilla::dom::PluginDocument();
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
