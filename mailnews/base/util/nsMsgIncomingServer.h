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
#include "msgCore.h"
#include "nsIFolder.h"
#include "nsCOMPtr.h"

class nsIMsgFolderCache;

/*
 * base class for nsIMsgIncomingServer - derive your class from here
 * if you want to get some free implementation
 * 
 * this particular implementation is not meant to be used directly.
 */

class NS_MSG_BASE nsMsgIncomingServer : public nsIMsgIncomingServer {
 public:
  nsMsgIncomingServer();
  virtual ~nsMsgIncomingServer();

  NS_DECL_ISUPPORTS

  NS_DECL_NSIMSGINCOMINGSERVER
  
private:
  nsIPref *m_prefs;
  char *m_serverKey;
  nsCString m_password;
  PRBool m_serverBusy;

protected:
  char *getPrefName(const char *serverKey, const char *pref);
  char *getDefaultPrefName(const char *pref);

  // these are private pref getters and setters for the password
  // field. Callers should be using Get/Set Password
  NS_IMETHOD GetPrefPassword(char * *aPassword);
  NS_IMETHOD SetPrefPassword(const char * aPassword);
  
  nsCOMPtr <nsIFolder> m_rootFolder;
  nsresult getDefaultCharPref(const char *pref, char **);
  nsresult getDefaultBoolPref(const char *pref, PRBool *);
  nsresult getDefaultIntPref(const char *pref, PRInt32 *);
  
  nsresult CreateRootFolder();

};

