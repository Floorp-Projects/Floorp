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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsCollationCID.h"
#include "nsCollationUnix.h"
#include "nsDateTimeFormatCID.h"
#include "nsDateTimeFormatUnix.h"
#include "nsIComponentManager.h"
#include "nsIGenericFactory.h"
#include "nsILocaleService.h"
#include "nsIScriptableDateFormat.h"
#include "nsIServiceManager.h"
#include "nsLanguageAtomService.h"
#include "nsLocaleCID.h"
#include "nsLocaleSO.h"
#include "nsPosixLocale.h"

#define MAKE_CTOR(ctor_, iface_, func_)                   \
static NS_IMETHODIMP                                      \
ctor_(nsISupports* aOuter, REFNSIID aIID, void** aResult) \
{                                                         \
  *aResult = nsnull;                                      \
  if (aOuter)                                             \
    return NS_ERROR_NO_AGGREGATION;                       \
  iface_* inst;                                           \
  nsresult rv = func_(&inst);                             \
  if (NS_SUCCEEDED(rv)) {                                 \
    rv = inst->QueryInterface(aIID, aResult);             \
    NS_RELEASE(inst);                                     \
  }                                                       \
  return rv;                                              \
}


MAKE_CTOR(CreateLocaleService, nsILocaleService, NS_NewLocaleService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPosixLocale)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCollationFactory)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCollationUnix)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDateTimeFormatUnix)
//NS_GENERIC_FACTORY_CONSTRUCTOR(nsScriptableDateTimeFormat)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLanguageAtomService)

// The list of components we register
static nsModuleComponentInfo gComponents[] = {
  { "nsLocaleService component",
    NS_LOCALESERVICE_CID,
    NS_LOCALESERVICE_CONTRACTID,
    CreateLocaleService },

  { "Platform locale",
    NS_POSIXLOCALE_CID,
    NS_POSIXLOCALE_CONTRACTID,
    nsPosixLocaleConstructor },

  { "Collation factory",
    NS_COLLATIONFACTORY_CID,
    NULL,
    nsCollationFactoryConstructor },

  { "Collation",
    NS_COLLATION_CID,
    NULL,
    nsCollationUnixConstructor },

  { "Date/Time formatter",
    NS_DATETIMEFORMAT_CID,
    NULL,
    nsDateTimeFormatUnixConstructor },

  { "Scriptable Date Format",
    NS_SCRIPTABLEDATEFORMAT_CID,
    NS_SCRIPTABLEDATEFORMAT_CONTRACTID,
    NS_NewScriptableDateFormat },

  { "Language Atom Service",
    NS_LANGUAGEATOMSERVICE_CID,
    NS_LANGUAGEATOMSERVICE_CONTRACTID,
    nsLanguageAtomServiceConstructor },
};

NS_IMPL_NSGETMODULE(nsLocaleModule, gComponents)
