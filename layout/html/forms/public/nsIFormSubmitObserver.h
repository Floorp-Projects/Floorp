/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


/**
 * MODULE NOTES:
 * @created  pollmann 6/21/1999
 * 
 */

#ifndef nsIFormSubmitObserver_h__
#define nsIFormSubmitObserver_h__

#include "nsISupports.h"
#include "prtypes.h"

class nsIContent;
class nsIDOMWindowInternal;
class nsIURI;

// {a6cf9106-15b3-11d2-932e-00805f8add32}
#define NS_IFORMSUBMITOBSERVER_IID      \
{ 0xa6cf9106, 0x15b3, 0x11d2, {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

#define NS_FORMSUBMIT_SUBJECT "formsubmit"
#define NS_FIRST_FORMSUBMIT_CATEGORY "firstformsubmit"
#define NS_PASSWORDMANAGER_CATEGORY "passwordmanager"

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
   *   @param cancelSubmit- outparam - cancels form submit if set to true
   */
  NS_IMETHOD Notify(nsIContent* formNode, nsIDOMWindowInternal* window, nsIURI* actionURL, PRBool* cancelSubmit) = 0;

};

#endif /* nsIFormSubmitObserver_h__ */

