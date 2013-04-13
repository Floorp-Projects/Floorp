/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_HTMLAudioElement_h
#define mozilla_dom_HTMLAudioElement_h

#include "nsIDOMHTMLAudioElement.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/TypedArray.h"

typedef uint16_t nsMediaNetworkState;
typedef uint16_t nsMediaReadyState;

namespace mozilla {
namespace dom {

class HTMLAudioElement : public HTMLMediaElement,
                         public nsIDOMHTMLAudioElement
{
public:
  HTMLAudioElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~HTMLAudioElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  // nsIDOMHTMLMediaElement
  using HTMLMediaElement::GetPaused;
  NS_FORWARD_NSIDOMHTMLMEDIAELEMENT(HTMLMediaElement::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual nsresult SetAcceptHeader(nsIHttpChannel* aChannel);

  virtual nsIDOMNode* AsDOMNode() { return this; }

  // WebIDL

  static already_AddRefed<HTMLAudioElement> Audio(const GlobalObject& global,
                                                  ErrorResult& aRv);
  static already_AddRefed<HTMLAudioElement> Audio(const GlobalObject& global,
                                                  const nsAString& src,
                                                  ErrorResult& aRv);

  void MozSetup(uint32_t aChannels, uint32_t aRate, ErrorResult& aRv);

  uint32_t MozWriteAudio(const Float32Array& aData, ErrorResult& aRv)
  {
    return MozWriteAudio(aData.Data(), aData.Length(), aRv);
  }
  uint32_t MozWriteAudio(const Sequence<float>& aData, ErrorResult& aRv)
  {
    return MozWriteAudio(aData.Elements(), aData.Length(), aRv);
  }
  uint32_t MozWriteAudio(const float* aData, uint32_t aLength,
                         ErrorResult& aRv);

  uint64_t MozCurrentSampleOffset(ErrorResult& aRv);

protected:
  virtual JSObject* WrapNode(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLAudioElement_h
