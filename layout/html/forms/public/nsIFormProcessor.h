/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


/**
 * MODULE NOTES:
 * @created  kmcclusk 10/19/99
 * 
 */

#ifndef nsIFormProcessor_h__
#define nsIFormProcessor_h__

#include "nsISupports.h"
#include "prtypes.h"

#include "nsIDOMHTMLInputElement.h"

class nsString;
class nsVoidArray;

// {7E4BB410-8740-11d3-967F-00105A1B1B76}
#define NS_FORMPROCESSOR_CID \
{ 0x7e4bb410, 0x8740, 0x11d3, { 0x96, 0x7f, 0x0, 0x10, 0x5a, 0x1b, 0x1b, 0x76 } }

#define NS_FORMPROCESSOR_CONTRACTID "@mozilla.org/layout/form-processor;1"

// F11C1E20-873F-11d3-967F-00105A1B1B76
#define NS_IFORMPROCESSOR_IID      \
{ 0xf11c1e20, 0x873f, 0x11d3, { 0x96, 0x7f, 0x0, 0x10, 0x5a, 0x1b, 0x1b, 0x76 } }


// XXX:In the future, we should examine combining this interface with nsIFormSubmitObserver.
// nsIFormSubmitObserver could have a before, during, and after form submission methods.
// The before and after methods would be passed a nsISupports interface. The During would
// Have the same method signature as ProcessValue.


class nsIFormProcessor : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFORMPROCESSOR_IID)

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
  NS_IMETHOD ProcessValue(nsIDOMHTMLElement *aElement, 
                          const nsString& aName, 
                          nsString& aValue) = 0;

  /* Provide content for a form element. This method provides a mechanism to provide 
   * content which comes from a source other than the document (i.e. a local database)
   *  
   *   @param aFormType   Type of form to get content for.
   *   @param aOptions    List of nsStrings which define the contents for the form element
   *   @param aAttr       Attribute to be attached to the form element. It is used to identify
   *                      the form element contains non-standard content.
   */

  NS_IMETHOD ProvideContent(const nsString& aFormType, 
                            nsVoidArray& aContent,
                            nsString& aAttribute) = 0;

};

#endif /* nsIFormProcessor_h__ */

