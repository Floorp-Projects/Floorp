/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLVideoElement_h
#define mozilla_dom_HTMLVideoElement_h

#include "nsIDOMHTMLVideoElement.h"
#include "mozilla/dom/HTMLMediaElement.h"

namespace mozilla {
namespace dom {

class HTMLVideoElement : public HTMLMediaElement,
                         public nsIDOMHTMLVideoElement
{
public:
  HTMLVideoElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~HTMLVideoElement();

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLVideoElement, video)

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

  // nsIDOMHTMLVideoElement
  NS_DECL_NSIDOMHTMLVIDEOELEMENT

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // Set size with the current video frame's height and width.
  // If there is no video frame, returns NS_ERROR_FAILURE.
  nsresult GetVideoSize(nsIntSize* size);

  virtual nsresult SetAcceptHeader(nsIHttpChannel* aChannel);

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLVideoElement_h
