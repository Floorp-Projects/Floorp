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

/* TODO:
 *
 *  + Remove all XXX's.
 *
 */

#include "prtime.h"
#include "nsPrincipal.h"
#include "nsPrivilegeManager.h"

#include "structs.h"
#include "xp_mem.h"
#include "prmem.h"
#include "zig.h"
#include "secnav.h"

#ifdef MOZ_SECURITY
#include "navhook.h"
#include "jarutil.h"
#endif /* MOZ_SECURITY */

/* XXX: Hack to determine the system principal */

PR_BEGIN_EXTERN_C

#include "proto.h"
#include "jpermission.h"
#include "nsZip.h"
#include "fe_proto.h"
#include "nsLoadZig.h"

static void destroyCertificates(nsVector* certArray);
static nsVector* getTempCertificates(const unsigned char **certChain, 
                                     PRUint32 *certChainLengths, 
                                     PRUint32 noOfCerts);

/* XXX: Create an error object with all arguments except errorText, instead pass error enum,
   This will be a method on caps consumer interface. */
PR_PUBLIC_API(int)
nsPrintZigError(int status, ZIG *zig, const char *metafile, char *pathname, 
				 char *errortext)
{
    char* data;
	char* error_fmt = "# Error: %s (%d)\n#\tjar file: %s\n#\tpath:     %s\n";
    char* zig_name = NULL;
    int len;

	PR_ASSERT(errortext);

    if (zig) {
        zig_name = SOB_get_url(zig);
    }
        
    if (!zig_name) {
        zig_name = "unknown";
    }

    if (!pathname) {
        pathname = "";
    }

    /* Add 16 slop bytes */
	len = strlen(error_fmt) + strlen(zig_name) + strlen(pathname) + 
		  strlen(errortext) + 32;

    if ((data = (char *)PR_MALLOC(len)) == 0) {
	    return 0;
    }
	sprintf(data, error_fmt, errortext, status, zig_name, pathname);

    MWContext* someRandomContext = XP_FindSomeContext();
    FE_Alert(someRandomContext, data);
	PR_DELETE(data);

	return 0;
}

nsPrincipal *
CreateSystemPrincipal(char* zip_file_name, char *pathname)
{
  ZIG* zig;
  ZIG_Context * context;
  SOBITEM *item;
  FINGERZIG *fingPrint;
  int size=0;
  int slot=0;
  ns_zip_t *zip;
  nsPrincipal *sysPrin = NULL;
  
  if (!pathname)
    return NULL;
  
  zip = ns_zip_open(zip_file_name);
  if (zip == NULL) {
    return NULL;
  }
  zig = (ZIG*)nsInitializeZig(zip, nsPrintZigError);
  if (zig == NULL) {
    goto done;
  }
  
  /* count the number of signers */ 
  if ((context = SOB_find(zig, pathname, ZIG_SIGN)) == NULL) {
    goto done;
  }
  while (SOB_find_next (context, &item) >= 0) {
    size++;
  }
  SOB_find_end(context);
  
  if ((context = SOB_find(zig, pathname, ZIG_SIGN)) == NULL) {
    goto done;
  }
  while (SOB_find_next(context, &item) >= 0) {
    PR_ASSERT(slot < size);
    
    /* Allocate the Cert's FP and put them in an array */
    fingPrint = (FINGERZIG *) item->data;
    sysPrin = new nsPrincipal(nsPrincipalType_CertKey,
                              fingPrint->key,
                              fingPrint->length);
    if (sysPrin != NULL)
      break;
  }
  SOB_find_end(context);
  
done:   
  ns_zip_close(zip);
  return sysPrin;
}

/* XXX: Move all PR_END_EXTERN_C to end of each file */
PR_END_EXTERN_C

/* XXX: end of hack to determine the system principal */


static void destroyCertificates(nsVector* certArray) 
{
  if (certArray == NULL)
    return;

  for (PRUint32 i = certArray->GetSize(); i-- > 0; ) {
    CERTCertificate *cert = (CERTCertificate *)certArray->Get(i);
    if (cert != NULL) {
      CERT_DestroyCertificate(cert);
      certArray->Set(i, NULL);
    }
  }
  delete certArray;
}


