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

#include <mapidefs.h>

#include "msgMapiDefs.h"
#include "msgMapi.h"
#include "msgMapiImp.h"
#include "msgMapiFactory.h"
#include "msgMapiMain.h"

#include "msgMapiHook.h"
#include "nsString.h"

const CLSID CLSID_nsMapiImp = {0x29f458be, 0x8866, 0x11d5, {0xa3, 0xdd, 0x0, 0xb0, 0xd0, 0xf3, 0xba, 0xa7}};

nsMapiImp::nsMapiImp()
: m_cRef(1)
{
    m_Lock = PR_NewLock();
}

nsMapiImp::~nsMapiImp() 
{ 
    if (m_Lock)
        PR_DestroyLock(m_Lock);
}

STDMETHODIMP nsMapiImp::QueryInterface(const IID& aIid, void** aPpv)
{    
    if (aIid == IID_IUnknown)
    {
        *aPpv = static_cast<nsIMapi*>(this); 
    }
    else if (aIid == IID_nsIMapi)
    {
        *aPpv = static_cast<nsIMapi*>(this);
    }
    else
    {
        *aPpv = nsnull;
        return E_NOINTERFACE;
    }

    reinterpret_cast<IUnknown*>(*aPpv)->AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) nsMapiImp::AddRef()
{
    return PR_AtomicIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) nsMapiImp::Release() 
{
    PRInt32 temp;
    temp = PR_AtomicDecrement(&m_cRef);
    if (m_cRef == 0)
    {
        delete this;
        return 0;
    }

    return temp;
}

STDMETHODIMP nsMapiImp::Initialize()
{
    HRESULT hr = E_FAIL;

    if (!m_Lock)
        return E_FAIL;

    PR_Lock(m_Lock);

    // Initialize MAPI Configuration

    nsMAPIConfiguration *pConfig = nsMAPIConfiguration::GetMAPIConfiguration();
    if (pConfig != nsnull)
        if (nsMapiHook::Initialize())
            hr = S_OK;

    PR_Unlock(m_Lock);

    return hr;
}

STDMETHODIMP nsMapiImp::Login(unsigned long aUIArg, LOGIN_PW_TYPE aLogin, LOGIN_PW_TYPE aPassWord,
                unsigned long aFlags, unsigned long *aSessionId)
{
    HRESULT hr = E_FAIL;

    nsString tempProfileName;
    nsString tempPassCode;

    PRBool bNewSession = PR_FALSE;
    PRBool bLoginUI = PR_FALSE;
    PRBool bPasswordUI = PR_FALSE;
    PRBool bResult = PR_FALSE;

    char *id_key = nsnull;

    // See wheather user wants a new session with the same user name.

    if (aFlags & MAPI_NEW_SESSION)
        bNewSession = PR_TRUE;

    // Check For Profile Name

    if (aLogin == nsnull || aLogin[0] == '\0')
    {
        // no user name is passed by the user

        if (aFlags & MAPI_LOGON_UI)   // asked to show the login dialog
            bLoginUI = PR_TRUE;
        else
        {
            // user name not passed and not opted to show the logon UI.
            // it is an error.

            *aSessionId = MAPI_E_FAILURE;  
            return hr;
        }
    }
    else
        tempProfileName.Assign ((PRUnichar *) aLogin);

    // Check For Password

    if (aPassWord == nsnull || aPassWord[0] == '\0')
    {
        // no password.. is opted for Password UI.

        if ((aFlags & MAPI_PASSWORD_UI) && !bPasswordUI)
            bPasswordUI = PR_TRUE;
        
        // Looking for 'else' !!.  Don't know wheather password is set! :-)
    }
    else
        tempPassCode.Assign ((PRUnichar *) aPassWord);

    if (bLoginUI)
    {
        // Display the Login UI

        PRUnichar *Name = nsnull, *Pass = nsnull;

        bResult = nsMapiHook::DisplayLoginDialog(PR_TRUE, &Name, &Pass);
        if (bResult == PR_FALSE)
        {
            *aSessionId = MAPI_E_USER_ABORT;
            return hr;
        }

        tempProfileName.Assign (Name);
        tempPassCode.Assign (Pass);

        delete(Name);
        delete(Pass);
    }
    else if (bPasswordUI)
    {
        // Display the Password UI

        PRUnichar *Name = (PRUnichar *)tempProfileName.get() ; 
        PRUnichar *Pass = nsnull;

        bResult = nsMapiHook::DisplayLoginDialog(PR_FALSE, &Name, &Pass);
        if (bResult == PR_FALSE)
        {
            *aSessionId = MAPI_E_USER_ABORT;
            return hr;
        }

        tempPassCode.Assign (Pass) ;

        delete(Pass);
    }

    // No matter what ever the params are; Profile Name must be resloved by now

    if (tempProfileName.Length() <= 0)
    {
        *aSessionId = MAPI_E_LOGIN_FAILURE;
        return hr;
    }

    // Verify wheather username exists in the current mozilla profile.

    bResult = nsMapiHook::VerifyUserName(tempProfileName.get(), &id_key);
    if (bResult == PR_FALSE)
    {
        *aSessionId = MAPI_E_LOGIN_FAILURE;
        return hr;
    }

    // finally register(create) the session.

    
    PRUint32 nSession_Id;
    PRInt16 nResult = 0;

    nsMAPIConfiguration *pConfig = nsMAPIConfiguration::GetMAPIConfiguration();
    if (pConfig != nsnull)
        nResult = pConfig->RegisterSession(aUIArg, tempProfileName.get(),
                                           tempPassCode.get(),
                                           (aFlags & MAPI_FORCE_DOWNLOAD), bNewSession,
                                           &nSession_Id, id_key);

    switch (nResult)
    {
        case -1 :
        {
            *aSessionId = MAPI_E_TOO_MANY_SESSIONS;
            return hr;
        }
        case 0 :
        {
            *aSessionId = MAPI_E_INSUFFICIENT_MEMORY;
            return hr;
        }
        default :
        {
            *aSessionId = nSession_Id;
            break;
        }
    }

    return S_OK;
}

STDMETHODIMP nsMapiImp::Logoff (unsigned long aSession)
{
    nsMAPIConfiguration *pConfig = nsMAPIConfiguration::GetMAPIConfiguration();

    if (pConfig->UnRegisterSession((PRUint32)aSession))
        return S_OK;

    return E_FAIL;
}

STDMETHODIMP nsMapiImp::CleanUp()
{
    nsMapiHook::CleanUp();
    return S_OK;
}
