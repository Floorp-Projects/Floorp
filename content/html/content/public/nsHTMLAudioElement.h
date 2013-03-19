/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(nsHTMLAudioElement_h__)
#define nsHTMLAudioElement_h__

#include "nsIDOMHTMLAudioElement.h"
#include "nsIJSNativeInitializer.h"
#include "mozilla/dom/HTMLMediaElement.h"

typedef uint16_t nsMediaNetworkState;
typedef uint16_t nsMediaReadyState;

class nsHTMLAudioElement : public mozilla::dom::HTMLMediaElement,
                           public nsIDOMHTMLAudioElement,
                           public nsIJSNativeInitializer
{
public:
  nsHTMLAudioElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLAudioElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  // nsIDOMHTMLMediaElement
  using mozilla::dom::HTMLMediaElement::GetPaused;
  NS_FORWARD_NSIDOMHTMLMEDIAELEMENT(mozilla::dom::HTMLMediaElement::)

  // nsIDOMHTMLAudioElement
  NS_DECL_NSIDOMHTMLAUDIOELEMENT

  // nsIJSNativeInitializer
  NS_IMETHOD Initialize(nsISupports* aOwner, JSContext* aContext,
                        JSObject* aObj, uint32_t argc, jsval* argv);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual nsresult SetAcceptHeader(nsIHttpChannel* aChannel);

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};

#endif
