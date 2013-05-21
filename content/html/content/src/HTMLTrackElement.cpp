/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/HTMLTrackElement.h"
#include "mozilla/dom/HTMLTrackElementBinding.h"
#include "mozilla/dom/HTMLUnknownElement.h"
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
#include "nsIFrame.h"
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
#include "webvtt/parser.h"

#ifdef PR_LOGGING
static PRLogModuleInfo* gTrackElementLog;
#define LOG(type, msg) PR_LOG(gTrackElementLog, type, msg)
#else
#define LOG(type, msg)
#endif

// Replace the usual NS_IMPL_NS_NEW_HTML_ELEMENT(Track) so
// we can return an UnknownElement instead when pref'd off.
nsGenericHTMLElement*
NS_NewHTMLTrackElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                       mozilla::dom::FromParser aFromParser)
{
  if (!mozilla::dom::HTMLTrackElement::IsWebVTTEnabled()) {
    return mozilla::dom::NewHTMLElementHelper::Create<nsHTMLUnknownElement,
           mozilla::dom::HTMLUnknownElement>(aNodeInfo);
  }

  return mozilla::dom::NewHTMLElementHelper::Create<nsHTMLTrackElement,
         mozilla::dom::HTMLTrackElement>(aNodeInfo);
}

namespace mozilla {
namespace dom {

/** HTMLTrackElement */
HTMLTrackElement::HTMLTrackElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
  , mReadyState(NONE)
{
#ifdef PR_LOGGING
  if (!gTrackElementLog) {
    gTrackElementLog = PR_NewLogModule("nsTrackElement");
  }
#endif

  SetIsDOMBinding();
}

HTMLTrackElement::~HTMLTrackElement()
{
}

NS_IMPL_ELEMENT_CLONE(HTMLTrackElement)

NS_IMPL_ADDREF_INHERITED(HTMLTrackElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLTrackElement, Element)

NS_IMPL_CYCLE_COLLECTION_INHERITED_3(HTMLTrackElement, nsGenericHTMLElement,
                                     mTrack, mChannel, mMediaParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(HTMLTrackElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLElement)
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLElement)

JSObject*
HTMLTrackElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return HTMLTrackElementBinding::Wrap(aCx, aScope, this);
}

bool
HTMLTrackElement::IsWebVTTEnabled()
{
  return HTMLTrackElementBinding::PrefEnabled();
}

TextTrack*
HTMLTrackElement::Track()
{
  if (!mTrack) {
    // We're expected to always have an internal TextTrack so create
    // an empty object to return if we don't already have one.
    mTrack = new TextTrack(OwnerDoc()->GetParentObject());
  }

  return mTrack;
}

void
HTMLTrackElement::DisplayCueText(webvtt_node* head)
{
  // TODO: Bug 833382 - Propagate to the LoadListener.
}

void
HTMLTrackElement::CreateTextTrack()
{
  DOMString label, srcLang;
  GetSrclang(srcLang);
  GetLabel(label);
  mTrack = new TextTrack(OwnerDoc()->GetParentObject(), Kind(), label, srcLang);

  if (mMediaParent) {
    mMediaParent->AddTextTrack(mTrack);
  }
}

TextTrackKind
HTMLTrackElement::Kind() const
{
  const nsAttrValue* value = GetParsedAttr(nsGkAtoms::kind);
  if (!value) {
    return TextTrackKind::Subtitles;
  }
  return static_cast<TextTrackKind>(value->GetEnumValue());
}

static EnumEntry
StringFromKind(TextTrackKind aKind)
{
  return TextTrackKindValues::strings[static_cast<int>(aKind)];
}

void
HTMLTrackElement::SetKind(TextTrackKind aKind, ErrorResult& aError)
{
  const EnumEntry& string = StringFromKind(aKind);
  nsAutoString kind;

  kind.AssignASCII(string.value, string.length);
  SetHTMLAttr(nsGkAtoms::kind, kind, aError);
}

bool
HTMLTrackElement::ParseAttribute(int32_t aNamespaceID,
                                 nsIAtom* aAttribute,
                                 const nsAString& aValue,
                                 nsAttrValue& aResult)
{
  // Map html attribute string values to TextTrackKind enums.
  static const nsAttrValue::EnumTable kKindTable[] = {
    { "subtitles", static_cast<int16_t>(TextTrackKind::Subtitles) },
    { "captions", static_cast<int16_t>(TextTrackKind::Captions) },
    { "descriptions", static_cast<int16_t>(TextTrackKind::Descriptions) },
    { "chapters", static_cast<int16_t>(TextTrackKind::Chapters) },
    { "metadata", static_cast<int16_t>(TextTrackKind::Metadata) },
    { 0 }
  };

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

    // TODO: this section needs to become async in bug 833382.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=833385#c55.

    // Find our 'src' url
    nsAutoString src;

    // TODO: we might want to instead call LoadResource() in a
    // AfterSetAttr, like we do in media element.
    if (GetAttr(kNameSpaceID_None, nsGkAtoms::src, src)) {
      nsCOMPtr<nsIURI> uri;
      nsresult rvTwo = NewURIFromString(src, getter_AddRefs(uri));
      if (NS_SUCCEEDED(rvTwo)) {
        LOG(PR_LOG_ALWAYS, ("%p Trying to load from src=%s", this,
        NS_ConvertUTF16toUTF8(src).get()));
        // TODO: bug 833382 - dispatch a load request.
      }
    }
  }

  return NS_OK;
}

void
HTMLTrackElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  if (mMediaParent && aNullParent) {
    mMediaParent = nullptr;
  }

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

} // namespace dom
} // namespace mozilla
