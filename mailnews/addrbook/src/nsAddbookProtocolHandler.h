/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsAddbookProtocolHandler_h___
#define nsAddbookProtocolHandler_h___

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsAddbookProtocolHandler.h"
#include "nsIProtocolHandler.h"
#include "nsIAddbookUrl.h"
#include "nsIAddrDatabase.h"

typedef struct {
  const char  *abField;
  PRBool      includeIt;
} reportColumnStruct;

class nsAddbookProtocolHandler : public nsIProtocolHandler
{
public:
	nsAddbookProtocolHandler();
	virtual ~nsAddbookProtocolHandler();

  NS_DECL_ISUPPORTS

  static NS_METHOD    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

	//////////////////////////////////////////////////////////////////////////
	// we suppport the nsIProtocolHandler interface 
	//////////////////////////////////////////////////////////////////////////
  NS_DECL_NSIPROTOCOLHANDLER

private:
  // Output Generation calls to create the HTML output 
  NS_METHOD    GenerateHTMLOutputChannel(char *aHtmlOutput,
                                         PRInt32  aHtmlOutputSize,
                                         nsIAddbookUrl *addbookUrl,
                                         nsIURI *aURI, 
                                         nsIChannel **_retval);

  NS_METHOD    GeneratePrintOutput(nsIAddbookUrl *addbookUrl, 
                                   char          **outBuf);
  NS_METHOD    BuildSingleHTML(nsIAddrDatabase *aDatabase, nsIAbDirectory *directory, 
                               char *charEmail, nsString &workBuffer);

  NS_METHOD    BuildAllHTML(nsIAddrDatabase *aDatabase, nsIAbDirectory *directory, 
                            nsString &workBuffer);

  NS_METHOD    AddIndividualUserAttribPair(nsString &aString, const char *aColumn, nsIAbCard *workCard);

  // Database access calls...
  NS_METHOD    OpenAB(char *aAbName, nsIAddrDatabase **aDatabase);
  NS_METHOD    FindPossibleAbName(nsIAbCard  *aCard,
                                  PRUnichar  **retName);

  // Member vars...
  PRInt32      mAddbookOperation;
};

#endif /* nsAddbookProtocolHandler_h___ */
