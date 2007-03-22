/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsIFormSubmission_h___
#define nsIFormSubmission_h___

#include "nsISupports.h"
class nsAString;
class nsACString;
class nsIURI;
class nsIInputStream;
class nsGenericHTMLElement;
class nsILinkHandler;
class nsIContent;
class nsIFormControl;
class nsIDOMHTMLElement;
class nsIDocShell;
class nsIRequest;

#define NS_IFORMSUBMISSION_IID   \
{ 0x7ee38e3a, 0x1dd2, 0x11b2, \
  {0x89, 0x6f, 0xab, 0x28, 0x03, 0x96, 0x25, 0xa9} }

/**
 * Interface for form submissions; encompasses the function to call to submit as
 * well as the form submission name/value pairs
 */
class nsIFormSubmission : public nsISupports
{
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFORMSUBMISSION_IID)

  /**
   * Find out whether or not this form submission accepts files
   *
   * @param aAcceptsFiles the boolean output
   */
  virtual PRBool AcceptsFiles() const = 0;

  /**
   * Call to perform the submission
   *
   * @param aActionURL the URL to submit to (may be modified with GET contents)
   * @param aTarget the target window
   * @param aSource the element responsible for the submission (for web shell)
   * @param aLinkHandler the link handler to use
   * @param aDocShell (out param) the DocShell in which the submission was
   *        loaded
   * @param aRequest (out param) the Request for the submission
   */
  virtual nsresult SubmitTo(nsIURI* aActionURL, const nsAString& aTarget,
                            nsIContent* aSource, nsILinkHandler* aLinkHandler,
                            nsIDocShell** aDocShell,
                            nsIRequest** aRequest) = 0;

  /**
   * Submit a name/value pair
   *
   * @param aSource the control sending the parameter
   * @param aName the name of the parameter
   * @param aValue the value of the parameter
   */
  virtual nsresult AddNameValuePair(nsIDOMHTMLElement* aSource,
                                    const nsAString& aName,
                                    const nsAString& aValue) = 0;

  /**
   * Submit a name/file pair
   *
   * @param aSource the control sending the parameter
   * @param aName the name of the parameter
   * @param aFilename the name of the file (pass null to provide no name)
   * @param aStream the stream containing the file data to be sent
   * @param aContentType the content-type of the file data being sent
   * @param aMoreFilesToCome true if another name/file pair with the same name
   *        will be sent soon
   */
  virtual nsresult AddNameFilePair(nsIDOMHTMLElement* aSource,
                               const nsAString& aName,
                               const nsAString& aFilename,
                               nsIInputStream* aStream,
                               const nsACString& aContentType,
                               PRBool aMoreFilesToCome) = 0;

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIFormSubmission, NS_IFORMSUBMISSION_IID)

//
// Factory methods
// 

/**
 * Get a submission object based on attributes in the form (ENCTYPE and METHOD)
 *
 * @param aForm the form to get a submission object based on
 * @param aFormSubmission the form submission object (out param)
 */
nsresult GetSubmissionFromForm(nsGenericHTMLElement* aForm,
                               nsIFormSubmission** aFormSubmission);


#endif /* nsIFormSubmission_h___ */
