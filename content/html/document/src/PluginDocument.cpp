/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDocument.h"
#include "nsIPluginDocument.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsIObjectFrame.h"
#include "nsNPAPIPluginInstance.h"
#include "nsIDocumentInlines.h"
#include "nsIDocShellTreeItem.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentPolicyUtils.h"
#include "nsIPropertyBag2.h"
#include "mozilla/dom/Element.h"
#include "nsObjectLoadingContent.h"
#include "GeckoProfiler.h"

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
                                     nsIContentSink*     aSink = nullptr);

  virtual void SetScriptGlobalObject(nsIScriptGlobalObject* aScriptGlobalObject);
  virtual bool CanSavePresentation(nsIRequest *aNewRequest);

  const nsCString& GetType() const { return mMimeType; }
  nsIContent*      GetPluginContent() { return mPluginContent; }

  void StartLayout() { MediaDocument::StartLayout(); }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PluginDocument, MediaDocument)
protected:
  nsresult CreateSyntheticPluginDocument();

  nsCOMPtr<nsIContent>                     mPluginContent;
  nsRefPtr<MediaDocumentStreamListener>    mStreamListener;
  nsCString                                mMimeType;
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
  nsRefPtr<PluginDocument> mPluginDoc;
};


NS_IMETHODIMP
PluginStreamListener::OnStartRequest(nsIRequest* request, nsISupports *ctxt)
{
  PROFILER_LABEL("PluginStreamListener", "OnStartRequest");

  nsCOMPtr<nsIContent> embed = mPluginDoc->GetPluginContent();
  nsCOMPtr<nsIObjectLoadingContent> objlc = do_QueryInterface(embed);
  nsCOMPtr<nsIStreamListener> objListener = do_QueryInterface(objlc);

  if (!objListener) {
    NS_NOTREACHED("PluginStreamListener without appropriate content node");
    return NS_BINDING_ABORTED;
  }

  SetStreamListener(objListener);

  // Sets up the ObjectLoadingContent tag as if it is waiting for a
  // channel, so it can proceed with a load normally once it gets OnStartRequest
  nsresult rv = objlc->InitializeFromChannel(request);
  if (NS_FAILED(rv)) {
    NS_NOTREACHED("InitializeFromChannel failed");
    return rv;
  }

  // Note that because we're now hooked up to a plugin listener, this will
  // likely spawn a plugin, which may re-enter.
  return MediaDocumentStreamListener::OnStartRequest(request, ctxt);
}

  // NOTE! nsDocument::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

PluginDocument::PluginDocument()
{}

PluginDocument::~PluginDocument()
{}


NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PluginDocument, MediaDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPluginContent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PluginDocument, MediaDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPluginContent)
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
    BecomeInteractive();
  } else {
    mStreamListener = nullptr;
  }
}


bool
PluginDocument::CanSavePresentation(nsIRequest *aNewRequest)
{
  // Full-page plugins cannot be cached, currently, because we don't have
  // the stream listener data to feed to the plugin instance.
  return false;
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

  MediaDocument::UpdateTitleAndCharset(mMimeType);

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
  NS_ASSERTION(!GetShell() || !GetShell()->DidInitialize(),
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
  body->SetAttr(kNameSpaceID_None, nsGkAtoms::marginwidth, zero, false);
  body->SetAttr(kNameSpaceID_None, nsGkAtoms::marginheight, zero, false);


  // make plugin content
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = mNodeInfoManager->GetNodeInfo(nsGkAtoms::embed, nullptr,
                                           kNameSpaceID_XHTML,
                                           nsIDOMNode::ELEMENT_NODE);
  rv = NS_NewHTMLElement(getter_AddRefs(mPluginContent), nodeInfo.forget(),
                         NOT_FROM_PARSER);
  NS_ENSURE_SUCCESS(rv, rv);

  // make it a named element
  mPluginContent->SetAttr(kNameSpaceID_None, nsGkAtoms::name,
                          NS_LITERAL_STRING("plugin"), false);

  // fill viewport and auto-resize
  NS_NAMED_LITERAL_STRING(percent100, "100%");
  mPluginContent->SetAttr(kNameSpaceID_None, nsGkAtoms::width, percent100,
                          false);
  mPluginContent->SetAttr(kNameSpaceID_None, nsGkAtoms::height, percent100,
                          false);

  // set URL
  nsAutoCString src;
  mDocumentURI->GetSpec(src);
  mPluginContent->SetAttr(kNameSpaceID_None, nsGkAtoms::src,
                          NS_ConvertUTF8toUTF16(src), false);

  // set mime type
  mPluginContent->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                          NS_ConvertUTF8toUTF16(mMimeType), false);

  // nsHTML(Shared)ObjectElement does not kick off a load on BindToTree if it is
  // to a PluginDocument
  body->AppendChildTo(mPluginContent, false);

  return NS_OK;


}

NS_IMETHODIMP
PluginDocument::Print()
{
  NS_ENSURE_TRUE(mPluginContent, NS_ERROR_FAILURE);

  nsIObjectFrame* objectFrame =
    do_QueryFrame(mPluginContent->GetPrimaryFrame());
  if (objectFrame) {
    nsRefPtr<nsNPAPIPluginInstance> pi;
    objectFrame->GetPluginInstance(getter_AddRefs(pi));
    if (pi) {
      NPPrint npprint;
      npprint.mode = NP_FULL;
      npprint.print.fullPrint.pluginPrinted = false;
      npprint.print.fullPrint.printOne = false;
      npprint.print.fullPrint.platformPrint = nullptr;

      pi->Print(&npprint);
    }
  }

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
