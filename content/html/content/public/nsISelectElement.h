/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsISelectElement_h___
#define nsISelectElement_h___

#include "nsISupports.h"

// IID for the nsISelect interface
#define NS_ISELECTELEMENT_IID    \
{ 0xa6cf90f6, 0x15b3, 0x11d2,    \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

class nsIDOMHTMLOptionElement;

/** 
 * This interface is used to notify a SELECT when OPTION
 * elements are added and removed from its subtree.
 * Note that the nsIDOMHTMLSelectElement and nsIContent 
 * interfaces are the ones to use to access and enumerate
 * OPTIONs within a SELECT element.
 */
class nsISelectElement : public nsISupports {
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISELECTELEMENT_IID)

  /**
   * An OPTION element has been added to the SELECT's
   * subtree.
   */
  NS_IMETHOD AddOption(nsIContent* aContent) = 0;

  /**
   * An OPTION element has been deleted from the SELECT's
   * subtree.
   */
  NS_IMETHOD RemoveOption(nsIContent* aContent) = 0;

  /**
  * Indicates that we're done adding child content
  * to the select during document loading.
  */
  NS_IMETHOD DoneAddingContent(PRBool aIsDone) = 0;

  /**
  * Returns whether we're done adding child content
  * to the select during document loading.
  */
  NS_IMETHOD IsDoneAddingContent(PRBool * aIsDone) = 0;

  /**
  * Returns whether we're the option is selected
  */
  NS_IMETHOD IsOptionSelected(nsIDOMHTMLOptionElement* anOption, PRBool * aIsSelected) = 0;

  /**
  * Sets an option selected or delselected
  */
  NS_IMETHOD SetOptionSelected(nsIDOMHTMLOptionElement* anOption, PRBool aIsSelected) = 0;

};

#endif // nsISelectElement_h___

