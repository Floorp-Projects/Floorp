/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsMsgIdentity_h___
#define nsMsgIdentity_h___

#include "nsIMsgIdentity.h"
#include "nsIPref.h"
#include "msgCore.h"
#include "nsISmtpServer.h"
#include "nsWeakPtr.h"


class NS_MSG_BASE nsMsgIdentity : public nsIMsgIdentity
{
public:
  nsMsgIdentity();
  virtual ~nsMsgIdentity();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGIDENTITY
  
private:
  nsIMsgSignature* m_signature;
  nsIMsgVCard* m_vCard;
  char *m_identityKey;
  nsIPref *m_prefs;
  nsWeakPtr m_smtpServer;

protected:
  nsresult getPrefService();
  char *getPrefName(const char *identityKey, const char *pref);
  char *getDefaultPrefName(const char *pref);
  
  nsresult getCharPref(const char *pref, char **);
  nsresult getDefaultCharPref(const char *pref, char **);
  nsresult setCharPref(const char *pref, const char *);

  nsresult getUnicharPref(const char *pref, PRUnichar **);
  nsresult getDefaultUnicharPref(const char *pref, PRUnichar **);
  nsresult setUnicharPref(const char *pref, const PRUnichar *);

  nsresult getBoolPref(const char *pref, PRBool *);
  nsresult getDefaultBoolPref(const char *pref, PRBool *);
  nsresult setBoolPref(const char *pref, PRBool);

  nsresult getIntPref(const char *pref, PRInt32 *);
  nsresult getDefaultIntPref(const char *pref, PRInt32 *);
  nsresult setIntPref(const char *pref, PRInt32);

  nsresult getFolderPref(const char *pref, char **);
  nsresult setFolderPref(const char *pref, const char *);

private:
  static void clearPrefEnum(const char *aPref, void *aClosure);

  nsresult loadSmtpServer(nsISmtpServer**);
  
};


#define NS_IMPL_IDPREF_STR(_postfix, _prefname)	\
NS_IMETHODIMP								   	\
nsMsgIdentity::Get##_postfix(char **retval)   	\
{											   	\
  return getCharPref(_prefname, retval);		\
}												\
NS_IMETHODIMP	   								\
nsMsgIdentity::Set##_postfix(const char *value)		\
{												\
  return setCharPref(_prefname, value);\
}

#define NS_IMPL_IDPREF_WSTR(_postfix, _prefname)\
NS_IMETHODIMP								   	\
nsMsgIdentity::Get##_postfix(PRUnichar **retval)\
{											   	\
  return getUnicharPref(_prefname, retval);		\
}												\
NS_IMETHODIMP	   								\
nsMsgIdentity::Set##_postfix(const PRUnichar *value)\
{												\
  return setUnicharPref(_prefname, value);\
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

#define NS_IMPL_IDPREF_INT(_postfix, _prefname) \
NS_IMETHODIMP								   	\
nsMsgIdentity::Get##_postfix(PRInt32 *retval)   \
{											   	\
  return getIntPref(_prefname, retval);		\
}												\
NS_IMETHODIMP	   								\
nsMsgIdentity::Set##_postfix(PRInt32 value)		\
{												\
  return setIntPref(_prefname, value);			\
}


#define NS_IMPL_FOLDERPREF_STR(_postfix, _prefname)	\
NS_IMETHODIMP								   	\
nsMsgIdentity::Get##_postfix(char **retval)   	\
{											   	\
  return getFolderPref(_prefname, retval);		\
}												\
NS_IMETHODIMP	   								\
nsMsgIdentity::Set##_postfix(const char *value)		\
{												\
  return setFolderPref(_prefname, value);\
}

#endif /* nsMsgIdentity_h___ */
