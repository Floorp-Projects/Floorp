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
 * Contributor(s):
 *                 Krishna Mohan Khandrika (kkhandrika@netscape.com)
 *                 Rajiv Dayal (rdayal@netscape.com)
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
#include <mapi.h>
#include "msgMapi.h"
#include "msgMapiImp.h"
#include "msgMapiFactory.h"
#include "msgMapiMain.h"

#include "nsMsgCompFields.h"
#include "msgMapiHook.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsMsgCompCID.h"



CMapiImp::CMapiImp()
: m_cRef(1)
{
    m_Lock = PR_NewLock();
}

CMapiImp::~CMapiImp() 
{ 
    if (m_Lock)
        PR_DestroyLock(m_Lock);
}

STDMETHODIMP CMapiImp::QueryInterface(const IID& aIid, void** aPpv)
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

STDMETHODIMP_(ULONG) CMapiImp::AddRef()
{
    return PR_AtomicIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CMapiImp::Release() 
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

STDMETHODIMP CMapiImp::IsValid()
{
    return S_OK;
}

STDMETHODIMP CMapiImp::IsValidSession(unsigned long aSession)
{
    nsMAPIConfiguration *pConfig = nsMAPIConfiguration::GetMAPIConfiguration();
    if (pConfig && pConfig->IsSessionValid(aSession))
        return S_OK;

    return E_FAIL;
}

STDMETHODIMP CMapiImp::Initialize()
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

STDMETHODIMP CMapiImp::Login(unsigned long aUIArg, LOGIN_PW_TYPE aLogin, LOGIN_PW_TYPE aPassWord,
                unsigned long aFlags, unsigned long *aSessionId)
{
    HRESULT hr = E_FAIL;
    PRBool bNewSession = PR_FALSE;
    char *id_key = nsnull;

    if (aFlags & MAPI_NEW_SESSION)
        bNewSession = PR_TRUE;

    // Check For Profile Name
    if (aLogin != nsnull && aLogin[0] != '\0')
    {
        if (nsMapiHook::VerifyUserName(aLogin, &id_key) == PR_FALSE)
        {
            *aSessionId = MAPI_E_LOGIN_FAILURE;
            return hr;
        }
    }

    // finally register(create) the session.
    PRUint32 nSession_Id;
    PRInt16 nResult = 0;

    nsMAPIConfiguration *pConfig = nsMAPIConfiguration::GetMAPIConfiguration();
    if (pConfig != nsnull)
        nResult = pConfig->RegisterSession(aUIArg, aLogin, aPassWord,
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

STDMETHODIMP CMapiImp::SendMail( unsigned long aSession, lpnsMapiMessage aMessage,
     short aRecipCount, lpnsMapiRecipDesc aRecips , short aFileCount, lpnsMapiFileDesc aFiles , 
     unsigned long aFlags, unsigned long aReserved)
{
    nsresult rv = NS_OK ;

    // Assign the pointers in the aMessage struct to the array of Recips and Files
    // recieved here from MS COM. These are used in BlindSendMail and ShowCompWin fns 
    aMessage->lpRecips = aRecips ;
    aMessage->lpFiles = aFiles ;

    /** create nsIMsgCompFields obj and populate it **/
    nsCOMPtr<nsIMsgCompFields> pCompFields = do_CreateInstance(NS_MSGCOMPFIELDS_CONTRACTID, &rv) ;
    if (NS_FAILED(rv) || (!pCompFields) ) return MAPI_E_INSUFFICIENT_MEMORY ;

    if (aFlags & MAPI_UNICODE)
        rv = nsMapiHook::PopulateCompFields(aMessage, pCompFields) ;
    else
        rv = nsMapiHook::PopulateCompFieldsWithConversion(aMessage, pCompFields) ;
    
    if (NS_SUCCEEDED (rv))
    {
        // see flag to see if UI needs to be brought up
        if (!(aFlags & MAPI_DIALOG))
        {
            rv = nsMapiHook::BlindSendMail(aSession, pCompFields);
        }
        else
        {
            rv = nsMapiHook::ShowComposerWindow(aSession, pCompFields);
        }
    }
    
    return nsMAPIConfiguration::GetMAPIErrorFromNSError (rv) ;
}


STDMETHODIMP CMapiImp::SendDocuments( unsigned long aSession, LPTSTR aDelimChar,
                            LPTSTR aFilePaths, LPTSTR aFileNames, ULONG aFlags)
{
    nsresult rv = NS_OK ;

    /** create nsIMsgCompFields obj and populate it **/
    nsCOMPtr<nsIMsgCompFields> pCompFields = do_CreateInstance(NS_MSGCOMPFIELDS_CONTRACTID, &rv) ;
    if (NS_FAILED(rv) || (!pCompFields) ) return MAPI_E_INSUFFICIENT_MEMORY ;

    if (aFilePaths)
    {
        rv = nsMapiHook::PopulateCompFieldsForSendDocs(pCompFields, aFlags, aDelimChar, aFilePaths) ;
    }

    if (NS_SUCCEEDED (rv)) 
        rv = nsMapiHook::ShowComposerWindow(aSession, pCompFields);

    return nsMAPIConfiguration::GetMAPIErrorFromNSError (rv) ;
}

STDMETHODIMP CMapiImp::Logoff (unsigned long aSession)
{
    nsMAPIConfiguration *pConfig = nsMAPIConfiguration::GetMAPIConfiguration();

    if (pConfig->UnRegisterSession((PRUint32)aSession))
        return S_OK;

    return E_FAIL;
}

STDMETHODIMP CMapiImp::CleanUp()
{
    nsMapiHook::CleanUp();
    return S_OK;
}


