/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsEntityConverter.h"
#include "nsLiteralString.h"
#include "nsString.h"
#include "mozilla/Services.h"
#include "nsServiceManagerUtils.h"
#include "nsCRT.h"

//
// implementation methods
//
nsEntityConverter::nsEntityConverter() { }

nsEntityConverter::~nsEntityConverter() { }

nsIStringBundle*
nsEntityConverter:: GetVersionBundleInstance(uint32_t versionNumber)
{
  switch(versionNumber){
  case nsIEntityConverter::html40Latin1:
    if (!mHTML40Latin1Bundle) {
      mHTML40Latin1Bundle = LoadEntityBundle(kHTML40LATIN1);
      MOZ_ASSERT(mHTML40Latin1Bundle, "LoadEntityBundle failed");
    }
    return mHTML40Latin1Bundle;
  case nsIEntityConverter::html40Symbols:
    if (!mHTML40SymbolsBundle) {
      mHTML40SymbolsBundle = LoadEntityBundle(kHTML40SYMBOLS);
      MOZ_ASSERT(mHTML40SymbolsBundle, "LoadEntityBundle failed");
    }
    return mHTML40SymbolsBundle;
  case nsIEntityConverter::html40Special:
    if (!mHTML40SpecialBundle) {
      mHTML40SpecialBundle = LoadEntityBundle(kHTML40SPECIAL);
      MOZ_ASSERT(mHTML40SpecialBundle, "LoadEntityBundle failed");
    }
    return mHTML40SpecialBundle;
  case nsIEntityConverter::mathml20:
    if (!mMathML20Bundle) {
      mMathML20Bundle = LoadEntityBundle(kMATHML20);
      MOZ_ASSERT(mMathML20Bundle, "LoadEntityBundle failed");
    }
    return mMathML20Bundle;
  default:
    return nullptr;
  }
}

already_AddRefed<nsIStringBundle>
nsEntityConverter:: LoadEntityBundle(const char *fileName)
{
  NS_ENSURE_TRUE(fileName, nullptr);

  nsAutoCString url("resource://gre/res/entityTables/");
  nsresult rv;

  nsCOMPtr<nsIStringBundleService> bundleService =
  do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, nullptr);

  url.Append(fileName);

  nsCOMPtr<nsIStringBundle> bundle;
  rv = bundleService->CreateBundle(url.get(), getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, nullptr);

  return bundle.forget();
}

//
// nsISupports methods
//
NS_IMPL_ISUPPORTS(nsEntityConverter,nsIEntityConverter)

//
// nsIEntityConverter
//
NS_IMETHODIMP
nsEntityConverter::ConvertToEntity(char16_t character, uint32_t entityVersion, char **_retval)
{
  return ConvertUTF32ToEntity((uint32_t)character, entityVersion, _retval);
}

NS_IMETHODIMP
nsEntityConverter::ConvertUTF32ToEntity(uint32_t character, uint32_t entityVersion, char **_retval)
{
  NS_ASSERTION(_retval, "null ptr- _retval");
  if (nullptr == _retval) {
    return NS_ERROR_NULL_POINTER;
  }
  *_retval = nullptr;

  for (uint32_t mask = 1, mask2 = 0xFFFFFFFFL; (0!=(entityVersion & mask2)); mask<<=1, mask2<<=1) {
    if (0 == (entityVersion & mask)) {
      continue;
    }

    nsIStringBundle* entities = GetVersionBundleInstance(entityVersion & mask);
    NS_ASSERTION(entities, "Cannot get the entity");

    if (!entities) {
      continue;
    }

    nsAutoString key(NS_LITERAL_STRING("entity."));
    key.AppendInt(character,10);

    nsXPIDLString value;
    nsresult rv = entities->GetStringFromName(key.get(), getter_Copies(value));
    if (NS_SUCCEEDED(rv)) {
      *_retval = ToNewCString(value);
      return NS_OK;
    }
  }
  return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP
nsEntityConverter::ConvertToEntities(const char16_t *inString, uint32_t entityVersion, char16_t **_retval)
{
  NS_ENSURE_ARG_POINTER(inString);
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = nullptr;

  nsString outString;

  // per character look for the entity
  uint32_t len = NS_strlen(inString);
  for (uint32_t i = 0; i < len; i++) {
    nsAutoString key(NS_LITERAL_STRING("entity."));
    if (NS_IS_HIGH_SURROGATE(inString[i]) && i + 2 < len && NS_IS_LOW_SURROGATE(inString[i + 1])) {
      key.AppendInt(SURROGATE_TO_UCS4(inString[i], inString[i+1]), 10);
      ++i;
    } else {
      key.AppendInt(inString[i],10);
    }

    nsXPIDLString value;
    const char16_t *entity = nullptr;

    for (uint32_t mask = 1, mask2 = 0xFFFFFFFFL; (0!=(entityVersion & mask2)); mask<<=1, mask2<<=1) {
      if (0 == (entityVersion & mask)) {
        continue;
      }
      nsIStringBundle* entities = GetVersionBundleInstance(entityVersion & mask);
      NS_ASSERTION(entities, "Cannot get the property file");

      if (!entities) {
        continue;
      }

      nsresult rv = entities->GetStringFromName(key.get(), getter_Copies(value));
      if (NS_SUCCEEDED(rv)) {
        entity = value.get();
        break;
      }
    }
    if (entity) {
      outString.Append(entity);
    } else {
      outString.Append(&inString[i], 1);
    }
  }

  *_retval = ToNewUnicode(outString);

  return NS_OK;
}