static nsVector* getTempCertificates(const unsigned char **certChain, 
                                     PRUint32 *certChainLengths, 
                                     PRUint32 noOfCerts)
{
#ifndef MOZ_SECURITY
  return NULL;
#else
  CERTCertificate *cert;
  CERTCertDBHandle *handle = CERT_GetDefaultCertDB();
  SECStatus rv;

  nsVector* certArray = new nsVector();
  certArray->SetSize(noOfCerts, 1);
  if (certArray == NULL) {
    return NULL;
  }

  for (PRUint32 i = noOfCerts; i-- > 0; ) {
    SECItem derCert;

    derCert.data = (unsigned char *)certChain[i];
    derCert.len = certChainLengths[i];

    cert = CERT_NewTempCertificate(handle, &derCert, NULL,
                                   PR_FALSE, PR_TRUE);
       
    if (cert != NULL) {
      certArray->Set(i, (void*)cert);
    } else {
      // unable to add cert to the temp database
      certArray->Set(i, NULL);
    }
  }

  cert = (CERTCertificate *)certArray->Get(0);
  rv = CERT_VerifyCert(handle, cert, PR_TRUE, 
                       certUsageObjectSigner,
                       PR_Now(), NULL, NULL);
  if (rv != SECSuccess) {
     // Free the certificates and mark this principal as not trusted.
     destroyCertificates(certArray);
     return NULL;
  }
  return certArray;
#endif /* MOZ_SECURITY */
}


//
// 			PUBLIC METHODS 
//

nsPrincipal::nsPrincipal(nsPrincipalType type, void * key, PRUint32 key_len)
{
  init(type, key, key_len);
}

nsPrincipal::nsPrincipal(nsPrincipalType type, void * key, PRUint32 key_len, void *zigObject)
{
  init(type, key, key_len);
  itsZig = zigObject;
}

nsPrincipal::nsPrincipal(nsPrincipalType type, void * key, PRUint32 key_len, char *stringRep)
{
  init(type, key, key_len);
  itsString = stringRep;
}

nsPrincipal::nsPrincipal(nsPrincipalType type, 
                         const unsigned char **certChain, 
                         PRUint32 *certChainLengths, 
                         PRUint32 noOfCerts)
{
  /* We will store the signers certificate as the key */
  init(type, (void*)certChain[0], certChainLengths[0]);
  itsCertArray = getTempCertificates(certChain, 
                                     certChainLengths, 
                                     noOfCerts);
}

nsPrincipal::~nsPrincipal(void)
{
  if (itsKey) {
#ifdef DEBUG_raman
  fprintf(stderr, "Deleting principal %s\n", itsKey);
#endif /* DEBUG_raman */
    delete []itsKey;
  }
  if (itsCompanyName) {
    delete []itsCompanyName;
  }
  if (itsCertAuth) {
    delete []itsCertAuth;
  }
  if (itsSerialNo) {
    delete []itsSerialNo;
  }
  if (itsExpDate) {
    delete []itsExpDate;
  }
  if (itsAsciiFingerPrint) {
    delete []itsAsciiFingerPrint;
  }
  if (itsNickname) {
    delete []itsNickname;
  }
  if (itsCertArray) {
    destroyCertificates(itsCertArray);
  }
}


PRBool nsPrincipal::equals(nsPrincipal *prin)
{
  if (prin == this) 
    return PR_TRUE;

  /* Deal with CertChain principal specially */
  if ((itsType == nsPrincipalType_CertChain) ||
      (prin->itsType == nsPrincipalType_CertChain)) {
    /* Because we have full certificate for the CertChain
     * we will compare different attributes of the principal 
     * and if all the attributes match, then we return TRUE
     */
    if ((XP_STRCMP(getSerialNo(), prin->getSerialNo()) == 0) &&
        (XP_STRCMP(getSecAuth(), prin->getSecAuth()) == 0) &&
        (XP_STRCMP(getExpDate(), prin->getExpDate()) == 0) &&
        (XP_STRCMP(getFingerPrint(), prin->getFingerPrint()) == 0))
      return PR_TRUE;
  }

  if ((itsType != prin->itsType) ||
      (itsKeyLen != prin->itsKeyLen))
    return PR_FALSE;

  if (0 == memcmp(itsKey, prin->itsKey, itsKeyLen))
    return PR_TRUE;
  return PR_FALSE;
}

char * nsPrincipal::getVendor(void)
{
  switch(itsType) {
  case nsPrincipalType_Cert:
  case nsPrincipalType_CertKey:
  case nsPrincipalType_CertFingerPrint:
  case nsPrincipalType_CertChain:
    return getNickname();

  default:
    PR_ASSERT(PR_FALSE);
    return NULL;

  case nsPrincipalType_CodebaseExact:
    return itsKey;
  }
}

