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

#ifndef nsParserUtils_h__
#define nsParserUtils_h__

#include "nsString.h"

class nsParserUtils {
public:
  static nsresult
  GetQuotedAttributeValue(nsString& aSource,
                          const nsString& aAttribute,
                          nsString& aValue);

  static PRBool
  IsJavaScriptLanguage(const nsString& aName, const char* *aVersion);
};

#endif // nsParserUtils_h__



