/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLAudioElement.h"
#include "mozilla/dom/HTMLAudioElementBinding.h"
#include "nsError.h"
#include "nsIDOMHTMLAudioElement.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsIDocument.h"
#include "jsfriendapi.h"
#include "nsContentUtils.h"
#include "nsJSUtils.h"
#include "AudioSampleFormat.h"
#include "AudioChannelCommon.h"
#include <algorithm>
#include "nsComponentManagerUtils.h"
#include "nsIHttpChannel.h"
#include "mozilla/dom/TimeRanges.h"
#include "AudioStream.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Audio)

namespace mozilla {
namespace dom {

extern bool IsAudioAPIEnabled();

NS_IMPL_ISUPPORTS_INHERITED(HTMLAudioElement, HTMLMediaElement,
                            nsIDOMHTMLMediaElement, nsIDOMHTMLAudioElement)

NS_IMPL_ELEMENT_CLONE(HTMLAudioElement)


HTMLAudioElement::HTMLAudioElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
  : HTMLMediaElement(aNodeInfo)
{
}

HTMLAudioElement::~HTMLAudioElement()
{
}


already_AddRefed<HTMLAudioElement>
HTMLAudioElement::Audio(const GlobalObject& aGlobal,
                        const Optional<nsAString>& aSrc,
                        ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aGlobal.GetAsSupports());
  nsIDocument* doc;
  if (!win || !(doc = win->GetExtantDoc())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  already_AddRefed<nsINodeInfo> nodeInfo =
    doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::audio, nullptr,
                                        kNameSpaceID_XHTML,
                                        nsIDOMNode::ELEMENT_NODE);

  nsRefPtr<HTMLAudioElement> audio = new HTMLAudioElement(nodeInfo);
  audio->SetHTMLAttr(nsGkAtoms::preload, NS_LITERAL_STRING("auto"), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (aSrc.WasPassed()) {
    aRv = audio->SetSrc(aSrc.Value());
  }

  return audio.forget();
}

nsresult HTMLAudioElement::SetAcceptHeader(nsIHttpChannel* aChannel)
{
    nsAutoCString value(
#ifdef MOZ_WEBM
      "audio/webm,"
#endif
      "audio/ogg,"
#ifdef MOZ_WAVE
      "audio/wav,"
#endif
      "audio/*;q=0.9,"
      "application/ogg;q=0.7,"
      "video/*;q=0.6,*/*;q=0.5");

    return aChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                      value,
                                      false);
}

JSObject*
HTMLAudioElement::WrapNode(JSContext* aCx)
{
  return HTMLAudioElementBinding::Wrap(aCx, this);
}

} // namespace dom
} // namespace mozilla
