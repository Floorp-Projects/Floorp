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
#include "nsCaps.h"
#include "nsCapsEnums.h"


typedef nsVector nsPrincipalArray;

struct nsPrincipal {

public:

	/* Public Field Accessors */
	/* Public Methods */

	nsPrincipal(nsPrincipalType type, const void * key, PRUint32 key_len);
	nsPrincipal(nsPrincipalType type, const void * key, PRUint32 key_len, char *stringRep);
	virtual ~nsPrincipal();
	nsPrincipal(nsPrincipalType type, const void * key, PRUint32 key_len, void *zigObject);

	nsPrincipal(nsPrincipalType type, const unsigned char **certChain, 
                PRUint32 *certChainLengths, 
                PRUint32 noOfCerts);

	PRBool equals(nsPrincipal *principal);

	char * getVendor(void);

	char * getCompanyName(void);

	char * getSecAuth(void);

	char * getSerialNo(void);

	char * getExpDate(void);

	char * getFingerPrint(void);

	char * getNickname(void);

	nsPrincipalType getType();

	void *getCertificate();

	char *getKey();

	PRUint32 getKeyLength();

	PRInt32 hashCode(void);

	PRBool isCodebase(void);

	PRBool isCodebaseExact(void);

	PRBool isCodebaseRegexp(void);

	PRBool isSecurePrincipal(void);

    /* The following method is used by Javasoft JVM to verify their 
     * code signing certificates against our security DB files
     */
	PRBool isTrustedCertChainPrincipal(void);

	PRBool isFileCodeBase(void);

	PRBool isCert(void);

	PRBool isCertFingerprint(void);

	char * toString(void);

	char * toVerboseString(void);

	char * savePrincipalPermanently(void);

    /* Caller should free the principals and the Principal array */
	static nsPrincipalArray* getSigners(void* zigPtr, char* pathname);


private:
	/* Private Field Accessors */
	nsPrincipalType itsType;

	void* itsZig;

	char* itsKey;
	PRUint32 itsKeyLen;

    nsVector* itsCertArray;

	PRInt32 itsHashCode;

	char* itsCompanyName;
	char* itsCertAuth;
	char* itsSerialNo;
	char* itsExpDate;
	char* itsAsciiFingerPrint;
	char* itsNickname;
	char* itsString;

	/* Private Methods */
	void init(nsPrincipalType type, const void * key, PRUint32 key_len);

	PRInt32 computeHashCode(const void * key, PRUint32 key_len);

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
