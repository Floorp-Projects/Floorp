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

#ifndef MSG_MAPI_IMP_H
#define MSG_MAPI_IMP_H

#include <windows.h>
#include <mapi.h>
#include "msgMapi.h"
#include "nsXPIDLString.h"
#include "nspr.h"

const CLSID CLSID_CMapiImp = {0x29f458be, 0x8866, 0x11d5, {0xa3, 0xdd, 0x0, 0xb0, 0xd0, 0xf3, 0xba, 0xa7}};

// this class implements the MS COM interface nsIMapi that provides the methods
// called by mapi32.dll to perform the mail operations as specified by MAPI.
// These class methods in turn use the Mozilla Mail XPCOM interfaces to do so.
class CMapiImp : public nsIMapi
{

public :

    // IUnknown

    STDMETHODIMP            QueryInterface(const IID& aIid, void** aPpv);
    STDMETHODIMP_(ULONG)    AddRef();
    STDMETHODIMP_(ULONG)    Release();

    // Interface INsMapi

    STDMETHODIMP Login(unsigned long aUIArg, LOGIN_PW_TYPE aLogin, 
                       LOGIN_PW_TYPE aPassWord, unsigned long aFlags,
                       unsigned long *aSessionId);

    STDMETHODIMP SendMail( unsigned long aSession, lpnsMapiMessage aMessage,
         short aRecipCount, lpnsMapiRecipDesc aRecips , 
         short aFileCount, lpnsMapiFileDesc aFiles , 
         unsigned long aFlags, unsigned long aReserved) ;

    STDMETHODIMP SendDocuments( unsigned long aSession, LPTSTR aDelimChar,
                                LPTSTR aFilePaths, LPTSTR aFileNames, ULONG aFlags);

    STDMETHODIMP Initialize();
    STDMETHODIMP IsValid();
    STDMETHODIMP IsValidSession(unsigned long aSession);

    STDMETHODIMP Logoff (unsigned long aSession);
    STDMETHODIMP CleanUp();

    CMapiImp();
    ~CMapiImp();

private :

    PRLock *m_Lock;
    PRInt32 m_cRef;
};

#endif // MSG_MAPI_IMP_H
