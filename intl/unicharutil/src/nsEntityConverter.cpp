/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsEntityConverter.h"
#include "nsIProperties.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIURL.h"
#include "nsNetUtil.h"

//
// guids
//
NS_DEFINE_IID(kIFactoryIID,NS_IFACTORY_IID);
NS_DEFINE_IID(kIPersistentPropertiesIID,NS_IPERSISTENTPROPERTIES_IID);

//
// implementation methods
//
nsEntityConverter::nsEntityConverter()
:	mVersionList(NULL),
  mVersionListLength(0)
{
	NS_INIT_REFCNT();
}

nsEntityConverter::~nsEntityConverter()
{
  if (NULL != mVersionList) delete [] mVersionList;
}

NS_IMETHODIMP 
nsEntityConverter::LoadVersionPropertyFile()
{
	nsString	aUrl; aUrl.AssignWithConversion("resource:/res/entityTables/htmlEntityVersions.properties");
	nsIPersistentProperties* entityProperties = NULL;
	nsIURI* url = NULL;
	nsIInputStream* in = NULL;
	nsresult rv;
  
  rv = NS_NewURI(&url,aUrl,NULL);
	if (NS_FAILED(rv)) return rv;

	rv = NS_OpenURI(&in,url);
	NS_RELEASE(url);
	if (NS_FAILED(rv)) return rv;

	rv = nsComponentManager::CreateInstance(NS_PERSISTENTPROPERTIES_CONTRACTID,NULL,
                                             kIPersistentPropertiesIID, 
                                             (void**)&entityProperties);
	if(NS_SUCCEEDED(rv) && in) {
		rv = entityProperties->Load(in);
    if (NS_SUCCEEDED(rv)) {
	    nsAutoString key, value; key.AssignWithConversion("length");
    	PRInt32	result;

	    rv = entityProperties->GetStringProperty(key,value);
	    NS_ASSERTION(NS_SUCCEEDED(rv),"nsEntityConverter: malformed entity table\n");
      if (NS_FAILED(rv)) goto done;
      mVersionListLength = value.ToInteger(&result);
 	    NS_ASSERTION(32 >= mVersionListLength,"nsEntityConverter: malformed entity table\n");
      if (32 < mVersionListLength) goto done;
      mVersionList = new nsEntityVersionList[mVersionListLength];
      if (NULL == mVersionList) {rv = NS_ERROR_OUT_OF_MEMORY; goto done;}

      for (PRUint32 i = 0; i < mVersionListLength && NS_SUCCEEDED(rv); i++) {
        key.SetLength(0);
        key.AppendInt(i+1, 10);
	      rv = entityProperties->GetStringProperty(key, value);
        PRUint32 len = value.Length();
        if (kVERSION_STRING_LEN < len) {rv = NS_ERROR_OUT_OF_MEMORY; goto done;}
        nsCRT::memcpy(mVersionList[i].mEntityListName, value.get(), len*sizeof(PRUnichar));
        mVersionList[i].mEntityListName[len] = 0;
        mVersionList[i].mVersion = (1 << i);
        mVersionList[i].mEntityProperties = NULL;
      }
    }
done:
		NS_IF_RELEASE(in);
		NS_IF_RELEASE(entityProperties);
	}
  return rv;
}

nsIPersistentProperties* 
nsEntityConverter::LoadEntityPropertyFile(PRInt32 version)
{
  nsString aUrl; aUrl.AssignWithConversion("resource:/res/entityTables/");
	nsIPersistentProperties* entityProperties = NULL;
	nsIURI* url = NULL;
	nsIInputStream* in = NULL;
  const PRUnichar *versionName = NULL;
	nsresult rv;
  
  versionName = GetVersionName(version);
  if (NULL == versionName) return NULL;

  aUrl.Append(versionName);
  aUrl.AppendWithConversion(".properties");

  rv = NS_NewURI(&url,aUrl,NULL);
	if (NS_FAILED(rv)) return NULL;

	rv = NS_OpenURI(&in,url);
	NS_RELEASE(url);
	if (NS_FAILED(rv)) return NULL;

	rv = nsComponentManager::CreateInstance(NS_PERSISTENTPROPERTIES_CONTRACTID,NULL,
                                             kIPersistentPropertiesIID, 
                                             (void**)&entityProperties);
	if(NS_SUCCEEDED(rv) && in) {
		rv = entityProperties->Load(in);
    if (NS_SUCCEEDED(rv)) {
      NS_IF_RELEASE(in);
      return entityProperties;
    }
	}
  NS_IF_RELEASE(in);
  NS_IF_RELEASE(entityProperties);
    
  return NULL;
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

nsIPersistentProperties*
nsEntityConverter:: GetVersionPropertyInst(PRUint32 versionNumber)
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
      if (NULL == mVersionList[i].mEntityProperties)
      { // not loaded
        // load the property file
        mVersionList[i].mEntityProperties = LoadEntityPropertyFile(versionNumber);
        NS_ASSERTION(mVersionList[i].mEntityProperties, "LoadEntityPropertyFile failed");
      }
      return mVersionList[i].mEntityProperties;
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
    nsIPersistentProperties* entityProperties = GetVersionPropertyInst(entityVersion & mask);
    NS_ASSERTION(entityProperties, "Cannot get the property file");

    if (NULL == entityProperties) 
      continue;

    nsAutoString key, value; key.AssignWithConversion("entity.");
		key.AppendInt(character,10);
    nsresult rv = entityProperties->GetStringProperty(key, value);
    if (NS_SUCCEEDED(rv)) {
      *_retval = value.ToNewCString();
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
    nsAutoString value, key; key.AssignWithConversion("entity.");
		key.AppendInt(inString[i],10);
    entity = NULL;
    for (PRUint32 mask = 1, mask2 = 0xFFFFFFFFL; (0!=(entityVersion & mask2)); mask<<=1, mask2<<=1) {
      if (0 == (entityVersion & mask)) 
         continue;
      nsIPersistentProperties* entityProperties = GetVersionPropertyInst(entityVersion & mask);
      NS_ASSERTION(entityProperties, "Cannot get the property file");

      if (NULL == entityProperties) 
          continue;

      nsresult rv = entityProperties->GetStringProperty(key, value);
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

  *_retval = outString.ToNewUnicode();
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
