/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#define       kMaxReportColumns   39

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
  NS_METHOD    GenerateHTMLOutputChannel(const char *aHtmlOutput,
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

  NS_METHOD    CheckColumnValidity(nsIAbCard *aCard);
  NS_METHOD    GenerateRowForCard(nsString           &aString, 
                                  nsIAbCard          *aCard);
  NS_METHOD    GenerateColumnHeadings(nsString           &aString);
  NS_METHOD    InitPrintColumns();

  // Member vars...
  PRInt32             mAddbookOperation;
  reportColumnStruct  *mReportColumns;
};

#endif /* nsAddbookProtocolHandler_h___ */
