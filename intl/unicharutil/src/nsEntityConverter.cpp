/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsEntityConverter.h"
#include "nsIProperties.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsLiteralString.h"
#include "nsXPIDLString.h"
#include "nsString.h"

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
	nsresult rv;
    nsCOMPtr<nsIStringBundleService> bundleService =
        do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);

    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIStringBundle> entities;
    rv = bundleService->CreateBundle(url.get(), getter_AddRefs(entities));
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
  PRUint32 len = nsCRT::strlen(inString);
  for (PRUint32 i = 0; i < len; i++) {
    nsAutoString key(NS_LITERAL_STRING("entity."));
    key.AppendInt(inString[i],10);
    
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



nsresult NS_NewEntityConverter(nsISupports** oResult)
{
   if(!oResult)
      return NS_ERROR_NULL_POINTER;
   *oResult = new nsEntityConverter();
   if(*oResult)
      NS_ADDREF(*oResult);
   return (*oResult) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}
