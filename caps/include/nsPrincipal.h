/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef _NS_PRINCIPAL_H_
#define _NS_PRINCIPAL_H_

#include "prtypes.h"
#include "nsHashtable.h"
#include "nsVector.h"
#include "nsZig.h"
#include "nsCom.h"

/* The following should match what is in nsJVM plugin's java security code */
typedef enum nsPrincipalType {
  nsPrincipalType_Unknown=-1, 
  nsPrincipalType_CodebaseExact=10,
  nsPrincipalType_CodebaseRegexp,
  nsPrincipalType_Cert,
  nsPrincipalType_CertFingerPrint,
  nsPrincipalType_CertKey
} nsPrincipalType;


typedef nsVector nsPrincipalArray;

class nsPrincipal {

public:

	/* Public Field Accessors */
	/* Public Methods */

	nsPrincipal(nsPrincipalType type, void * key, PRUint32 key_len);
	nsPrincipal(nsPrincipalType type, void * key, PRUint32 key_len, char *stringRep);
	virtual ~nsPrincipal();
	nsPrincipal(nsPrincipalType type, void * key, PRUint32 key_len, nsZig *zigObject);

	PRBool equals(nsPrincipal *principal);

	char * getVendor(void);

	char * getCompanyName(void);

	char * getSecAuth(void);

	char * getSerialNo(void);

	char * getExpDate(void);

	char * getFingerPrint(void);

	char * getNickname(void);

	nsPrincipalType getType();

	char *getKey();

	PRUint32 getKeyLength();

	PRInt32 hashCode(void);

	PRBool isCodebase(void);

	PRBool isCodebaseExact(void);

	PRBool isCodebaseRegexp(void);

	PRBool isSecurePrincipal(void);

	PRBool isCert(void);

	PRBool isCertFingerprint(void);

	char * toString(void);

	char * toVerboseString(void);

	char * savePrincipalPermanently(void);


private:
	/* Private Field Accessors */
	nsPrincipalType itsType;

	nsZig * itsZig;

	char * itsKey;

	PRUint32 itsKeyLen;

	PRInt32 itsHashCode;

	char * itsCompanyName;
	char * itsCertAuth;
	char * itsSerialNo;
	char * itsExpDate;
	char * itsAsciiFingerPrint;
	char * itsNickname;
	char * itsString;

	/* Private Methods */
	void init(nsPrincipalType type, void * key, PRUint32 key_len);

	PRInt32 computeHashCode(void * key, PRUint32 key_len);

	PRInt32 computeHashCode(void);

	char * saveCert(void);

	char * getCertAttribute(int attrib);
};


class PrincipalKey: public nsHashKey {
public:
  nsPrincipal *itsPrincipal;
  PrincipalKey(nsPrincipal *prin) {
    itsPrincipal = prin;
  }
  
  PRUint32 HashValue(void) const {
    return itsPrincipal->hashCode();
  }

  PRBool Equals(const nsHashKey *aKey) const {
    return (itsPrincipal->equals(((const PrincipalKey *) aKey)->itsPrincipal));
  }

  nsHashKey *Clone(void) const {
    return new PrincipalKey(itsPrincipal);
  }
};

/* XXX: Hack to determine the system principal */
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

nsPrincipal * CreateSystemPrincipal(char* zip_file_name, char *pathname);
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
/* XXX: end of hack to determine the system principal */

#endif /* _NS_PRINCIPAL_H_ */
