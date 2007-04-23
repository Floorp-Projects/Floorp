/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher A. Aillon <christopher@aillon.com>
 *   Giorgio Maone <g.maone@informaction.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsPrincipal_h__
#define nsPrincipal_h__

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsHashtable.h"
#include "nsJSPrincipals.h"

class nsIObjectInputStream;
class nsIObjectOutputStream;

class nsPrincipal : public nsIPrincipal
{
public:
  nsPrincipal();

protected:
  virtual ~nsPrincipal();

public:
  // Our refcount is managed by mJSPrincipals.  Use this macro to avoid
  // an extra refcount member.
  NS_DECL_ISUPPORTS_INHERITED
public:

  NS_DECL_NSIPRINCIPAL
  NS_DECL_NSISERIALIZABLE

  // Either Init() or InitFromPersistent() must be called before
  // the principal is in a usable state.
  nsresult Init(const nsACString& aCertFingerprint,
                const nsACString& aSubjectName,
                const nsACString& aPrettyName,
                nsISupports* aCert,
                nsIURI *aCodebase);
  nsresult InitFromPersistent(const char* aPrefName,
                              const nsCString& aFingerprint,
                              const nsCString& aSubjectName,
                              const nsACString& aPrettyName,
                              const char* aGrantedList,
                              const char* aDeniedList,
                              nsISupports* aCert,
                              PRBool aIsCert,
                              PRBool aTrusted);

  // Call this to ensure that this principal has a subject name, a pretty name,
  // and a cert pointer.  This method will throw if there is already a
  // different subject name or if this principal has no certificate.
  nsresult EnsureCertData(const nsACString& aSubjectName,
                          const nsACString& aPrettyName,
                          nsISupports* aCert);

  enum AnnotationValue { AnnotationEnabled=1, AnnotationDisabled };

  void SetURI(nsIURI *aURI);
  nsresult SetCapability(const char *capability, void **annotation, 
                         AnnotationValue value);

  static const char sInvalid[];

protected:
  nsJSPrincipals mJSPrincipals;
  nsVoidArray mAnnotations;
  nsHashtable mCapabilities;
  nsCString mPrefName;
  static PRInt32 sCapabilitiesOrdinal;

  // XXXcaa This is a semi-hack.  The best solution here is to keep
  // a reference to an interface here, except there is no interface
  // that we can use yet.
  struct Certificate
  {
    Certificate(const nsACString& aFingerprint, const nsACString& aSubjectName,
                const nsACString& aPrettyName, nsISupports* aCert)
      : fingerprint(aFingerprint),
        subjectName(aSubjectName),
        prettyName(aPrettyName),
        cert(aCert)
    {
    }
    nsCString fingerprint;
    nsCString subjectName;
    nsCString prettyName;
    nsCOMPtr<nsISupports> cert;
  };

  nsresult SetCertificate(const nsACString& aFingerprint,
                          const nsACString& aSubjectName,
                          const nsACString& aPrettyName,
                          nsISupports* aCert);

  // Keep this is a pointer, even though it may slightly increase the
  // cost of keeping a certificate, this is a good tradeoff though since
  // it is very rare that we actually have a certificate.
  nsAutoPtr<Certificate> mCert;

  DomainPolicy* mSecurityPolicy;

  nsCOMPtr<nsIURI> mCodebase;
  nsCOMPtr<nsIURI> mDomain;
  nsCOMPtr<nsIURI> mOrigin;
  PRPackedBool mTrusted;
  PRPackedBool mInitialized;
};


#define NS_PRINCIPAL_CLASSNAME  "principal"
#define NS_PRINCIPAL_CONTRACTID "@mozilla.org/principal;1"
#define NS_PRINCIPAL_CID \
  { 0x36102b6b, 0x7b62, 0x451a, \
    { 0xa1, 0xc8, 0xa0, 0xd4, 0x56, 0xc9, 0x2d, 0xc5 }}


#endif // nsPrincipal_h__
