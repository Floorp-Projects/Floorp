/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
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
   */
  NS_IMETHOD Notify(nsIContent* formNode) = 0;


};

#endif /* nsIFormSubmitObserver_h__ */

