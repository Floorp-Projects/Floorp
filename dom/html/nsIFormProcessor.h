/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 * MODULE NOTES:
 * @created  kmcclusk 10/19/99
 * 
 */

#ifndef nsIFormProcessor_h__
#define nsIFormProcessor_h__

#include "nsISupports.h"
#include "nsTArrayForwardDeclare.h"

class nsString;
class nsIDOMHTMLElement;

// {0ae53c0f-8ea2-4916-bedc-717443c3e185}
#define NS_FORMPROCESSOR_CID \
{ 0x0ae53c0f, 0x8ea2, 0x4916, { 0xbe, 0xdc, 0x71, 0x74, 0x43, 0xc3, 0xe1, 0x85 } }

#define NS_FORMPROCESSOR_CONTRACTID "@mozilla.org/layout/form-processor;1"

// bf8b1986-8800-424b-b1e5-7a2ca8b9e76c
#define NS_IFORMPROCESSOR_IID      \
{ 0xbf8b1986, 0x8800, 0x424b, \
  { 0xb1, 0xe5, 0x7a, 0x2c, 0xa8, 0xb9, 0xe7, 0x6c } }

// XXX:In the future, we should examine combining this interface with nsIFormSubmitObserver.
// nsIFormSubmitObserver could have a before, during, and after form submission methods.
// The before and after methods would be passed a nsISupports interface. The During would
// Have the same method signature as ProcessValue.


class nsIFormProcessor : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFORMPROCESSOR_IID)

  /* ProcessValue is called for each name value pair that is 
   * about to be submitted for both "get" and "post" form submissions.
   *
   * The formprocessor is registered as a service that gets called for
   * every form submission.
   *
   *   @param aElement element which the attribute/value pair is submitted for
   *   @param aName    value of the form element name attribute about to be submitted
   *   @param aValue   On entry it contains the value about to be submitted for aName. 
   *                   On exit it contains the value which will actually be submitted for aName.
   *                   
   */
  virtual nsresult ProcessValue(nsIDOMHTMLElement* aElement,
                                const nsAString& aName,
                                nsAString& aValue) = 0;

  /**
   * The same as above, but with the element unpacked so that this can be
   * called as the result of an IPC message.
   */
  virtual nsresult ProcessValueIPC(const nsAString& aOldValue,
                                   const nsAString& aKeyType,
                                   const nsAString& aChallenge,
                                   const nsAString& aKeyParams,
                                   nsAString& newValue) = 0;

  /* Provide content for a form element. This method provides a mechanism to provide 
   * content which comes from a source other than the document (i.e. a local database)
   *  
   *   @param aFormType   Type of form to get content for.
   *   @param aOptions    List of nsStrings which define the contents for the form element
   *   @param aAttr       Attribute to be attached to the form element. It is used to identify
   *                      the form element contains non-standard content.
   */

  virtual nsresult ProvideContent(const nsAString& aFormType,
                                  nsTArray<nsString>& aContent,
                                  nsAString& aAttribute) = 0;

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIFormProcessor, NS_IFORMPROCESSOR_IID)

#endif /* nsIFormProcessor_h__ */

