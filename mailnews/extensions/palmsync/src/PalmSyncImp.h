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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef PALMSYNC_IMP_H
#define PALMSYNC_IMP_H

#include <windows.h>
#include "IPalmSync.h"
#include "nspr.h"
#include "nsString.h"

const CLSID CLSID_CPalmSyncImp = { 0xb20b4521, 0xccf8, 0x11d6, { 0xb8, 0xa5, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } };

// this class implements the MS COM interface IPalmSync that provides the methods
// called by the Palm Conduit to perform the AB Sync operations.
class CPalmSyncImp : public IPalmSync
{

public :

    // IUnknown

    STDMETHODIMP            QueryInterface(const IID& aIid, void** aPpv);
    STDMETHODIMP_(ULONG)    AddRef();
    STDMETHODIMP_(ULONG)    Release();

    // Interface IPalmSync

    STDMETHODIMP IsValid();

    CPalmSyncImp();
    ~CPalmSyncImp();

    // Get the list of Address Books for the currently logged in user profile
    STDMETHODIMP nsGetABList(BOOL aIsUnicode, short * aABListCount,
                            lpnsMozABDesc * aABList, long ** aABCatIndexList, BOOL ** aDirFlags);

    // Synchronize the Address Book represented by the aCategoryIndex and/or corresponding aABName in Mozilla
    STDMETHODIMP nsSynchronizeAB(BOOL aIsUnicode, long aCategoryIndex, long aCategoryId, LPTSTR aABName,
                        int aModRemoteRecCount, lpnsABCOMCardStruct aModRemoteRecList,
                        int * aModMozRecCount, lpnsABCOMCardStruct * aModMozRecList);

    STDMETHODIMP nsAddAllABRecords(BOOL aIsUnicode, BOOL replaceExisting, long aCategoryIndex, LPTSTR aABName,
                            int aRemoteRecCount, lpnsABCOMCardStruct aRemoteRecList);


    STDMETHODIMP nsGetAllABCards(BOOL aIsUnicode, long aCategoryIndex, LPTSTR aABName,
                            int * aMozRecCount, lpnsABCOMCardStruct * aMozRecList);

    STDMETHODIMP nsAckSyncDone(BOOL aIsSuccess, long aCatIndex, int aNewRecCount, unsigned long * aNewPalmRecIDList);

    STDMETHODIMP nsUpdateABSyncInfo(BOOL aIsUnicode, long aCategoryIndex, LPTSTR aABName);

    STDMETHODIMP nsDeleteAB(BOOL aIsUnicode, long aCategoryIndex, LPTSTR aABName, LPTSTR aABUrl);
 
    STDMETHODIMP nsRenameAB(BOOL aIsUnicode, long aCategoryIndex, LPTSTR aABName, LPTSTR aABUrl);

    STDMETHODIMP nsUseABHomeAddressForPalmAddress(BOOL *aUseHomeAddress);

    STDMETHODIMP nsPreferABHomePhoneForPalmPhone(BOOL *aPreferHomePhone);

    STDMETHODIMP nsGetABDeleted(LPTSTR aABName, BOOL *abDeleted);

    static PRBool GetBoolPref(const char *prefName, PRBool defaultVal);
    static PRInt32 GetIntPref(const char *prefName, PRInt32 defaultVal);
    static PRBool nsUseABHomeAddressForPalmAddress();
    static PRBool nsPreferABHomePhoneForPalmPhone();
private :
    PRInt32 m_cRef;
    void * m_PalmHotSync;
    void CopyUnicodeString(LPTSTR *destStr, const nsAFlatString& srcStr);
    void CopyCString(LPTSTR *destStr, const nsAFlatCString& srcStr);
};

#endif // MSG_MAPI_IMP_H
