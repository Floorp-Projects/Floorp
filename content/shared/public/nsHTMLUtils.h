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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*

  Some silly extra stuff that doesn't have anywhere better to go

*/

#ifndef nsHTMLUtils_h__
#define nsHTMLUtils_h__

class nsICharsetConverterManager;
class nsIDocument;
class nsIIOService;
class nsIURI;
class nsString;

/**
 * A version of NS_MakeAbsoluteURI that's savvy to document character
 * set encodings, and will recode a relative spec in the specified
 * charset and URL-escape it before resolving.
 */
nsresult
NS_MakeAbsoluteURIWithCharset(char* *aResult,
                              const nsString& aSpec,
                              nsIDocument* aDocument,
                              nsIURI* aBaseURI = nsnull,
                              nsIIOService* aIOService = nsnull,
                              nsICharsetConverterManager* aConvMgr = nsnull);


class nsHTMLUtils {
public:
  static void AddRef();
  static void Release();

  static nsIIOService* IOService;
  static nsICharsetConverterManager* CharsetMgr;

protected:
  static PRInt32 gRefCnt;
};

#endif
