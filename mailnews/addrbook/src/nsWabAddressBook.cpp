/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Created by Cyrille Moureaux <Cyrille.Moureaux@sun.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsWabAddressBook.h"
#include "nsAbUtils.h"
#include "nsAutoLock.h"
#include "prlog.h"

#ifdef PR_LOGGING
static PRLogModuleInfo* gWabAddressBookLog
    = PR_NewLogModule("nsWabAddressBookLog");
#endif

#define PRINTF(args) PR_LOG(nsWabAddressBookLog, PR_LOG_DEBUG, args)

HMODULE nsWabAddressBook::mLibrary = NULL ;
PRInt32 nsWabAddressBook::mLibUsage = 0 ;
LPWABOPEN nsWabAddressBook::mWABOpen = NULL ;
LPWABOBJECT nsWabAddressBook::mRootSession = NULL ;
LPADRBOOK nsWabAddressBook::mRootBook = NULL ;

BOOL nsWabAddressBook::LoadWabLibrary(void)
{
    if (mLibrary) { ++ mLibUsage ; return TRUE ; }
    // We try to fetch the location of the WAB DLL from the registry
    TCHAR wabDLLPath [MAX_PATH] ;
    DWORD keyType = 0 ;
    ULONG byteCount = sizeof(wabDLLPath) ;
    HKEY keyHandle = NULL ;
    
    wabDLLPath [MAX_PATH - 1] = 0 ;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, WAB_DLL_PATH_KEY, 0, KEY_READ, &keyHandle) == ERROR_SUCCESS) {
        RegQueryValueEx(keyHandle, "", NULL, &keyType, (LPBYTE) wabDLLPath, &byteCount) ;
    }
    if (keyHandle) { RegCloseKey(keyHandle) ; }
    mLibrary = LoadLibrary( (lstrlen(wabDLLPath)) ? wabDLLPath : WAB_DLL_NAME );
    if (!mLibrary) { return FALSE ; }
    ++ mLibUsage ;
    mWABOpen = NS_REINTERPRET_CAST(LPWABOPEN, GetProcAddress(mLibrary, "WABOpen")) ;
    if (!mWABOpen) { return FALSE ; }
    HRESULT retCode = mWABOpen(&mRootBook, &mRootSession, NULL, 0) ;

    if (HR_FAILED(retCode)) {
        PRINTF(("Cannot initialize WAB %08x.\n", retCode)) ; return FALSE ;
    }
    return TRUE ;
}

void nsWabAddressBook::FreeWabLibrary(void)
{
    if (mLibrary) {
        if (-- mLibUsage == 0) {
            if (mRootBook) { mRootBook->Release() ; }
            if (mRootSession) { mRootSession->Release() ; }
            FreeLibrary(mLibrary) ;
            mLibrary = NULL ;
        }
    }
}

MOZ_DECL_CTOR_COUNTER(nsWabAddressBook)

nsWabAddressBook::nsWabAddressBook(void)
: nsAbWinHelper()
{
    BOOL result = Initialize() ;

    NS_ASSERTION(result == TRUE, "Couldn't initialize Wab Helper") ;
    MOZ_COUNT_CTOR(nsWabAddressBook) ;
}

nsWabAddressBook::~nsWabAddressBook(void)
{
    nsAutoLock guard(mMutex) ;
    FreeWabLibrary() ;
    MOZ_COUNT_DTOR(nsWabAddressBook) ;
}

BOOL nsWabAddressBook::Initialize(void)
{
    if (mAddressBook) { return TRUE ; }
    nsAutoLock guard(mMutex) ;

    if (!LoadWabLibrary()) {
        PRINTF(("Cannot load library.\n")) ;
        return FALSE ;
    }
    mAddressBook = mRootBook ;
    return TRUE ;
}

void nsWabAddressBook::AllocateBuffer(ULONG aByteCount, LPVOID *aBuffer)
{
    mRootSession->AllocateBuffer(aByteCount, aBuffer) ;
}

void nsWabAddressBook::FreeBuffer(LPVOID aBuffer)
{
    mRootSession->FreeBuffer(aBuffer) ;
}






