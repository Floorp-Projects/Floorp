/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsIEntityConverter.h"
#include "nsIFactory.h"
#include "nsIProperties.h"


nsresult NS_NewEntityConverter(nsISupports** oResult);

#define kVERSION_STRING_LEN 128

class nsEntityVersionList
{
public:
  nsEntityVersionList() : mEntityProperties(NULL) {}
  ~nsEntityVersionList() {NS_IF_RELEASE(mEntityProperties);}
  PRUint32 mVersion;
  PRUnichar mEntityListName[kVERSION_STRING_LEN+1];
  nsIPersistentProperties *mEntityProperties;
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
	NS_IMETHOD ConvertToEntity(PRUnichar character, PRUint32 entityVersion, char **_retval);

	NS_IMETHOD ConvertToEntities(const PRUnichar *inString, PRUint32 entityVersion, PRUnichar **_retval);

protected:

  // load a version property file and generate a version list (number/name pair)
  NS_IMETHOD LoadVersionPropertyFile();

  // map version number to version string
  const PRUnichar* GetVersionName(PRUint32 versionNumber);

  // map version number to nsIPersistentProperties
  nsIPersistentProperties* GetVersionPropertyInst(PRUint32 versionNumber);

  // load a properies file
  nsIPersistentProperties* LoadEntityPropertyFile(PRInt32 version);


  nsEntityVersionList *mVersionList;            // array of version number/name pairs
  PRUint32 mVersionListLength;                  // number of supported versions
};
