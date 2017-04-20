/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_HTMLAudioElement_h
#define mozilla_dom_HTMLAudioElement_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/TypedArray.h"

typedef uint16_t nsMediaNetworkState;
typedef uint16_t nsMediaReadyState;

namespace mozilla {
namespace dom {

class HTMLAudioElement final : public HTMLMediaElement
{
public:
  typedef mozilla::dom::NodeInfo NodeInfo;

  explicit HTMLAudioElement(already_AddRefed<NodeInfo>& aNodeInfo);

  // Element
  virtual bool IsInteractiveHTMLContent(bool aIgnoreTabindex) const override;

  // nsIDOMHTMLMediaElement
  using HTMLMediaElement::GetPaused;

  virtual nsresult Clone(NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;
  virtual nsresult SetAcceptHeader(nsIHttpChannel* aChannel) override;

  virtual nsIDOMNode* AsDOMNode() override { return this; }

  // WebIDL

  static already_AddRefed<HTMLAudioElement>
  Audio(const GlobalObject& aGlobal,
        const Optional<nsAString>& aSrc, ErrorResult& aRv);

protected:
  virtual ~HTMLAudioElement();

  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLAudioElement_h
