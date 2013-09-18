/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLTrackElement.h"
#include "mozilla/dom/TextTrackCue.h"
#include "nsIFrame.h"
#include "nsVideoFrame.h"

// Alternate value for the 'auto' keyword.
#define WEBVTT_AUTO -1

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_4(TextTrackCue,
                                     nsDOMEventTargetHelper,
                                     mDocument,
                                     mTrack,
                                     mTrackElement,
                                     mDisplayState)

NS_IMPL_ADDREF_INHERITED(TextTrackCue, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TextTrackCue, nsDOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TextTrackCue)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

// Set cue setting defaults based on step 19 & seq.
// in http://dev.w3.org/html5/webvtt/#parsing
void
TextTrackCue::SetDefaultCueSettings()
{
  mPosition = 50;
  mSize = 100;
  mPauseOnExit = false;
  mSnapToLines = true;
  mLine = WEBVTT_AUTO;
  mAlign = TextTrackCueAlign::Middle;
  mVertical = DirectionSetting::_empty;
}

TextTrackCue::TextTrackCue(nsISupports* aGlobal,
                           double aStartTime,
                           double aEndTime,
                           const nsAString& aText,
                           ErrorResult& aRv)
  : mText(aText)
  , mStartTime(aStartTime)
  , mEndTime(aEndTime)
  , mHead(nullptr)
  , mReset(false)
{
  SetDefaultCueSettings();
  MOZ_ASSERT(aGlobal);
  SetIsDOMBinding();
  if (NS_FAILED(StashDocument(aGlobal))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

TextTrackCue::TextTrackCue(nsISupports* aGlobal,
                           double aStartTime,
                           double aEndTime,
                           const nsAString& aText,
                           HTMLTrackElement* aTrackElement,
                           webvtt_node* head,
                           ErrorResult& aRv)
  : mText(aText)
  , mStartTime(aStartTime)
  , mEndTime(aEndTime)
  , mTrackElement(aTrackElement)
  , mHead(head)
  , mReset(false)
{
  // Ref mHead here.
  SetDefaultCueSettings();
  MOZ_ASSERT(aGlobal);
  SetIsDOMBinding();
  if (NS_FAILED(StashDocument(aGlobal))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

TextTrackCue::~TextTrackCue()
{
  if (mHead) {
    // Release mHead here.
  }
}

/** Save a reference to our creating document so it's available
 *  even when unlinked during discard/teardown.
 */
nsresult
TextTrackCue::StashDocument(nsISupports* aGlobal)
{
  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(aGlobal));
  if (!window) {
    return NS_ERROR_NO_INTERFACE;
  }
  mDocument = window->GetDoc();
  if (!mDocument) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return NS_OK;
}

void
TextTrackCue::CreateCueOverlay()
{
  mDocument->CreateElem(NS_LITERAL_STRING("div"), nullptr,
                        kNameSpaceID_XHTML,
                        getter_AddRefs(mDisplayState));
  nsGenericHTMLElement* cueDiv =
    static_cast<nsGenericHTMLElement*>(mDisplayState.get());
  cueDiv->SetClassName(NS_LITERAL_STRING("caption-text"));
}

void
TextTrackCue::RenderCue()
{
  nsRefPtr<DocumentFragment> frag = GetCueAsHTML();
  if (!frag || !mTrackElement) {
    return;
  }

  if (!mDisplayState) {
    CreateCueOverlay();
  }

  HTMLMediaElement* parent = mTrackElement->mMediaParent;
  if (!parent) {
    return;
  }

  nsIFrame* frame = parent->GetPrimaryFrame();
  if (!frame) {
    return;
  }

  nsVideoFrame* videoFrame = do_QueryFrame(frame);
  if (!videoFrame) {
    return;
  }

  nsIContent* overlay =  videoFrame->GetCaptionOverlay();
  if (!overlay) {
    return;
  }

  ErrorResult rv;
  nsContentUtils::SetNodeTextContent(overlay, EmptyString(), true);
  nsContentUtils::SetNodeTextContent(mDisplayState, EmptyString(), true);

  mDisplayState->AppendChild(*frag, rv);
  overlay->AppendChild(*mDisplayState, rv);
}

already_AddRefed<DocumentFragment>
TextTrackCue::GetCueAsHTML()
{
  MOZ_ASSERT(mDocument);
  nsRefPtr<DocumentFragment> frag = mDocument->CreateDocumentFragment();
  ConvertNodeTreeToDOMTree(frag);

  return frag.forget();
}

struct WebVTTNodeParentPair
{
  webvtt_node* mNode;
  nsIContent* mParent;

  WebVTTNodeParentPair(webvtt_node* aNode, nsIContent* aParent)
    : mNode(aNode)
    , mParent(aParent)
  {}
};

void
TextTrackCue::ConvertNodeTreeToDOMTree(nsIContent* aParentContent)
{
  nsTArray<WebVTTNodeParentPair> nodeParentPairStack;

  // mHead should actually be the head of a node tree.
  // Seed the stack for traversal.
}

already_AddRefed<nsIContent>
TextTrackCue::ConvertInternalNodeToContent(const webvtt_node* aWebVTTNode)
{
  nsIAtom* atom = nsGkAtoms::span;

  nsCOMPtr<nsIContent> cueTextContent;
  mDocument->CreateElem(nsDependentAtomString(atom), nullptr,
                        kNameSpaceID_XHTML,
                        getter_AddRefs(cueTextContent));
  return cueTextContent.forget();
}

already_AddRefed<nsIContent>
TextTrackCue::ConvertLeafNodeToContent(const webvtt_node* aWebVTTNode)
{
  nsCOMPtr<nsIContent> cueTextContent;
  // Use mDocument to create nodes on cueTextContent.

  return cueTextContent.forget();
}

JSObject*
TextTrackCue::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return VTTCueBinding::Wrap(aCx, aScope, this);
}

void
TextTrackCue::CueChanged()
{
  if (mTrack) {
    mTrack->CueChanged(*this);
  }
}
} // namespace dom
} // namespace mozilla
