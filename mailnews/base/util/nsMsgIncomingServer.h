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

#include "nsIMsgIncomingServer.h"
#include "nsIPref.h"

/*
 * base class for nsIMsgIncomingServer - derive your class from here
 * if you want to get some free implementation
 * 
 * this particular implementation is not meant to be used directly.
 */

class nsMsgIncomingServer : public nsIMsgIncomingServer {
 public:
  NS_DECL_ISUPPORTS

  nsMsgIncomingServer();
  virtual ~nsMsgIncomingServer();

  /* attribute string key; */
  NS_IMETHOD GetKey(char * *aKey);
  NS_IMETHOD SetKey(char * aKey);
  
   /* attribute string prettyName; */
  NS_IMETHOD GetPrettyName(char * *aPrettyName);
  NS_IMETHOD SetPrettyName(char * aPrettyName);

  /* attribute string hostName; */
  NS_IMETHOD GetHostName(char * *aHostName);
  NS_IMETHOD SetHostName(char * aHostName);

  /* attribute string userName; */
  NS_IMETHOD GetUserName(char * *aUserName);
  NS_IMETHOD SetUserName(char * aUserName);

  /* attribute boolean rememberPassword; */
  NS_IMETHOD GetRememberPassword(PRBool *aRememberPassword);
  NS_IMETHOD SetRememberPassword(PRBool aRememberPassword);
  
  NS_IMETHOD GetPassword(char * *aPassword);
  NS_IMETHOD SetPassword(char * aPassword);
  
  /* attribute boolean doBiff; */
  NS_IMETHOD GetDoBiff(PRBool *aDoBiff);
  NS_IMETHOD SetDoBiff(PRBool aDoBiff);

  /* attribute long biffMinutes; */
  NS_IMETHOD GetBiffMinutes(PRInt32 *aBiffMinutes);
  NS_IMETHOD SetBiffMinutes(PRInt32 aBiffMinutes);

  /* attribute boolean downloadOnBiff; */
  NS_IMETHOD GetDownloadOnBiff(PRBool *aDownloadOnBiff);
  NS_IMETHOD SetDownloadOnBiff(PRBool aDownloadOnBiff);

  NS_IMETHOD GetLocalPath(char * *aLocalPath);
  NS_IMETHOD SetLocalPath(char * aLocalPath);
  
private:
  nsIPref *m_prefs;
  char *m_serverKey;

protected:
  char *getPrefName(const char *serverKey, const char *pref);
  char *getDefaultPrefName(const char *pref);
  
  nsresult getCharPref(const char *pref, char **);
  nsresult getDefaultCharPref(const char *pref, char **);
  nsresult setCharPref(const char *pref, char *);
  
  nsresult getBoolPref(const char *pref, PRBool *);
  nsresult getDefaultBoolPref(const char *pref, PRBool *);
  nsresult setBoolPref(const char *pref, PRBool);
  
  nsresult getIntPref(const char *pref, PRInt32 *);
  nsresult getDefaultIntPref(const char *pref, PRInt32 *);
  nsresult setIntPref(const char *pref, PRInt32);
};

/* some useful macros to implement accessors */
#define NS_IMPL_SERVERPREF_STR(_class, _postfix, _prefname)	\
NS_IMETHODIMP								   	\
_class::Get##_postfix(char **retval)   			\
{											   	\
  return getCharPref(_prefname, retval);		\
}												\
NS_IMETHODIMP	   								\
_class::Set##_postfix(char *value)	\
{												\
  return setCharPref(_prefname, value);			\
}

#define NS_IMPL_SERVERPREF_BOOL(_class, _postfix, _prefname)\
NS_IMETHODIMP								   	\
_class::Get##_postfix(PRBool *retval)   		\
{											   	\
  return getBoolPref(_prefname, retval);		\
}												\
NS_IMETHODIMP	   								\
_class::Set##_postfix(PRBool value)				\
{												\
  return setBoolPref(_prefname, value);			\
}

#define NS_IMPL_SERVERPREF_INT(_class, _postfix, _prefname)\
NS_IMETHODIMP								   	\
_class::Get##_postfix(PRInt32 *retval)   		\
{											   	\
  return getIntPref(_prefname, retval);			\
}												\
NS_IMETHODIMP	   								\
_class::Set##_postfix(PRInt32 value)			\
{												\
  return setIntPref(_prefname, value);			\
}

