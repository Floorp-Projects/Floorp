/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsMsgIdentity_h___
#define nsMsgIdentity_h___

#include "nsIMsgIdentity.h"
#include "nsIPref.h"

///////////////////////////////////////////////////////////////////////////////////
// an identity is an object designed to encapsulate all the information we need 
// to know about a user identity. I expect this interface to grow and change a lot
// as we flesh out our thoughts on multiple identities and what properties go into
// these identities.
//////////////////////////////////////////////////////////////////////////////////


class nsMsgIdentity : public nsIMsgIdentity
{
public:
  nsMsgIdentity();
  virtual ~nsMsgIdentity();
  
  NS_DECL_ISUPPORTS
  
   /* attribute string identityName; */
  NS_IMETHOD GetIdentityName(char * *aIdentityName);
  NS_IMETHOD SetIdentityName(char * aIdentityName);

  /* attribute string fullName; */
  NS_IMETHOD GetFullName(char * *aFullName);
  NS_IMETHOD SetFullName(char * aFullName);

  /* attribute string email; */
  NS_IMETHOD GetEmail(char * *aEmail);
  NS_IMETHOD SetEmail(char * aEmail);

  /* attribute string replyTo; */
  NS_IMETHOD GetReplyTo(char * *aReplyTo);
  NS_IMETHOD SetReplyTo(char * aReplyTo);

  /* attribute string organization; */
  NS_IMETHOD GetOrganization(char * *aOrganization);
  NS_IMETHOD SetOrganization(char * aOrganization);

  /* attribute boolean useHtml; */
  NS_IMETHOD GetUseHtml(PRBool *aUseHtml);
  NS_IMETHOD SetUseHtml(PRBool aUseHtml);

  /* attribute nsIMsgSignature signature; */
  NS_IMETHOD GetSignature(nsIMsgSignature * *aSignature);
  NS_IMETHOD SetSignature(nsIMsgSignature * aSignature);

  /* attribute nsIMsgVCard vCard; */
  NS_IMETHOD GetVCard(nsIMsgVCard * *aVCard);
  NS_IMETHOD SetVCard(nsIMsgVCard * aVCard);

  /* attribute string smtpHostname; */
  NS_IMETHOD GetSmtpHostname(char * *aSmtpHostname);
  NS_IMETHOD SetSmtpHostname(char * aSmtpHostname);

  /* attribute string smtpUsername; */
  NS_IMETHOD GetSmtpUsername(char * *aSmtpUsername);
  NS_IMETHOD SetSmtpUsername(char * aSmtpUsername);
 
  /* attribute string key; */
  NS_IMETHOD GetKey(char * *aKey);
  NS_IMETHOD SetKey(char * aKey);

private:
  nsIMsgSignature* m_signature;
  nsIMsgVCard* m_vCard;
  char *m_identityKey;
  nsIPref *m_prefs;
  
protected:
  static char *getPrefName(const char *identityKey, const char *pref);
  nsresult getCharPref(const char *pref, char **);
  nsresult setCharPref(const char *pref, char *);
  nsresult getBoolPref(const char *pref, PRBool *);
  nsresult setBoolPref(const char *pref, PRBool);
  
};


#define NS_IMPL_IDPREF_STR(_postfix, _prefname)	\
NS_IMETHODIMP								   	\
nsMsgIdentity::Get##_postfix(char **retval)   	\
{											   	\
  return getCharPref(_prefname, retval);		\
}												\
NS_IMETHODIMP	   								\
nsMsgIdentity::Set##_postfix(char *value)		\
{												\
  return setCharPref(_prefname, value);\
}

#define NS_IMPL_IDPREF_BOOL(_postfix, _prefname)\
NS_IMETHODIMP								   	\
nsMsgIdentity::Get##_postfix(PRBool *retval)   	\
{											   	\
  return getBoolPref(_prefname, retval);		\
}												\
NS_IMETHODIMP	   								\
nsMsgIdentity::Set##_postfix(PRBool value)		\
{												\
  return setBoolPref(_prefname, value);			\
}


#endif /* nsMsgIdentity_h___ */
