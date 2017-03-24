/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCollationFactory.h"
#include "nsCollationCID.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/intl/LocaleService.h"

////////////////////////////////////////////////////////////////////////////////

NS_DEFINE_CID(kCollationCID, NS_COLLATION_CID);

NS_IMPL_ISUPPORTS(nsCollationFactory, nsICollationFactory)

nsresult nsCollationFactory::CreateCollation(nsICollation** instancePtr)
{
  nsAutoCString appLocale;
  mozilla::intl::LocaleService::GetInstance()->GetAppLocaleAsLangTag(appLocale);

  return CreateCollationForLocale(appLocale, instancePtr);
}

nsresult
nsCollationFactory::CreateCollationForLocale(const nsACString& locale, nsICollation** instancePtr)
{
  // Create a collation interface instance.
  //
  nsICollation *inst;
  nsresult res;

  res = CallCreateInstance(kCollationCID, &inst);
  if (NS_FAILED(res)) {
    return res;
  }

  inst->Initialize(locale);

  *instancePtr = inst;

  return res;
}
