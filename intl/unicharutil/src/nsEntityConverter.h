/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEntityConverter_h__
#define nsEntityConverter_h__

#include "nsIEntityConverter.h"
#include "nsIFactory.h"
#include "nsIStringBundle.h"
#include "nsCOMPtr.h"

#define kVERSION_STRING_LEN 128

class nsEntityVersionList
{
public:
    nsEntityVersionList() : mEntities(NULL) {}
    
    PRUint32 mVersion;
    PRUnichar mEntityListName[kVERSION_STRING_LEN+1];
    nsCOMPtr<nsIStringBundle> mEntities;
};

class nsEntityConverter: public nsIEntityConverter
{
public:
	
	//
	// implementation methods
	//
	nsEntityConverter();
	virtual ~nsEntityConverter();

	//
	// nsISupports
	//
	NS_DECL_ISUPPORTS

	//
	// nsIEntityConverter
	//
	NS_IMETHOD ConvertUTF32ToEntity(PRUint32 character, PRUint32 entityVersion, char **_retval);
	NS_IMETHOD ConvertToEntity(PRUnichar character, PRUint32 entityVersion, char **_retval);

	NS_IMETHOD ConvertToEntities(const PRUnichar *inString, PRUint32 entityVersion, PRUnichar **_retval);

protected:

  // load a version property file and generate a version list (number/name pair)
  NS_IMETHOD LoadVersionPropertyFile();

  // map version number to version string
  const PRUnichar* GetVersionName(PRUint32 versionNumber);

  // map version number to a string bundle
  nsIStringBundle* GetVersionBundleInstance(PRUint32 versionNumber);

  // load a string bundle file
  already_AddRefed<nsIStringBundle> LoadEntityBundle(PRUint32 version);


  nsEntityVersionList *mVersionList;            // array of version number/name pairs
  PRUint32 mVersionListLength;                  // number of supported versions
};

#endif
