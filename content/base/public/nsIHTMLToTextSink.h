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

#ifndef _nsIPlainTextSink_h__
#define _nsIPlainTextSink_h__

#include "nsISupports.h"
#include "nsAWritableString.h"

#define NS_PLAINTEXTSINK_CONTRACTID "@mozilla.org/layout/plaintextsink;1"

/* starting interface:    nsIContentSerializer */
#define NS_IHTMLTOTEXTSINK_IID_STR "b12b5643-07cb-401e-aabb-64b2dcd2717f"

#define NS_IHTMLTOTEXTSINK_IID \
  {0xb12b5643, 0x07cb, 0x401e, \
    { 0xaa, 0xbb, 0x64, 0xb2, 0xdc, 0xd2, 0x71, 0x7f }}


class nsIHTMLToTextSink : public nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IHTMLTOTEXTSINK_IID)

  NS_IMETHOD Initialize(nsAWritableString* aOutString,
                        PRUint32 aFlags, PRUint32 aWrapCol) = 0;
};

#endif