// XXX copyied from ns/lib/libjar/zig.h
// RAMAN: Why??
#ifndef ZIG_C_COMPANY
#define ZIG_C_COMPANY   1
#endif
#ifndef ZIG_C_CA
#define ZIG_C_CA        2
#endif
#ifndef ZIG_C_SERIAL
#define ZIG_C_SERIAL    3
#endif
#ifndef ZIG_C_EXPIRES
#define ZIG_C_EXPIRES   4
#endif
#ifndef ZIG_C_NICKNAME
#define ZIG_C_NICKNAME  5
#endif
#ifndef ZIG_C_FP
#define ZIG_C_FP        6
#endif
#ifndef ZIG_C_JAVA
#define ZIG_C_JAVA      100
#endif

char * nsPrincipal::getCompanyName(void)
{
  if (itsCompanyName == NULL)
    itsCompanyName = getCertAttribute(ZIG_C_COMPANY);
  return itsCompanyName;
}

char * nsPrincipal::getSecAuth(void)
{
  if (itsCertAuth == NULL)
    itsCertAuth = getCertAttribute(ZIG_C_CA);
  return itsCertAuth;
}

char * nsPrincipal::getSerialNo(void)
{
  if (itsSerialNo == NULL)
    itsSerialNo = getCertAttribute(ZIG_C_SERIAL);
  return itsSerialNo;
}

char * nsPrincipal::getExpDate(void)
{
  if (itsExpDate == NULL)
    itsExpDate = getCertAttribute(ZIG_C_EXPIRES);
  return itsExpDate;
}

char * nsPrincipal::getFingerPrint(void)
{
  switch(itsType) {
  case nsPrincipalType_Cert:
  case nsPrincipalType_CertFingerPrint:
  case nsPrincipalType_CodebaseExact:
  case nsPrincipalType_CodebaseRegexp:
    return toString();

  case nsPrincipalType_CertKey:
  case nsPrincipalType_CertChain:
    if (itsAsciiFingerPrint == NULL)
      itsAsciiFingerPrint = getCertAttribute(ZIG_C_FP);
    return itsAsciiFingerPrint;
  default:
    return NULL;
  }
  return NULL;
}

char * nsPrincipal::getNickname(void)
{
  if ((nsPrincipalType_Cert == itsType) && 
      (this == nsPrivilegeManager::getUnsignedPrincipal())) {
    /* XXX: The following needs to i18n */
    return "Unsigned classes from local hard disk";
  }

  if ((nsPrincipalType_Cert == itsType) && 
      (this == nsPrivilegeManager::getUnknownPrincipal())) {
    /* XXX: The following needs to i18n */
    return "Classes for whom we don't the principal";
  }

  if ((nsPrincipalType_CertKey != itsType) &&
      (nsPrincipalType_CertChain != itsType))
    return itsKey;

  if (itsNickname == NULL)
    itsNickname = getCertAttribute(ZIG_C_NICKNAME);
  return itsNickname;
}

nsPrincipalType
nsPrincipal::getType()
{
  return itsType;
}

void*
nsPrincipal::getCertificate()
{
  void* cert=NULL;
  if ((itsType == nsPrincipalType_CertChain) &&
      (itsCertArray != NULL)) {
    cert = itsCertArray->Get(0);
  }
  return cert;
}

char *
nsPrincipal::getKey()
{
  return itsKey;
}

PRUint32
nsPrincipal::getKeyLength()
{
  return itsKeyLen;
}

PRInt32 nsPrincipal::hashCode(void)
{
  return itsHashCode;
}

PRBool nsPrincipal::isCodebase(void)
{
  if (itsType == nsPrincipalType_CodebaseExact)
    return PR_TRUE;
  return PR_FALSE;
}

PRBool nsPrincipal::isCodebaseRegexp(void) 
{
  /* We don't support regular expressions yet */
  return PR_FALSE;
}

PRBool nsPrincipal::isCodebaseExact(void)
{
  if (itsType == nsPrincipalType_CodebaseExact)
    return PR_TRUE;
  return PR_FALSE;
}


