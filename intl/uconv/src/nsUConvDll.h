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

#ifndef nsUConvDll_h___
#define nsUConvDll_h___

#include "nsISupports.h"
class nsIComponentManager;
class nsIFile;
struct nsModuleComponentInfo;

// Factory methods

NS_IMETHODIMP
NS_NewCharsetConverterManager(nsISupports* aOuter, const nsIID& aIID,
                              void** aResult);

NS_IMETHODIMP
NS_NewUnicodeDecodeHelper(nsISupports* aOuter, const nsIID& aIID,
                          void** aResult);

NS_IMETHODIMP
NS_NewUnicodeEncodeHelper(nsISupports* aOuter, const nsIID& aIID,
                          void** aResult);

NS_IMETHODIMP
NS_NewPlatformCharset(nsISupports* aOuter, const nsIID& aIID,
                      void** aResult);

NS_IMETHODIMP
NS_NewCharsetAlias(nsISupports* aOuter, const nsIID& aIID,
                   void** aResult);

NS_IMETHODIMP
NS_NewCharsetMenu(nsISupports* aOuter, const nsIID& aIID,
                  void** aResult);

NS_IMETHODIMP
NS_NewTextToSubURI(nsISupports* aOuter, const nsIID& aIID,
                   void** aResult);

NS_IMETHODIMP
NS_RegisterConverterManagerData(nsIComponentManager* aCompMgr,
                                nsIFile* aPath,
                                const char *aLocation,
                                const char *aType,
                                const nsModuleComponentInfo* aInfo);

NS_IMETHODIMP
NS_UnregisterConverterManagerData(nsIComponentManager* aCompMgr,
                                  nsIFile* aPath,
                                  const char* aRegistryLocation,
                                  const nsModuleComponentInfo* aInfo);

NS_IMETHODIMP
NS_NewISO88591ToUnicode(nsISupports* aOuter, const nsIID& aIID,
                   void** aResult);

NS_IMETHODIMP
NS_NewCP1252ToUnicode(nsISupports* aOuter, const nsIID& aIID,
                   void** aResult);

NS_IMETHODIMP
NS_NewMacRomanToUnicode(nsISupports* aOuter, const nsIID& aIID,
                   void** aResult);

NS_IMETHODIMP
NS_NewUTF8ToUnicode(nsISupports* aOuter, const nsIID& aIID,
                   void** aResult);

NS_IMETHODIMP
NS_NewUnicodeToISO88591(nsISupports* aOuter, const nsIID& aIID,
                   void** aResult);

NS_IMETHODIMP
NS_NewUnicodeToCP1252(nsISupports* aOuter, const nsIID& aIID,
                   void** aResult);

NS_IMETHODIMP
NS_NewUnicodeToMacRoman(nsISupports* aOuter, const nsIID& aIID,
                   void** aResult);

NS_IMETHODIMP
NS_NewUnicodeToUTF8(nsISupports* aOuter, const nsIID& aIID,
                   void** aResult);

#endif /* nsUConvDll_h___ */
