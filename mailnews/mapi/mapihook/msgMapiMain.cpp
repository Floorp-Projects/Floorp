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
 * The Original Code is Mozilla
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Krishna Mohan Khandrika (kkhandrika@netscape.com)
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

#include "msgMapiMain.h"

// move to xpcom bug 81956.
class nsPRUintKey : public nsHashKey {
protected:
    PRUint32 mKey;
public:
    nsPRUintKey(PRUint32 key) : mKey(key) {}

    PRUint32 HashCode(void) const {
        return mKey;
    }

    PRBool Equals(const nsHashKey *aKey) const {
        return mKey == ((const nsPRUintKey *) aKey)->mKey;
    }
    nsHashKey *Clone() const {
        return new nsPRUintKey(mKey);
    }
    PRUint32 GetValue() { return mKey; }
};
//


nsMAPIConfiguration *nsMAPIConfiguration::m_pSelfRef = nsnull;
PRUint32 nsMAPIConfiguration::session_generator = 0;
PRUint32 nsMAPIConfiguration::sessionCount = 0;

nsMAPIConfiguration *nsMAPIConfiguration::GetMAPIConfiguration()
{
    if (m_pSelfRef == nsnull)
        m_pSelfRef = new nsMAPIConfiguration();

    return m_pSelfRef;
}

nsMAPIConfiguration::nsMAPIConfiguration()
: m_nMaxSessions(MAX_SESSIONS),
  m_ProfileMap(nsnull),
  m_SessionMap(nsnull)
{
    m_Lock = PR_NewLock();
    m_ProfileMap = new nsHashtable();
    m_SessionMap = new nsHashtable();
    NS_ASSERTION(m_SessionMap && m_ProfileMap, "hashtables not created");
}

static PRBool
FreeSessionMapEntries(nsHashKey *aKey, void *aData, void* aClosure)
{
    nsMAPISession *pTemp = (nsMAPISession*) aData;
    delete pTemp;
    pTemp = nsnull;
    return PR_TRUE;
}

static PRBool
FreeProfileMapEntries(nsHashKey *aKey, void *aData, void* aClosure)
{
    ProfileNameSessionIdMap *pTemp = (ProfileNameSessionIdMap *) aData;
    delete pTemp;
    pTemp = nsnull;
    return PR_TRUE;
}

nsMAPIConfiguration::~nsMAPIConfiguration()
{
    if (m_Lock)
        PR_DestroyLock(m_Lock);

    if (m_SessionMap)
    {
        m_SessionMap->Reset(FreeSessionMapEntries);
        delete m_SessionMap;
        m_SessionMap = nsnull;
    }

    if (m_ProfileMap)
    {
        m_ProfileMap->Reset(FreeProfileMapEntries);
        delete m_ProfileMap;
        m_ProfileMap = nsnull;
    }
}

void nsMAPIConfiguration::OpenConfiguration()
{
    // No. of max. sessions is set to MAX_SESSIONS.  In future
    // if it is decided to have configuration (registry)
    // parameter, this function can be used to set the
    // max sessions;

    return;
}

PRInt16 nsMAPIConfiguration::RegisterSession(PRUint32 aHwnd,
                const PRUnichar *aUserName, const PRUnichar *aPassword,
                PRBool aForceDownLoad, PRBool aNewSession,
                PRUint32 *aSession, char *aIdKey)
{
    PRInt16 nResult = 0;
    PRUint32 n_SessionId = 0;

    PR_Lock(m_Lock);

    // Check whether max sessions is exceeded

    if (sessionCount >= m_nMaxSessions)
    {
        PR_Unlock(m_Lock);
        return -1;
    }

    nsAutoString usernameString(aUserName);
    usernameString.AppendInt(aHwnd);
    nsStringKey usernameKey(usernameString.get());

    ProfileNameSessionIdMap *pProfTemp = nsnull;
    pProfTemp = (ProfileNameSessionIdMap *) m_ProfileMap->Get(&usernameKey);
    if (pProfTemp != nsnull)
    {
        n_SessionId = pProfTemp->sessionId;
        
        // check wheather is there any session. if not let us create one.

        nsPRUintKey sessionKey(n_SessionId);
        nsMAPISession *pCheckSession = (nsMAPISession *)m_SessionMap->Get(&sessionKey);
        if (pCheckSession == nsnull)
            n_SessionId = 0;
    }

    // create a new session ; if new session is specified OR there is no session

    if (aNewSession || n_SessionId == 0) // checking for n_SessionId is a concession
    {
        nsMAPISession *pTemp = nsnull;
        pTemp = new nsMAPISession(aHwnd, aUserName,
                               aPassword, aForceDownLoad, aIdKey);

        if (pTemp != nsnull)
        {
            if (pProfTemp == nsnull)
            {
                pProfTemp = new ProfileNameSessionIdMap;
                if (pProfTemp == nsnull)
                {
                    delete pTemp;
                    pTemp = nsnull;
                    PR_Unlock(m_Lock);
                    return 0;
                }

                pProfTemp->shareCount = 1;
            }
            else
                pProfTemp->shareCount++;

            session_generator++;

            // I don't think there will be (2 power 32) sessions alive
            // in a cycle.  This is an assumption

            if (session_generator == 0)
                session_generator++;

            nsPRUintKey sessionKey(session_generator);
            m_SessionMap->Put(&sessionKey, pTemp);

            pProfTemp->sessionId  = session_generator;
            m_ProfileMap->Put(&usernameKey, (void*)pProfTemp);

            *aSession = session_generator;
            sessionCount++;
            nResult = 1;
        }
    }
    else
    if (n_SessionId > 0)        // share the session;
    {
        nsPRUintKey sessionKey(n_SessionId);
        nsMAPISession *pTemp = (nsMAPISession *)m_SessionMap->Get(&sessionKey);
        if (pTemp != nsnull)
        {
            pTemp->IncrementSession();
            pProfTemp->shareCount++;
            *aSession = n_SessionId;
            nResult = 1;
        }
    }

    PR_Unlock(m_Lock);
    return nResult;
}