PRBool nsPrincipal::isSecurePrincipal(void)
{
  if (this == nsPrivilegeManager::getUnknownPrincipal()) {
    return PR_FALSE;
  }

  if (!isCodebase()) 
    return PR_TRUE;

  PR_ASSERT(itsKey != NULL);

  if ((0 == memcmp("https:", itsKey, strlen("https:"))) ||
      (0 == memcmp("file:", itsKey, strlen("file:"))))
    return PR_TRUE;

  /* signed.applets.codebase_principal_support */
  if ((0 == memcmp("http:", itsKey, strlen("http:"))) && 
      (CMGetBoolPref("signed.applets.codebase_principal_support")))
    return PR_TRUE;

  return PR_FALSE;
}

PRBool nsPrincipal::isTrustedCertChainPrincipal(void)
{
  /* We destroy the cert array if cert chain didn't verify */
  if ((itsType != nsPrincipalType_CertChain) ||
      (itsCertArray == NULL))
    return PR_FALSE;
  return PR_TRUE;
}


/* 
 * This method is introduced to check the whether a given url base 
 * is a file based or any other type. Returns TRUE if the key is a
 * file based url. Returns FALSE otherwise.
 */
PRBool nsPrincipal::isFileCodeBase(void)
{

  if (itsKey == NULL)
	return PR_FALSE;

  if ((memcmp("file:", itsKey, strlen("file:"))) == 0)
    return PR_TRUE;

  return PR_FALSE;
}

PRBool nsPrincipal::isCert(void)
{
  if (itsType == nsPrincipalType_Cert)
    return PR_TRUE;
  return PR_FALSE;
}


PRBool nsPrincipal::isCertFingerprint(void)
{
  if ((itsType == nsPrincipalType_CertFingerPrint) ||
      (itsType == nsPrincipalType_CertKey) ||
      (itsType == nsPrincipalType_CertChain))
    return PR_TRUE;
  return PR_FALSE;
}


char * nsPrincipal::toString(void)
{
  char * str;

  switch(itsType) {
  case nsPrincipalType_CertKey:
  case nsPrincipalType_CertChain:
    str = getNickname();
    break;

  case nsPrincipalType_Cert:
  case nsPrincipalType_CertFingerPrint:
  case nsPrincipalType_CodebaseExact:
    if (itsString != NULL) {
      str = itsString;
    } else {
      PR_ASSERT(itsKey != NULL);
      str = itsKey;
    }
    break;
  default:
    str =  "Unknown Principal";
    break;
  }
  return str;
}


char * nsPrincipal::toVerboseString(void) 
{
  return toString();
}

char * nsPrincipal::savePrincipalPermanently(void)
{
  if ((isCodebase()) || (itsZig == NULL))
    return NULL;

  char * ret_value = saveCert();

  // Don't hold the reference to itsZig, once we have saved the 
  // principal's certificate permanently. Deleting this reference would
  // allow us to garbage collect ZIG object and thus free up memory.
  itsZig = NULL; 

  return ret_value;
}


/* The following used to be LJ_GetCertificates */
nsPrincipalArray* nsPrincipal::getSigners(void* zigPtr, char* pathname)
{
  SOBITEM *item;
  ZIG *zig = (ZIG *)zigPtr;
  struct nsPrincipal *principal;
  ZIG_Context * context;
  FINGERZIG *fingPrint;
  int size=0;
  int slot=0;

  if (!pathname)
    return NULL;

  if (!zig) {
    return NULL;
  }

  /* count the number of signers */ 
  if ((context = SOB_find(zig, pathname, ZIG_SIGN)) == NULL)
    return NULL;
  while (SOB_find_next (context, &item) >= 0) {
    size++;
  }
  SOB_find_end(context);

  /* Now allocate the array */
  nsPrincipalArray *result = new nsPrincipalArray();
  result->SetSize(size, 1);
  if (result == NULL) {
    return NULL;
  }

  if ((context = SOB_find(zig, pathname, ZIG_SIGN)) == NULL) {
    return NULL;
  }
  while (SOB_find_next(context, &item) >= 0) {
    PR_ASSERT(slot < size);

    /* Allocate the Cert's FP and put them in an array */
    fingPrint = (FINGERZIG *) item->data;
    principal = nsCapsNewPrincipal(nsPrincipalType_CertKey, 
                                   fingPrint->key, 
                                   fingPrint->length,
                                   zig);
    nsCapsSetPrincipalArrayElement(result, slot++, principal);

  }
  SOB_find_end(context);
   
  return result;
}


//
// 			PRIVATE METHODS 
//

