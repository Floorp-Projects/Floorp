/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/HTMLTrackElement.h"
#include "mozilla/dom/HTMLTrackElementBinding.h"
#include "mozilla/dom/HTMLUnknownElement.h"
#include "nsIContentPolicy.h"
#include "mozilla/LoadInfo.h"
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
#include "nsISupportsPrimitives.h"
#include "nsMappedAttributes.h"
#include "nsNetUtil.h"
#include "nsRuleData.h"
#include "nsStyleConsts.h"
#include "nsThreadUtils.h"
#include "nsVideoFrame.h"

static mozilla::LazyLogModule gTrackElementLog("nsTrackElement");
#define LOG(type, msg) MOZ_LOG(gTrackElementLog, type, msg)

// Replace the usual NS_IMPL_NS_NEW_HTML_ELEMENT(Track) so
// we can return an UnknownElement instead when pref'd off.
nsGenericHTMLElement*
NS_NewHTMLTrackElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
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
static constexpr nsAttrValue::EnumTable kKindTable[] = {
  { "subtitles", static_cast<int16_t>(TextTrackKind::Subtitles) },
  { "captions", static_cast<int16_t>(TextTrackKind::Captions) },
  { "descriptions", static_cast<int16_t>(TextTrackKind::Descriptions) },
  { "chapters", static_cast<int16_t>(TextTrackKind::Chapters) },
  { "metadata", static_cast<int16_t>(TextTrackKind::Metadata) },
  { 0 }
};

// Invalid values are treated as "metadata" in ParseAttribute, but if no value
// at all is specified, it's treated as "subtitles" in GetKind
static constexpr const nsAttrValue::EnumTable* kKindTableInvalidValueDefault = &kKindTable[4];

class WindowDestroyObserver final : public nsIObserver
{
  NS_DECL_ISUPPORTS

public:
  explicit WindowDestroyObserver(HTMLTrackElement* aElement, uint64_t aWinID)
    : mTrackElement(aElement)
    , mInnerID(aWinID)
  {
    RegisterWindowDestroyObserver();
  }
  void RegisterWindowDestroyObserver()
  {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->AddObserver(this, "inner-window-destroyed", false);
    }
  }
  void UnRegisterWindowDestroyObserver()
  {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, "inner-window-destroyed");
    }
    mTrackElement = nullptr;
  }
  NS_IMETHODIMP Observe(nsISupports *aSubject, const char *aTopic, const char16_t *aData) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (strcmp(aTopic, "inner-window-destroyed") == 0) {
      nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
      NS_ENSURE_TRUE(wrapper, NS_ERROR_FAILURE);
      uint64_t innerID;
      nsresult rv = wrapper->GetData(&innerID);
      NS_ENSURE_SUCCESS(rv, rv);
      if (innerID == mInnerID) {
        if (mTrackElement) {
          mTrackElement->NotifyShutdown();
        }
        UnRegisterWindowDestroyObserver();
      }
    }
    return NS_OK;
  }

private:
  ~WindowDestroyObserver() {};
  HTMLTrackElement* mTrackElement;
  uint64_t mInnerID;
};
NS_IMPL_ISUPPORTS(WindowDestroyObserver, nsIObserver);

/** HTMLTrackElement */
HTMLTrackElement::HTMLTrackElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
  , mLoadResourceDispatched(false)
{
  nsISupports* parentObject = OwnerDoc()->GetParentObject();
  NS_ENSURE_TRUE_VOID(parentObject);
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(parentObject);
  mWindowDestroyObserver = new WindowDestroyObserver(this, window->WindowID());
}

HTMLTrackElement::~HTMLTrackElement()
{
  if (mWindowDestroyObserver) {
    mWindowDestroyObserver->UnRegisterWindowDestroyObserver();
  }
  NotifyShutdown();
}

NS_IMPL_ELEMENT_CLONE(HTMLTrackElement)

NS_IMPL_ADDREF_INHERITED(HTMLTrackElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLTrackElement, Element)

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLTrackElement, nsGenericHTMLElement,
                                   mTrack, mMediaParent, mListener)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(HTMLTrackElement)
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLElement)

void
HTMLTrackElement::GetKind(DOMString& aKind) const
{
  GetEnumAttr(nsGkAtoms::kind, kKindTable[0].tag, aKind);
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
HTMLTrackElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLTrackElementBinding::Wrap(aCx, this, aGivenProto);
}

