/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/HTMLTrackElement.h"
#include "mozilla/dom/HTMLTrackElementBinding.h"
#include "mozilla/dom/HTMLUnknownElement.h"
#include "WebVTTListener.h"
#include "nsAttrValueInlines.h"
#include "nsCOMPtr.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsICachingChannel.h"
#include "nsIChannelEventSink.h"
#include "nsIChannelPolicy.h"
#include "nsIContentPolicy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIDocument.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMHTMLMediaElement.h"
#include "nsIHttpChannel.h"
#include "nsIInterfaceRequestor.h"
#include "nsILoadGroup.h"
#include "nsIObserver.h"
#include "nsIStreamListener.h"
#include "nsISupportsImpl.h"
#include "nsMappedAttributes.h"
#include "nsNetUtil.h"
#include "nsRuleData.h"
#include "nsStyleConsts.h"
#include "nsThreadUtils.h"
#include "nsVideoFrame.h"

#ifdef PR_LOGGING
static PRLogModuleInfo* gTrackElementLog;
#define LOG(type, msg) PR_LOG(gTrackElementLog, type, msg)
#else
#define LOG(type, msg)
#endif

// Replace the usual NS_IMPL_NS_NEW_HTML_ELEMENT(Track) so
// we can return an UnknownElement instead when pref'd off.
nsGenericHTMLElement*
NS_NewHTMLTrackElement(already_AddRefed<nsINodeInfo>&& aNodeInfo,
                       mozilla::dom::FromParser aFromParser)
{
  if (!mozilla::dom::HTMLTrackElement::IsWebVTTEnabled()) {
    return new mozilla::dom::HTMLUnknownElement(aNodeInfo);
  }

  return new mozilla::dom::HTMLTrackElement(aNodeInfo);
}

namespace mozilla {
namespace dom {

// Map html attribute string values to TextTrackKind enums.
static MOZ_CONSTEXPR nsAttrValue::EnumTable kKindTable[] = {
  { "subtitles", static_cast<int16_t>(TextTrackKind::Subtitles) },
  { "captions", static_cast<int16_t>(TextTrackKind::Captions) },
  { "descriptions", static_cast<int16_t>(TextTrackKind::Descriptions) },
  { "chapters", static_cast<int16_t>(TextTrackKind::Chapters) },
  { "metadata", static_cast<int16_t>(TextTrackKind::Metadata) },
  { 0 }
};

// The default value for kKindTable is "subtitles"
static MOZ_CONSTEXPR const char* kKindTableDefaultString = kKindTable->tag;

/** HTMLTrackElement */
HTMLTrackElement::HTMLTrackElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
#ifdef PR_LOGGING
  if (!gTrackElementLog) {
    gTrackElementLog = PR_NewLogModule("nsTrackElement");
  }
#endif
}

HTMLTrackElement::~HTMLTrackElement()
{
}

NS_IMPL_ELEMENT_CLONE(HTMLTrackElement)

NS_IMPL_ADDREF_INHERITED(HTMLTrackElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLTrackElement, Element)

NS_IMPL_CYCLE_COLLECTION_INHERITED_4(HTMLTrackElement, nsGenericHTMLElement,
                                     mTrack, mChannel, mMediaParent,
                                     mListener)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(HTMLTrackElement)
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLElement)

void
HTMLTrackElement::GetKind(DOMString& aKind) const
{
  GetEnumAttr(nsGkAtoms::kind, kKindTableDefaultString, aKind);
}

void
HTMLTrackElement::OnChannelRedirect(nsIChannel* aChannel,
                                    nsIChannel* aNewChannel,
                                    uint32_t aFlags)
{
  NS_ASSERTION(aChannel == mChannel, "Channels should match!");
  mChannel = aNewChannel;
}

JSObject*
HTMLTrackElement::WrapNode(JSContext* aCx)
{
  return HTMLTrackElementBinding::Wrap(aCx, this);
}

bool
HTMLTrackElement::IsWebVTTEnabled()
{
  // Our callee does not use its arguments.
  return HTMLTrackElementBinding::ConstructorEnabled(nullptr, JS::NullPtr());
}

TextTrack*
HTMLTrackElement::GetTrack()
{
  if (!mTrack) {
    CreateTextTrack();
  }

  return mTrack;
}

void
HTMLTrackElement::CreateTextTrack()
{
  nsString label, srcLang;
  GetSrclang(srcLang);
  GetLabel(label);

  TextTrackKind kind;
  if (const nsAttrValue* value = GetParsedAttr(nsGkAtoms::kind)) {
    kind = static_cast<TextTrackKind>(value->GetEnumValue());
  } else {
    kind = TextTrackKind::Subtitles;
  }

  bool hasHadScriptObject = true;
  nsIScriptGlobalObject* scriptObject =
    OwnerDoc()->GetScriptHandlingObject(hasHadScriptObject);

  NS_ENSURE_TRUE_VOID(scriptObject || !hasHadScriptObject);

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(scriptObject);
  mTrack = new TextTrack(window, kind, label, srcLang,
                         TextTrackMode::Disabled,
                         TextTrackReadyState::NotLoaded,
                         TextTrackSource::Track);
  mTrack->SetTrackElement(this);

  if (mMediaParent) {
    mMediaParent->AddTextTrack(mTrack);
  }
}

bool
HTMLTrackElement::ParseAttribute(int32_t aNamespaceID,
                                 nsIAtom* aAttribute,
                                 const nsAString& aValue,
                                 nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::kind) {
    // Case-insensitive lookup, with the first element as the default.
    return aResult.ParseEnumValue(aValue, kKindTable, false, kKindTable);
  }

  // Otherwise call the generic implementation.
  return nsGenericHTMLElement::ParseAttribute(aNamespaceID,
                                              aAttribute,
                                              aValue,
                                              aResult);
}

