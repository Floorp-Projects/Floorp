/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(nsHTMLVideoElement_h__)
#define nsHTMLVideoElement_h__

#include "nsIDOMHTMLVideoElement.h"
#include "nsHTMLMediaElement.h"

class nsHTMLVideoElement : public nsHTMLMediaElement,
                           public nsIDOMHTMLVideoElement
{
public:
  nsHTMLVideoElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLVideoElement();

  static nsHTMLVideoElement* FromContent(nsIContent* aPossibleVideo)
  {
    if (!aPossibleVideo || !aPossibleVideo->IsHTML(nsGkAtoms::video)) {
      return NULL;
    }
    return static_cast<nsHTMLVideoElement*>(aPossibleVideo);
  }

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

  // nsIDOMHTMLVideoElement
  NS_DECL_NSIDOMHTMLVIDEOELEMENT

  virtual bool ParseAttribute(PRInt32 aNamespaceID,
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

#endif