void nsPrincipal::init(nsPrincipalType type, void * key, PRUint32 key_len)
{
  switch(type) {
  case nsPrincipalType_Cert:
  case nsPrincipalType_CertKey:
  case nsPrincipalType_CertChain:
  case nsPrincipalType_CertFingerPrint:
  case nsPrincipalType_CodebaseExact:
    break;

  default:
    type = nsPrincipalType_Unknown;
  }

  itsType=type;
  itsKey = new char[key_len+1];
  memcpy(itsKey, key, key_len);
  itsKey[key_len] = '\0';
#ifdef DEBUG_raman
  fprintf(stderr, "Creating principal %d, %s\n", type, itsKey);
#endif /* DEBUG_raman */
  itsKeyLen = key_len;
  itsHashCode = computeHashCode();
  itsZig = NULL;
  itsCertArray = NULL;
  itsString = NULL;
  itsCompanyName = NULL;
  itsCertAuth = NULL;
  itsSerialNo = NULL;
  itsExpDate = NULL;
  itsAsciiFingerPrint = NULL;
  itsNickname = NULL;
}

PRInt32 nsPrincipal::computeHashCode(void * key, PRUint32 keyLen)
{
  char *cptr = (char *)key;
  //
  // Same basic hash algorithm as used in java.lang.String --
  // no security relevance, only a performance optimization.
  // The security comes from the equals() method.
  //
  int hashCode=0;
  for (PRUint32 i = 0; i < keyLen; i++)
    hashCode = hashCode * 37 + cptr[i];
  return hashCode;
}

PRInt32 nsPrincipal::computeHashCode(void)
{
  switch(itsType) {
  case nsPrincipalType_Cert:
  case nsPrincipalType_CertFingerPrint:
  case nsPrincipalType_CertKey:
  case nsPrincipalType_CertChain:
  case nsPrincipalType_CodebaseExact:
    return computeHashCode(itsKey, itsKeyLen);
  default:
    return -1;
  }
  return -1;
}

char * nsPrincipal::saveCert(void)
{
  int result;
  if (itsType == nsPrincipalType_CertChain) {
    return NULL;
  }
  if ((!itsZig) || (!itsKey)) {
    return NULL;
  }

  result = SOB_stash_cert((ZIG *)itsZig, itsKeyLen, itsKey);
  if (result < 0) {
    return SOB_get_error(result);
  }
  return NULL;
}

/* The caller is responsible for free'ing the memory */
char *
nsPrincipal::getCertAttribute(int attrib)
{
    void *result;
    unsigned long length;
    char *attrStr;
    ZIG *zig = NULL;

    if (itsZig != NULL) {
      zig = (ZIG *)itsZig;
    }

    if (itsType == nsPrincipalType_CertChain) {
      char *attributeStr;
      if (itsCertArray == NULL) {
        return "Untrusted certificate (unknown attributes)";
      }
      CERTCertificate *cert = (CERTCertificate *)itsCertArray->Get(0);
      switch (attrib) {
#ifdef MOZ_SECURITY
      case ZIG_C_COMPANY:
        attributeStr = SECNAV_GetJarCertInfo(cert, snjSubjectName);
        break;
      case ZIG_C_CA:
        attributeStr = SECNAV_GetJarCertInfo(cert, snjIssuerName);
        break;
      case ZIG_C_SERIAL:
        attributeStr = SECNAV_GetJarCertInfo(cert, snjSerialNumber);
        break;
      case ZIG_C_EXPIRES:
        attributeStr = SECNAV_GetJarCertInfo(cert, snjExpirationDate);
        break;
      case ZIG_C_NICKNAME:
        attributeStr = SECNAV_GetJarCertInfo(cert, snjCommonName);
        break;
      case ZIG_C_FP:
        attributeStr = SECNAV_GetJarCertInfo(cert, snjFingerprint);
        break;
#endif /* MOZ_SECURITY */
      default:
        return NULL;
      }
      if (attributeStr) {
          attrStr = new char[strlen(attributeStr)+1];
          XP_STRCPY(attrStr, attributeStr);
         PR_FREEIF(attributeStr);
         return attrStr;
      } else {
         return "Untrusted certificate (unknown attributes)";
      }
    }
    
    if (SOB_cert_attribute(attrib, zig, 
                           itsKeyLen, itsKey, 
                           &result, &length) < 0) {
      /* We need to print the message "invalid certificate fingerprint" */
      return NULL;
    }
    attrStr = new char[length+1];
    memcpy(attrStr, result, length);
    attrStr[length] = '\0';
    /* Should be SOB_FREE(result); */
    XP_FREE(result);
    return attrStr;
}


