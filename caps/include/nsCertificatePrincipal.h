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
/*describes principals for use with signed scripts*/
#ifndef _NS_CERTIFICATE_PRINCIPAL_H_
#define _NS_CERTIFICATE_PRINCIPAL_H_
#include "nsVector.h"
#include "nsIPrincipal.h"

class nsCertificatePrincipal : public nsICertificatePrincipal {
public:
	NS_DECL_ISUPPORTS

	NS_IMETHOD
	GetPublicKey(char ** pk);

	NS_IMETHOD
	GetCompanyName(char ** cn);

	NS_IMETHOD
	GetCertificateAuthority(char ** ca);

	NS_IMETHOD
	GetSerialNumber(char ** sn);

	NS_IMETHOD
	GetExpirationDate(char ** ed);

	NS_IMETHOD
	GetFingerPrint(char ** fp);

	NS_IMETHOD
	GetType(PRInt16 * type);

	NS_IMETHOD
	IsSecure(PRBool * result);

	NS_IMETHOD
	ToString(char ** result);

	NS_IMETHOD
	HashCode(PRUint32 * code);

	NS_IMETHOD
	Equals(nsIPrincipal * other, PRBool * result);

	nsCertificatePrincipal(PRInt16 type, const char * key);
	nsCertificatePrincipal(PRInt16 type, const unsigned char ** certChain, PRUint32 * certChainLengths, PRUint32 noOfCerts);
	virtual ~nsCertificatePrincipal(void);

protected:
	PRInt16 itsType;
	const char * itsKey;
	nsVector * itsCertificateArray;
	char * itsCompanyName;
	char * itsCertificateAuthority;
	char * itsSerialNumber;
	char * itsExpirationDate;
	char * itsFingerPrint;	
	char * itsNickname;
	char * itsString;
};

#endif // _NS_CERTIFICATE_PRINCIPAL_H_
