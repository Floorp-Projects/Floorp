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

#include "msgMapiSupport.h"
#include "nsISupports.h"

const CLSID CLSID_nsMapiImp = {0x29f458be, 0x8866, 0x11d5, \
                              {0xa3, 0xdd, 0x0, 0xb0, 0xd0, 0xf3, 0xba, 0xa7}};

PRBool WINAPI DllMain(HINSTANCE aInstance, DWORD aReason, LPVOID aReserved)
{
    switch (aReason)
    {
        case DLL_PROCESS_ATTACH :
        {
            // Initialize MAPI Support

            nsMapiSupport *pTemp = nsMapiSupport::GetNsMapiSupport();
            break;
        }
        case DLL_PROCESS_DETACH :
        {
            nsMapiSupport *pTemp = nsMapiSupport::GetNsMapiSupport();
            pTemp->UnInitMSCom();
            delete pTemp;
            pTemp = nsnull;
        }
    }

    return PR_TRUE;
}

extern "C"
{
    void __declspec(dllexport) Init()
    {
        nsMapiSupport *pTemp = nsMapiSupport::GetNsMapiSupport();
        if (pTemp != nsnull)
            pTemp->InitMSCom();
    }
}

nsMapiSupport* nsMapiSupport::m_pSelfRef = nsnull;

nsMapiSupport *nsMapiSupport::GetNsMapiSupport()
{
    if (m_pSelfRef == nsnull)
        m_pSelfRef = new nsMapiSupport();

    return m_pSelfRef;
}

nsMapiSupport::nsMapiSupport()
: m_dwRegister(0),
  m_nsMapiFactory(nsnull)
{
}

nsMapiSupport::~nsMapiSupport()
{
}

PRBool nsMapiSupport::RegsiterComponents()
{
    if (m_nsMapiFactory == nsnull)    // No Registering if already done.  Sanity Check!!
    {
        m_nsMapiFactory = new nsMapiFactory();

        if (m_nsMapiFactory != nsnull)
        {
            HRESULT hr = ::CoRegisterClassObject(CLSID_nsMapiImp, \
                                                 m_nsMapiFactory, \
                                                 CLSCTX_LOCAL_SERVER, \
                                                 REGCLS_MULTIPLEUSE, \
                                                 &m_dwRegister);

            if (FAILED(hr))
            {
                m_nsMapiFactory->Release() ;
                m_nsMapiFactory = nsnull;
                return PR_FALSE;
            }
        }
    }

    return PR_TRUE;
}

PRBool nsMapiSupport::UnRegisterComponents()
{
    if (m_dwRegister != 0)
        ::CoRevokeClassObject(m_dwRegister);

    if (m_nsMapiFactory != nsnull)
    {
        m_nsMapiFactory->Release();
        m_nsMapiFactory = nsnull;
    }

    return PR_TRUE;
}

void nsMapiSupport::InitMSCom()
{
    ::CoInitialize(nsnull);
    RegsiterComponents();
}

void nsMapiSupport::UnInitMSCom()
{
    UnRegisterComponents();
    ::CoUninitialize();
}
