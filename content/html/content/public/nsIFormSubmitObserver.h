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
 * @created  pollmann 6/21/1999
 * 
 */

#ifndef nsIFormSubmitObserver_h__
#define nsIFormSubmitObserver_h__

#include "nsISupports.h"
#include "prtypes.h"
#include "nsIDOMWindowInternal.h"
#include "nsIURI.h"

class nsString;

// {a6cf9106-15b3-11d2-932e-00805f8add32}
#define NS_IFORMSUBMITOBSERVER_IID      \
{ 0xa6cf9106, 0x15b3, 0x11d2, {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

#define NS_FORMSUBMIT_SUBJECT "formsubmit"

class nsIFormSubmitObserver : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFORMSUBMITOBSERVER_IID)

  /*
   *   Subject calls the observer when the form is submitted
   *   @param formNode- the dom node corresonding to this form.
   *   @param window- the window that the form submit occured in.
   *                  NOTE: This is not necessarily the same window the form submit result
   *                        will be loaded in (form could have target attribute set)
   *   @param actionURL- URL to which the form will be submitted.
   */
  NS_IMETHOD Notify(nsIContent* formNode, nsIDOMWindowInternal* window, nsIURI* actionURL) = 0;


};

#endif /* nsIFormSubmitObserver_h__ */

