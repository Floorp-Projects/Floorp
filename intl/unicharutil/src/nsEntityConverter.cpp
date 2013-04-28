/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsEntityConverter.h"
#include "nsIProperties.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsLiteralString.h"
#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "mozilla/Services.h"

//
// implementation methods
//
nsEntityConverter::nsEntityConverter() :
    mVersionList(nullptr),
    mVersionListLength(0)
{
}

nsEntityConverter::~nsEntityConverter()
{
    if (mVersionList)
        delete [] mVersionList;
}

NS_IMETHODIMP 
nsEntityConverter::LoadVersionPropertyFile()
{
    NS_NAMED_LITERAL_CSTRING(url, "resource://gre/res/entityTables/htmlEntityVersions.properties");

    nsCOMPtr<nsIStringBundleService> bundleService =
        mozilla::services::GetStringBundleService();
    if (!bundleService)
        return NS_ERROR_FAILURE;
    
    nsCOMPtr<nsIStringBundle> entities;
    nsresult rv = bundleService->CreateBundle(url.get(), getter_AddRefs(entities));
    if (NS_FAILED(rv)) return rv;
    
    nsresult result;

    nsAutoString key;
    nsXPIDLString value;
    rv = entities->GetStringFromName(NS_LITERAL_STRING("length").get(),
                                     getter_Copies(value));
    NS_ASSERTION(NS_SUCCEEDED(rv),"nsEntityConverter: malformed entity table\n");
    if (NS_FAILED(rv)) return rv;
      
    mVersionListLength = nsAutoString(value).ToInteger(&result);
    NS_ASSERTION(32 >= mVersionListLength,"nsEntityConverter: malformed entity table\n");
    if (32 < mVersionListLength) return NS_ERROR_FAILURE;
    
    mVersionList = new nsEntityVersionList[mVersionListLength];
    if (!mVersionList) return NS_ERROR_OUT_OF_MEMORY;

    for (uint32_t i = 0; i < mVersionListLength && NS_SUCCEEDED(rv); i++) {
        key.SetLength(0);
        key.AppendInt(i+1, 10);
        rv = entities->GetStringFromName(key.get(), getter_Copies(value));
        uint32_t len = value.Length();
        if (kVERSION_STRING_LEN < len) return NS_ERROR_UNEXPECTED;
        
        memcpy(mVersionList[i].mEntityListName, value.get(), len*sizeof(PRUnichar));
        mVersionList[i].mEntityListName[len] = 0;
        mVersionList[i].mVersion = (1 << i);
    }

    return NS_OK;
}

already_AddRefed<nsIStringBundle>
nsEntityConverter::LoadEntityBundle(uint32_t version)
{
  nsAutoCString url(NS_LITERAL_CSTRING("resource://gre/res/entityTables/"));
  nsresult rv;

  nsCOMPtr<nsIStringBundleService> bundleService =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, nullptr);
  
  const PRUnichar *versionName = GetVersionName(version);
  NS_ENSURE_TRUE(versionName, nullptr);

  // all property file names are ASCII, like "html40Latin1" so this is safe
  LossyAppendUTF16toASCII(versionName, url);
  url.Append(".properties");

  nsCOMPtr<nsIStringBundle> bundle;
  rv = bundleService->CreateBundle(url.get(), getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, nullptr);
  
  return bundle.forget();
}

const PRUnichar*
nsEntityConverter:: GetVersionName(uint32_t versionNumber)
{
  for (uint32_t i = 0; i < mVersionListLength; i++) {
    if (versionNumber == mVersionList[i].mVersion)
      return mVersionList[i].mEntityListName;
  }

  return nullptr;
}

nsIStringBundle*
nsEntityConverter:: GetVersionBundleInstance(uint32_t versionNumber)
{
  if (!mVersionList) {
    // load the property file which contains available version names
    // and generate a list of version/name pair
    if (NS_FAILED(LoadVersionPropertyFile()))
      return nullptr;
  }

  uint32_t i;
  for (i = 0; i < mVersionListLength; i++) {
    if (versionNumber == mVersionList[i].mVersion) {
      if (!mVersionList[i].mEntities)
      { // not loaded
        // load the property file
        mVersionList[i].mEntities = LoadEntityBundle(versionNumber);
        NS_ASSERTION(mVersionList[i].mEntities, "LoadEntityBundle failed");
      }
      return mVersionList[i].mEntities.get();
    }
  }

  return nullptr;
}


//
// nsISupports methods
//
NS_IMPL_ISUPPORTS1(nsEntityConverter,nsIEntityConverter)


//
// nsIEntityConverter
//
NS_IMETHODIMP
nsEntityConverter::ConvertToEntity(PRUnichar character, uint32_t entityVersion, char **_retval)
{ 
  return ConvertUTF32ToEntity((uint32_t)character, entityVersion, _retval);
}

NS_IMETHODIMP
nsEntityConverter::ConvertUTF32ToEntity(uint32_t character, uint32_t entityVersion, char **_retval)
{
  NS_ASSERTION(_retval, "null ptr- _retval");
  if(nullptr == _retval)
    return NS_ERROR_NULL_POINTER;
  *_retval = nullptr;

  for (uint32_t mask = 1, mask2 = 0xFFFFFFFFL; (0!=(entityVersion & mask2)); mask<<=1, mask2<<=1) {
    if (0 == (entityVersion & mask)) 
      continue;
    nsIStringBundle* entities = GetVersionBundleInstance(entityVersion & mask);
    NS_ASSERTION(entities, "Cannot get the property file");

    if (!entities) 
      continue;

    nsAutoString key(NS_LITERAL_STRING("entity."));
    key.AppendInt(character,10);

    nsXPIDLString value;
    nsresult rv = entities->GetStringFromName(key.get(), getter_Copies(value));
    if (NS_SUCCEEDED(rv)) {
      *_retval = ToNewCString(value);
      if(nullptr == *_retval)
        return NS_ERROR_OUT_OF_MEMORY;
      else
        return NS_OK;
    }
  }
	return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP
nsEntityConverter::ConvertToEntities(const PRUnichar *inString, uint32_t entityVersion, PRUnichar **_retval)
{
  NS_ENSURE_ARG_POINTER(inString);
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = nullptr;

  nsString outString;

  // per character look for the entity
  uint32_t len = NS_strlen(inString);
  for (uint32_t i = 0; i < len; i++) {
    nsAutoString key(NS_LITERAL_STRING("entity."));
    if (NS_IS_HIGH_SURROGATE(inString[i]) &&
        i + 2 < len &&
        NS_IS_LOW_SURROGATE(inString[i + 1])) {
      key.AppendInt(SURROGATE_TO_UCS4(inString[i], inString[i+1]), 10);
      ++i;
    }
    else {
      key.AppendInt(inString[i],10);
    }
    
    nsXPIDLString value;
    const PRUnichar *entity = nullptr;

    for (uint32_t mask = 1, mask2 = 0xFFFFFFFFL; (0!=(entityVersion & mask2)); mask<<=1, mask2<<=1) {
      if (0 == (entityVersion & mask)) 
         continue;
      nsIStringBundle* entities = GetVersionBundleInstance(entityVersion & mask);
      NS_ASSERTION(entities, "Cannot get the property file");

      if (!entities) 
          continue;

      nsresult rv = entities->GetStringFromName(key.get(),
                                                getter_Copies(value));
      if (NS_SUCCEEDED(rv)) {
        entity = value.get();
        break;
      }
    }
    if (entity) {
      outString.Append(entity);
    }
    else {
      outString.Append(&inString[i], 1);
    }
  }

  *_retval = ToNewUnicode(outString);
  if (!*_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}
