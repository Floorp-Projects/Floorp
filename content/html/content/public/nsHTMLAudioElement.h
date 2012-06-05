/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(nsHTMLAudioElement_h__)
#define nsHTMLAudioElement_h__

#include "nsIDOMHTMLAudioElement.h"
#include "nsIJSNativeInitializer.h"
#include "nsHTMLMediaElement.h"

typedef PRUint16 nsMediaNetworkState;
typedef PRUint16 nsMediaReadyState;

class nsHTMLAudioElement : public nsHTMLMediaElement,
                           public nsIDOMHTMLAudioElement,
                           public nsIJSNativeInitializer
{
public:
  nsHTMLAudioElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLAudioElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsHTMLMediaElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsHTMLMediaElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsHTMLMediaElement::)

  // nsIDOMHTMLMediaElement
  NS_FORWARD_NSIDOMHTMLMEDIAELEMENT(nsHTMLMediaElement::)

  // nsIDOMHTMLAudioElement
  NS_DECL_NSIDOMHTMLAUDIOELEMENT

  // nsIJSNativeInitializer
  NS_IMETHOD Initialize(nsISupports* aOwner, JSContext* aContext,
                        JSObject* aObj, PRUint32 argc, jsval* argv);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual nsresult SetAcceptHeader(nsIHttpChannel* aChannel);

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

protected:
  virtual void GetItemValueText(nsAString& text);
  virtual void SetItemValueText(const nsAString& text);
};

#endif
