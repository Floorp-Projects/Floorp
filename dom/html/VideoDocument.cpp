/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDocument.h"
#include "nsGkAtoms.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "DocumentInlines.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Element.h"

namespace mozilla {
namespace dom {

class VideoDocument final : public MediaDocument {
 public:
  enum MediaDocumentKind MediaDocumentKind() const override {
    return MediaDocumentKind::Video;
  }

  virtual nsresult StartDocumentLoad(const char* aCommand, nsIChannel* aChannel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener** aDocListener,
                                     bool aReset = true,
                                     nsIContentSink* aSink = nullptr) override;
  virtual void SetScriptGlobalObject(
      nsIScriptGlobalObject* aScriptGlobalObject) override;

  virtual void Destroy() override {
    if (mStreamListener) {
      mStreamListener->DropDocumentRef();
    }
    MediaDocument::Destroy();
  }

  nsresult StartLayout() override;

 protected:
  nsresult CreateVideoElement();
  // Sets document <title> to reflect the file name and description.
  void UpdateTitle(nsIChannel* aChannel);

  RefPtr<MediaDocumentStreamListener> mStreamListener;
};

nsresult VideoDocument::StartDocumentLoad(const char* aCommand,
                                          nsIChannel* aChannel,
                                          nsILoadGroup* aLoadGroup,
                                          nsISupports* aContainer,
                                          nsIStreamListener** aDocListener,
                                          bool aReset, nsIContentSink* aSink) {
  nsresult rv = MediaDocument::StartDocumentLoad(
      aCommand, aChannel, aLoadGroup, aContainer, aDocListener, aReset, aSink);
  NS_ENSURE_SUCCESS(rv, rv);

  mStreamListener = new MediaDocumentStreamListener(this);
  NS_ADDREF(*aDocListener = mStreamListener);
  return rv;
}

nsresult VideoDocument::StartLayout() {
  // Create video element, and begin loading the media resource. Note we
  // delay creating the video element until now (we're called from
  // MediaDocumentStreamListener::OnStartRequest) as the PresShell is likely
  // to have been created by now, so the MediaDecoder will be able to tell
  // what kind of compositor we have, so the video element knows whether
  // it can create a hardware accelerated video decoder or not.
  nsresult rv = CreateVideoElement();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = MediaDocument::StartLayout();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void VideoDocument::SetScriptGlobalObject(
    nsIScriptGlobalObject* aScriptGlobalObject) {
  // Set the script global object on the superclass before doing
  // anything that might require it....
  MediaDocument::SetScriptGlobalObject(aScriptGlobalObject);

  if (aScriptGlobalObject && !InitialSetupHasBeenDone()) {
    DebugOnly<nsresult> rv = CreateSyntheticDocument();
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to create synthetic video document");

    if (!nsContentUtils::IsChildOfSameType(this)) {
      LinkStylesheet(NS_LITERAL_STRING(
          "resource://content-accessible/TopLevelVideoDocument.css"));
      LinkStylesheet(NS_LITERAL_STRING(
          "chrome://global/skin/media/TopLevelVideoDocument.css"));
      LinkScript(NS_LITERAL_STRING(
          "chrome://global/content/TopLevelVideoDocument.js"));
    }
    InitialSetupDone();
  }
}

nsresult VideoDocument::CreateVideoElement() {
  Element* body = GetBodyElement();
  if (!body) {
    NS_WARNING("no body on video document!");
    return NS_ERROR_FAILURE;
  }

  // make content
  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  nodeInfo = mNodeInfoManager->GetNodeInfo(
      nsGkAtoms::video, nullptr, kNameSpaceID_XHTML, nsINode::ELEMENT_NODE);

  RefPtr<HTMLMediaElement> element = static_cast<HTMLMediaElement*>(
      NS_NewHTMLVideoElement(nodeInfo.forget(), NOT_FROM_PARSER));
  if (!element) return NS_ERROR_OUT_OF_MEMORY;
  element->SetAutoplay(true, IgnoreErrors());
  element->SetControls(true, IgnoreErrors());
  element->LoadWithChannel(mChannel,
                           getter_AddRefs(mStreamListener->mNextStream));
  UpdateTitle(mChannel);

  if (nsContentUtils::IsChildOfSameType(this)) {
    // Video documents that aren't toplevel should fill their frames and
    // not have margins
    element->SetAttr(
        kNameSpaceID_None, nsGkAtoms::style,
        NS_LITERAL_STRING(
            "position:absolute; top:0; left:0; width:100%; height:100%"),
        true);
  }

  return body->AppendChildTo(element, false);
}

void VideoDocument::UpdateTitle(nsIChannel* aChannel) {
  if (!aChannel) return;

  nsAutoString fileName;
  GetFileName(fileName, aChannel);
  IgnoredErrorResult ignored;
  SetTitle(fileName, ignored);
}

}  // namespace dom
}  // namespace mozilla

nsresult NS_NewVideoDocument(mozilla::dom::Document** aResult) {
  auto* doc = new mozilla::dom::VideoDocument();

  NS_ADDREF(doc);
  nsresult rv = doc->Init();

  if (NS_FAILED(rv)) {
    NS_RELEASE(doc);
  }

  *aResult = doc;

  return rv;
}
