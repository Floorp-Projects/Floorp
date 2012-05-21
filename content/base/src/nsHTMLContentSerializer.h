/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsIContentSerializer implementation that can be used with an
 * nsIDocumentEncoder to convert an HTML (not XHTML!) DOM to an HTML
 * string that could be parsed into more or less the original DOM.
 */

#ifndef nsHTMLContentSerializer_h__
#define nsHTMLContentSerializer_h__

#include "nsXHTMLContentSerializer.h"
#include "nsIEntityConverter.h"
#include "nsString.h"

class nsIContent;
class nsIAtom;

class nsHTMLContentSerializer : public nsXHTMLContentSerializer {
 public:
  nsHTMLContentSerializer();
  virtual ~nsHTMLContentSerializer();

  NS_IMETHOD AppendElementStart(mozilla::dom::Element* aElement,
                                mozilla::dom::Element* aOriginalElement,
                                nsAString& aStr);

  NS_IMETHOD AppendElementEnd(mozilla::dom::Element* aElement,
                              nsAString& aStr);

  NS_IMETHOD AppendDocumentStart(nsIDocument *aDocument,
                                 nsAString& aStr);
 protected:

  virtual void SerializeHTMLAttributes(nsIContent* aContent,
                                       nsIContent *aOriginalElement,
                                       nsAString& aTagPrefix,
                                       const nsAString& aTagNamespaceURI,
                                       nsIAtom* aTagName,
                                       PRInt32 aNamespace,
                                       nsAString& aStr);

  virtual void AppendAndTranslateEntities(const nsAString& aStr,
                                          nsAString& aOutputStr);

};

nsresult
NS_NewHTMLContentSerializer(nsIContentSerializer** aSerializer);

#endif
