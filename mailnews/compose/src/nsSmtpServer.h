/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef __nsSmtpServer_h_
#define __nsSmtpServer_h_


#include "nsString.h"
#include "nsISmtpServer.h"
#include "nsWeakReference.h"

class nsSmtpServer : public nsISmtpServer,
                     public nsSupportsWeakReference
{
public:
    nsSmtpServer();
    virtual ~nsSmtpServer();
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSISMTPSERVER

private:
    nsCString mKey;

    nsresult getPrefString(const char *pref, nsCAutoString& result);
    nsresult getDefaultIntPref(nsIPref *prefs, PRInt32 defVal, const char *prefName, PRInt32 *val);
    nsCString m_password;
    static void clearPrefEnum(const char *aPref, void *aClosure);
};

#endif
