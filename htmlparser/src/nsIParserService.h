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

#include "nsISupports.h"
#include "nsString.h"

#define NS_IPARSERSERVICE_IID                           \
{ 0xa6cf9111, 0x15b3, 0x11d2,                           \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }


class nsIParserService : public nsISupports {
 public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPARSERSERVICE_IID)

  NS_IMETHOD HTMLStringTagToId(const nsString &aTag, PRInt32* aId) const =0;

  NS_IMETHOD HTMLIdToStringTag(PRInt32 aId, nsString& aTag) const =0;
  
  NS_IMETHOD HTMLConvertEntityToUnicode(const nsString& aEntity, 
                                        PRInt32* aUnicode) const =0;

  NS_IMETHOD HTMLConvertUnicodeToEntity(PRInt32 aUnicode,
                                        nsCString& aEntity) const =0;

  NS_IMETHOD IsContainer(nsString& aTag, PRBool& aIsContainer) const =0;
  NS_IMETHOD IsBlock(nsString& aTag, PRBool& aIsBlock) const =0;
};

