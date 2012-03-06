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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#ifndef nsLocaleConstructors_h__
#define nsLocaleConstructors_h__

#include "nsCollationCID.h"
#include "nsDateTimeFormatCID.h"
#include "mozilla/ModuleUtils.h"
#include "nsILocaleService.h"
#include "nsIScriptableDateFormat.h"
#include "nsIServiceManager.h"
#include "nsLanguageAtomService.h"
#include "nsPlatformCharset.h"
#include "nsLocaleCID.h"

#if defined(XP_MACOSX)
#define USE_MAC_LOCALE
#endif

#if defined(XP_UNIX) && !defined(XP_MACOSX)
#define USE_UNIX_LOCALE
#endif

#ifdef XP_WIN
#include "nsCollationWin.h"
#include "nsDateTimeFormatWin.h"
#endif

#ifdef XP_OS2
#include "nsOS2Locale.h"
#include "nsCollationOS2.h"
#include "nsDateTimeFormatOS2.h"
#endif

#ifdef USE_MAC_LOCALE
#include "nsCollationMacUC.h"
#include "nsDateTimeFormatMac.h"
#endif

#ifdef USE_UNIX_LOCALE
#include "nsCollationUnix.h"
#include "nsDateTimeFormatUnix.h"
#endif

#define NSLOCALE_MAKE_CTOR(ctor_, iface_, func_)          \
static nsresult                                           \
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


NSLOCALE_MAKE_CTOR(CreateLocaleService, nsILocaleService, NS_NewLocaleService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCollationFactory)
//NS_GENERIC_FACTORY_CONSTRUCTOR(nsScriptableDateTimeFormat)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLanguageAtomService)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPlatformCharset, Init)

#ifdef XP_WIN
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCollationWin)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDateTimeFormatWin)
#endif

#ifdef USE_UNIX_LOCALE
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCollationUnix)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDateTimeFormatUnix)
#endif  

#ifdef USE_MAC_LOCALE
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCollationMacUC)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDateTimeFormatMac)
#endif  

#ifdef XP_OS2
NS_GENERIC_FACTORY_CONSTRUCTOR(nsOS2Locale)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCollationOS2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDateTimeFormatOS2)
#endif

#endif