bool
HTMLTrackElement::IsWebVTTEnabled()
{
  // Our callee does not use its arguments.
  return HTMLTrackElementBinding::ConstructorEnabled(nullptr, nullptr);
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

  nsISupports* parentObject =
    OwnerDoc()->GetParentObject();

  NS_ENSURE_TRUE_VOID(parentObject);

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(parentObject);
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
    return aResult.ParseEnumValue(aValue, kKindTable, false,
                                  kKindTableInvalidValueDefault);
  }

  // Otherwise call the generic implementation.
  return nsGenericHTMLElement::ParseAttribute(aNamespaceID,
                                              aAttribute,
                                              aValue,
                                              aResult);
}

void
HTMLTrackElement::SetSrc(const nsAString& aSrc, ErrorResult& aError)
{
  SetHTMLAttr(nsGkAtoms::src, aSrc, aError);
  uint16_t oldReadyState = ReadyState();
  SetReadyState(TextTrackReadyState::NotLoaded);
  if (!mMediaParent) {
    return;
  }
  if (mTrack && (oldReadyState != TextTrackReadyState::NotLoaded)) {
    // Remove all the cues in MediaElement.
    mMediaParent->RemoveTextTrack(mTrack);
    // Recreate mTrack.
    CreateTextTrack();
  }
  // Stop WebVTTListener.
  mListener = nullptr;
  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nullptr;
  }

  DispatchLoadResource();
}

void
HTMLTrackElement::DispatchLoadResource()
{
  if (!mLoadResourceDispatched) {
    RefPtr<Runnable> r = NewRunnableMethod(this, &HTMLTrackElement::LoadResource);
    nsContentUtils::RunInStableState(r.forget());
    mLoadResourceDispatched = true;
  }
}

void
HTMLTrackElement::LoadResource()
{
  mLoadResourceDispatched = false;

  // Find our 'src' url
  nsAutoString src;
  if (!GetAttr(kNameSpaceID_None, nsGkAtoms::src, src)) {
    return;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NewURIFromString(src, getter_AddRefs(uri));
  NS_ENSURE_TRUE_VOID(NS_SUCCEEDED(rv));
  LOG(LogLevel::Info, ("%p Trying to load from src=%s", this,
      NS_ConvertUTF16toUTF8(src).get()));

  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nullptr;
  }

  nsCOMPtr<nsIChannel> channel;
  nsCOMPtr<nsILoadGroup> loadGroup = OwnerDoc()->GetDocumentLoadGroup();
  rv = NS_NewChannel(getter_AddRefs(channel),
                     uri,
                     static_cast<Element*>(this),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS,
                     nsIContentPolicy::TYPE_INTERNAL_TRACK,
                     loadGroup,
                     nullptr,   // aCallbacks
                     nsIRequest::LOAD_NORMAL | nsIChannel::LOAD_CLASSIFY_URI);

  NS_ENSURE_TRUE_VOID(NS_SUCCEEDED(rv));

  mListener = new WebVTTListener(this);
  rv = mListener->LoadResource();
  NS_ENSURE_TRUE_VOID(NS_SUCCEEDED(rv));
  channel->SetNotificationCallbacks(mListener);

  LOG(LogLevel::Debug, ("opening webvtt channel"));
  rv = channel->AsyncOpen2(mListener);
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

  LOG(LogLevel::Debug, ("Track Element bound to tree."));
  if (!aParent || !aParent->IsNodeOfType(nsINode::eMEDIA)) {
    return NS_OK;
  }

  // Store our parent so we can look up its frame for display.
  if (!mMediaParent) {
    mMediaParent = static_cast<HTMLMediaElement*>(aParent);

    // TODO: separate notification for 'alternate' tracks?
    mMediaParent->NotifyAddedSource();
    LOG(LogLevel::Debug, ("Track element sent notification to parent."));

    // We may already have a TextTrack at this point if GetTrack() has already
    // been called. This happens, for instance, if script tries to get the
    // TextTrack before its mTrackElement has been bound to the DOM tree.
    if (!mTrack) {
      CreateTextTrack();
    }
    DispatchLoadResource();
  }

  return NS_OK;
}

void
HTMLTrackElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  if (mMediaParent && aNullParent) {
    // mTrack can be null if HTMLTrackElement::LoadResource has never been
    // called.
    if (mTrack) {
      mMediaParent->RemoveTextTrack(mTrack);
    }
    mMediaParent = nullptr;
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
  if (ReadyState() == aReadyState) {
    return;
  }

  if (mTrack) {
    switch (aReadyState) {
      case TextTrackReadyState::Loaded:
        DispatchTrackRunnable(NS_LITERAL_STRING("load"));
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
    NewRunnableMethod
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

void
HTMLTrackElement::DropChannel()
{
  mChannel = nullptr;
}

void
HTMLTrackElement::NotifyShutdown()
{
  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
  }
  mChannel = nullptr;
  mListener = nullptr;
}

} // namespace dom
} // namespace mozilla
