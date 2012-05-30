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
nsEntityConverter::nsEntityConverter()
:	mVersionList(NULL),
  mVersionListLength(0)
{
}

nsEntityConverter::~nsEntityConverter()
{
  if (NULL != mVersionList) delete [] mVersionList;
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
    
    PRInt32	result;

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

    for (PRUint32 i = 0; i < mVersionListLength && NS_SUCCEEDED(rv); i++) {
        key.SetLength(0);
        key.AppendInt(i+1, 10);
        rv = entities->GetStringFromName(key.get(), getter_Copies(value));
        PRUint32 len = value.Length();
        if (kVERSION_STRING_LEN < len) return NS_ERROR_UNEXPECTED;
        
        memcpy(mVersionList[i].mEntityListName, value.get(), len*sizeof(PRUnichar));
        mVersionList[i].mEntityListName[len] = 0;
        mVersionList[i].mVersion = (1 << i);
    }

    return NS_OK;
}

already_AddRefed<nsIStringBundle>
nsEntityConverter::LoadEntityBundle(PRUint32 version)
{
  nsCAutoString url(NS_LITERAL_CSTRING("resource://gre/res/entityTables/"));
  const PRUnichar *versionName = NULL;
  nsresult rv;

  nsCOMPtr<nsIStringBundleService> bundleService =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return NULL;
  
  versionName = GetVersionName(version);
  if (NULL == versionName) return NULL;

  // all property file names are ASCII, like "html40Latin1" so this is safe
  LossyAppendUTF16toASCII(versionName, url);
  url.Append(".properties");

  nsIStringBundle* bundle;
  rv = bundleService->CreateBundle(url.get(), &bundle);
  if (NS_FAILED(rv)) return NULL;
  
  // does this addref right?
  return bundle;
}

const PRUnichar*
nsEntityConverter:: GetVersionName(PRUint32 versionNumber)
{
  for (PRUint32 i = 0; i < mVersionListLength; i++) {
    if (versionNumber == mVersionList[i].mVersion)
      return mVersionList[i].mEntityListName;
  }

  return NULL;
}

nsIStringBundle*
nsEntityConverter:: GetVersionBundleInstance(PRUint32 versionNumber)
{
  if (NULL == mVersionList) {
    // load the property file which contains available version names
    // and generate a list of version/name pair
    nsresult rv = LoadVersionPropertyFile();
    if (NS_FAILED(rv)) return NULL;
  }

  PRUint32 i;
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

  return NULL;
}


//
// nsISupports methods
//
NS_IMPL_ISUPPORTS1(nsEntityConverter,nsIEntityConverter)


//
// nsIEntityConverter
//
NS_IMETHODIMP
nsEntityConverter::ConvertToEntity(PRUnichar character, PRUint32 entityVersion, char **_retval)
{ 
  return ConvertUTF32ToEntity((PRUint32)character, entityVersion, _retval);
}

NS_IMETHODIMP
nsEntityConverter::ConvertUTF32ToEntity(PRUint32 character, PRUint32 entityVersion, char **_retval)
{
  NS_ASSERTION(_retval, "null ptr- _retval");
  if(nsnull == _retval)
    return NS_ERROR_NULL_POINTER;
  *_retval = NULL;

  for (PRUint32 mask = 1, mask2 = 0xFFFFFFFFL; (0!=(entityVersion & mask2)); mask<<=1, mask2<<=1) {
    if (0 == (entityVersion & mask)) 
      continue;
    nsIStringBundle* entities = GetVersionBundleInstance(entityVersion & mask);
    NS_ASSERTION(entities, "Cannot get the property file");

    if (NULL == entities) 
      continue;

    nsAutoString key(NS_LITERAL_STRING("entity."));
    key.AppendInt(character,10);

    nsXPIDLString value;
    nsresult rv = entities->GetStringFromName(key.get(), getter_Copies(value));
    if (NS_SUCCEEDED(rv)) {
      *_retval = ToNewCString(value);
      if(nsnull == *_retval)
        return NS_ERROR_OUT_OF_MEMORY;
      else
        return NS_OK;
    }
  }
	return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP
nsEntityConverter::ConvertToEntities(const PRUnichar *inString, PRUint32 entityVersion, PRUnichar **_retval)
{
  NS_ASSERTION(inString, "null ptr- inString");
  NS_ASSERTION(_retval, "null ptr- _retval");
  if((nsnull == inString) || (nsnull == _retval))
    return NS_ERROR_NULL_POINTER;
  *_retval = NULL;

  const PRUnichar *entity = NULL;
  nsString outString;

  // per character look for the entity
  PRUint32 len = NS_strlen(inString);
  for (PRUint32 i = 0; i < len; i++) {
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
    
    entity = NULL;
    for (PRUint32 mask = 1, mask2 = 0xFFFFFFFFL; (0!=(entityVersion & mask2)); mask<<=1, mask2<<=1) {
      if (0 == (entityVersion & mask)) 
         continue;
      nsIStringBundle* entities = GetVersionBundleInstance(entityVersion & mask);
      NS_ASSERTION(entities, "Cannot get the property file");

      if (NULL == entities) 
          continue;

      nsresult rv = entities->GetStringFromName(key.get(),
                                                getter_Copies(value));
      if (NS_SUCCEEDED(rv)) {
        entity = value.get();
        break;
      }
    }
    if (NULL != entity) {
      outString.Append(entity);
    }
    else {
      outString.Append(&inString[i], 1);
    }
  }

  *_retval = ToNewUnicode(outString);
  if (NULL == *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}
