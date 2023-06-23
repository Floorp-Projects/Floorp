/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLAudioElement.h"
#include "mozilla/dom/HTMLAudioElementBinding.h"
#include "nsError.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "mozilla/dom/Document.h"
#include "jsfriendapi.h"
#include "nsContentUtils.h"
#include "nsJSUtils.h"
#include "AudioSampleFormat.h"
#include <algorithm>
#include "nsComponentManagerUtils.h"
#include "nsIHttpChannel.h"
#include "mozilla/dom/TimeRanges.h"
#include "AudioStream.h"

nsGenericHTMLElement* NS_NewHTMLAudioElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    mozilla::dom::FromParser aFromParser) {
  RefPtr<mozilla::dom::NodeInfo> nodeInfo(aNodeInfo);
  auto* nim = nodeInfo->NodeInfoManager();
  mozilla::dom::HTMLAudioElement* element =
      new (nim) mozilla::dom::HTMLAudioElement(nodeInfo.forget());
  element->Init();
  return element;
}

namespace mozilla::dom {

nsresult HTMLAudioElement::Clone(mozilla::dom::NodeInfo* aNodeInfo,
                                 nsINode** aResult) const {
  *aResult = nullptr;
  RefPtr<mozilla::dom::NodeInfo> ni(aNodeInfo);
  auto* nim = ni->NodeInfoManager();
  HTMLAudioElement* it = new (nim) HTMLAudioElement(ni.forget());
  it->Init();
  nsCOMPtr<nsINode> kungFuDeathGrip = it;
  nsresult rv = const_cast<HTMLAudioElement*>(this)->CopyInnerTo(it);
  if (NS_SUCCEEDED(rv)) {
    kungFuDeathGrip.swap(*aResult);
  }
  return rv;
}

HTMLAudioElement::HTMLAudioElement(already_AddRefed<NodeInfo>&& aNodeInfo)
    : HTMLMediaElement(std::move(aNodeInfo)) {
  DecoderDoctorLogger::LogConstruction(this);
}

HTMLAudioElement::~HTMLAudioElement() {
  DecoderDoctorLogger::LogDestruction(this);
}

bool HTMLAudioElement::IsInteractiveHTMLContent() const {
  return HasAttr(nsGkAtoms::controls) ||
         HTMLMediaElement::IsInteractiveHTMLContent();
}

already_AddRefed<HTMLAudioElement> HTMLAudioElement::Audio(
    const GlobalObject& aGlobal, const Optional<nsAString>& aSrc,
    ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(aGlobal.GetAsSupports());
  Document* doc;
  if (!win || !(doc = win->GetExtantDoc())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<mozilla::dom::NodeInfo> nodeInfo = doc->NodeInfoManager()->GetNodeInfo(
      nsGkAtoms::audio, nullptr, kNameSpaceID_XHTML, ELEMENT_NODE);

  RefPtr<HTMLAudioElement> audio =
      static_cast<HTMLAudioElement*>(NS_NewHTMLAudioElement(nodeInfo.forget()));
  audio->SetHTMLAttr(nsGkAtoms::preload, u"auto"_ns, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (aSrc.WasPassed()) {
    audio->SetSrc(aSrc.Value(), aRv);
  }

  return audio.forget();
}

nsresult HTMLAudioElement::SetAcceptHeader(nsIHttpChannel* aChannel) {
  nsAutoCString value(
      "audio/webm,"
      "audio/ogg,"
      "audio/wav,"
      "audio/*;q=0.9,"
      "application/ogg;q=0.7,"
      "video/*;q=0.6,*/*;q=0.5");

  return aChannel->SetRequestHeader("Accept"_ns, value, false);
}

JSObject* HTMLAudioElement::WrapNode(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return HTMLAudioElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
