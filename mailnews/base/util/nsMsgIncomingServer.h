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

   /* attribute string prettyName; */
  NS_IMETHOD GetPrettyName(char * *aPrettyName);
  NS_IMETHOD SetPrettyName(char * aPrettyName);

  /* attribute string hostName; */
  NS_IMETHOD GetHostName(char * *aHostName);
  NS_IMETHOD SetHostName(char * aHostName);

  /* attribute string userName; */
  NS_IMETHOD GetUserName(char * *aUserName);
  NS_IMETHOD SetUserName(char * aUserName);

  NS_IMETHOD GetPassword(char * *aPassword);
  NS_IMETHOD SetPassword(char * aPassword);
  
  /* attribute boolean doBiff; */
  NS_IMETHOD GetDoBiff(PRBool *aDoBiff);
  NS_IMETHOD SetDoBiff(PRBool aDoBiff);

  /* attribute long biffMinutes; */
  NS_IMETHOD GetBiffMinutes(PRInt32 *aBiffMinutes);
  NS_IMETHOD SetBiffMinutes(PRInt32 aBiffMinutes);

  NS_IMETHOD LoadPreferences(nsIPref *prefs, const char *serverKey);
  
 private:
  char *m_prettyName;
  char *m_hostName;
  char *m_userName;
  char *m_password;
  
  PRBool m_doBiff;
  PRInt32 m_biffMinutes;

protected:
  char *getPrefName(const char *serverKey, const char *pref);
  char *getCharPref(nsIPref *prefs, const char *serverKey, const char *pref);
  PRBool getBoolPref(nsIPref *prefs, const char *serverKey, const char *pref);
  PRInt32 getIntPref(nsIPref *prefs, const char *serverKey, const char *pref);
  

  
};