PRBool nsMAPIConfiguration::UnRegisterSession(PRUint32 aSessionID)
{
    PRBool bResult = PR_FALSE;

    PR_Lock(m_Lock);

    if (aSessionID != 0)
    {
        nsPRUintKey sessionKey(aSessionID);
        nsMAPISession *pTemp = (nsMAPISession *)m_SessionMap->Get(&sessionKey);

        if (pTemp != nsnull)
        {
            if (pTemp->DecrementSession() == 0)
            {
                nsAutoString usernameString(pTemp->m_pProfileName.get());
                usernameString.AppendInt(pTemp->m_hAppHandle);
                nsStringKey stringKey(usernameString.get());

                ProfileNameSessionIdMap *pProfTemp = nsnull;
                pProfTemp = (ProfileNameSessionIdMap *) m_ProfileMap->Get(&stringKey);
                pProfTemp->shareCount--;
                if (pProfTemp->shareCount == 0)
                {
                    m_ProfileMap->Remove(&stringKey);
                    delete pProfTemp;
                    pProfTemp = nsnull;
                }

                delete pTemp;
                pTemp = nsnull;

                m_SessionMap->Remove(&sessionKey);
                sessionCount--;
                bResult = PR_TRUE;
            }
        }
    }
    PR_Unlock(m_Lock);
    return bResult;
}

PRUnichar *nsMAPIConfiguration::GetPassWord(PRUint32 aSessionID)
{
    PRUnichar *pResult = nsnull;

    PR_Lock(m_Lock);

    if (aSessionID != 0)
    {
        nsPRUintKey sessionKey(aSessionID);
        nsMAPISession *pTemp = (nsMAPISession *)m_SessionMap->Get(&sessionKey);

        if (pTemp)
        {
            pResult = pTemp->GetPassWord();
        }
    }

    PR_Unlock(m_Lock);

    return pResult;
}

char *nsMAPIConfiguration::GetIdKey(PRUint32 aSessionID)
{
    char *pResult = nsnull;

    PR_Lock(m_Lock);

    if (aSessionID != 0)
    {
        nsPRUintKey sessionKey(aSessionID);
        nsMAPISession *pTemp = (nsMAPISession *)m_SessionMap->Get(&sessionKey);
        if (pTemp)
        {
           pResult = pTemp->GetIdKey();
        }
    }

    PR_Unlock(m_Lock);
    return pResult;
}



nsMAPISession::nsMAPISession(PRUint32 aHwnd, const PRUnichar *aUserName,\
                             const PRUnichar *aPassword, \
                             PRBool aForceDownLoad, char *aKey)
: m_bIsForcedDownLoad(aForceDownLoad),
  m_hAppHandle(aHwnd),
  m_nShared(1),
  m_pIdKey(aKey)
{
    m_pProfileName.Assign(aUserName);
    m_pPassword.Assign(aPassword);
}

nsMAPISession::~nsMAPISession()
{
    if (m_pIdKey != nsnull)
    {
        delete [] m_pIdKey;
        m_pIdKey = nsnull;
    }
}

PRUint32 nsMAPISession::IncrementSession()
{
    return ++m_nShared;
}

PRUint32 nsMAPISession::DecrementSession()
{
    return --m_nShared;
}

PRUint32 nsMAPISession::GetSessionCount()
{
    return m_nShared;
}

PRUnichar *nsMAPISession::GetPassWord()
{
    return (PRUnichar *)m_pPassword.get();
}

char *nsMAPISession::GetIdKey()
{
    return m_pIdKey;
}