void
HTMLTrackElement::LoadResource()
{
  // Find our 'src' url
  nsAutoString src;
  if (!GetAttr(kNameSpaceID_None, nsGkAtoms::src, src)) {
    return;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NewURIFromString(src, getter_AddRefs(uri));
  NS_ENSURE_TRUE_VOID(NS_SUCCEEDED(rv));
  LOG(PR_LOG_ALWAYS, ("%p Trying to load from src=%s", this,
      NS_ConvertUTF16toUTF8(src).get()));

  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nullptr;
  }

  rv = nsContentUtils::GetSecurityManager()->
    CheckLoadURIWithPrincipal(NodePrincipal(), uri,
                              nsIScriptSecurityManager::STANDARD);
  NS_ENSURE_TRUE_VOID(NS_SUCCEEDED(rv));

  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_MEDIA,
                                 uri,
                                 NodePrincipal(),
                                 static_cast<Element*>(this),
                                 NS_LITERAL_CSTRING("text/vtt"), // mime type
                                 nullptr, // extra
                                 &shouldLoad,
                                 nsContentUtils::GetContentPolicy(),
                                 nsContentUtils::GetSecurityManager());
  NS_ENSURE_TRUE_VOID(NS_SUCCEEDED(rv));
  if (NS_CP_REJECTED(shouldLoad)) {
    return;
  }

  CreateTextTrack();

  // Check for a Content Security Policy to pass down to the channel
  // created to load the media content.
  nsCOMPtr<nsIChannelPolicy> channelPolicy;
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  rv = NodePrincipal()->GetCsp(getter_AddRefs(csp));
  NS_ENSURE_TRUE_VOID(NS_SUCCEEDED(rv));
  if (csp) {
    channelPolicy = do_CreateInstance("@mozilla.org/nschannelpolicy;1");
    if (!channelPolicy) {
      return;
    }
    channelPolicy->SetContentSecurityPolicy(csp);
    channelPolicy->SetLoadType(nsIContentPolicy::TYPE_MEDIA);
  }
  nsCOMPtr<nsIChannel> channel;
  nsCOMPtr<nsILoadGroup> loadGroup = OwnerDoc()->GetDocumentLoadGroup();
  rv = NS_NewChannel(getter_AddRefs(channel),
                     uri,
                     nullptr,
                     loadGroup,
                     nullptr,
                     nsIRequest::LOAD_NORMAL,
                     channelPolicy);
  NS_ENSURE_TRUE_VOID(NS_SUCCEEDED(rv));

  mListener = new WebVTTListener(this);
  rv = mListener->LoadResource();
  NS_ENSURE_TRUE_VOID(NS_SUCCEEDED(rv));
  channel->SetNotificationCallbacks(mListener);

  LOG(PR_LOG_DEBUG, ("opening webvtt channel"));
  rv = channel->AsyncOpen(mListener, nullptr);
  NS_ENSURE_TRUE_VOID(NS_SUCCEEDED(rv));

  mChannel = channel;
}

nsresult
HTMLTrackElement::BindToTree(nsIDocument* aDocument,
                             nsIContent* aParent,
                             nsIContent* aBindingParent,
                             bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument,
                                                 aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aDocument) {
    return NS_OK;
  }

  LOG(PR_LOG_DEBUG, ("Track Element bound to tree."));
  if (!aParent || !aParent->IsNodeOfType(nsINode::eMEDIA)) {
    return NS_OK;
  }

  // Store our parent so we can look up its frame for display.
  if (!mMediaParent) {
    mMediaParent = static_cast<HTMLMediaElement*>(aParent);

    HTMLMediaElement* media = static_cast<HTMLMediaElement*>(aParent);
    // TODO: separate notification for 'alternate' tracks?
    media->NotifyAddedSource();
    LOG(PR_LOG_DEBUG, ("Track element sent notification to parent."));

    mMediaParent->RunInStableState(
      NS_NewRunnableMethod(this, &HTMLTrackElement::LoadResource));
  }

  return NS_OK;
}

void
HTMLTrackElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  if (mMediaParent) {
    // mTrack can be null if HTMLTrackElement::LoadResource has never been
    // called.
    if (mTrack) {
      mMediaParent->RemoveTextTrack(mTrack);
    }
    if (aNullParent) {
      mMediaParent = nullptr;
    }
  }

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

uint16_t
HTMLTrackElement::ReadyState() const
{
  if (!mTrack) {
    return TextTrackReadyState::NotLoaded;
  }

  return mTrack->ReadyState();
}

void
HTMLTrackElement::SetReadyState(uint16_t aReadyState)
{
  if (mTrack) {
    switch (aReadyState) {
      case TextTrackReadyState::Loaded:
        DispatchTrackRunnable(NS_LITERAL_STRING("loaded"));
        break;
      case TextTrackReadyState::FailedToLoad:
        DispatchTrackRunnable(NS_LITERAL_STRING("error"));
        break;
    }
    mTrack->SetReadyState(aReadyState);
  }
}

void
HTMLTrackElement::DispatchTrackRunnable(const nsString& aEventName)
{
  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethodWithArg
      <const nsString>(this,
                       &HTMLTrackElement::DispatchTrustedEvent,
                       aEventName);
  NS_DispatchToMainThread(runnable);
}

void
HTMLTrackElement::DispatchTrustedEvent(const nsAString& aName)
{
  nsIDocument* doc = OwnerDoc();
  if (!doc) {
    return;
  }
  nsContentUtils::DispatchTrustedEvent(doc, static_cast<nsIContent*>(this),
                                       aName, false, false);
}

} // namespace dom
} // namespace mozilla
