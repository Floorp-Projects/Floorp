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
#ifndef nsWabAddressBook_h___
#define nsWabAddressBook_h___

#include "nsAbWinHelper.h"
#include <wab.h>

class nsWabAddressBook : public nsAbWinHelper
{
public :
    nsWabAddressBook(void) ;
    virtual ~nsWabAddressBook(void) ;

protected :
    // Session and address book that will be shared by all instances
    // (see nsMapiAddressBook.h for details)
    static LPWABOBJECT mRootSession ;
    static LPADRBOOK mRootBook ;
    // Class members to handle library loading/entry points
    static PRInt32 mLibUsage ;
    static HMODULE mLibrary ;
    static LPWABOPEN mWABOpen ;

    // Load the WAB environment
    BOOL Initialize(void) ;
    // Allocation of a buffer for transmission to interfaces
    virtual void AllocateBuffer(ULONG aByteCount, LPVOID *aBuffer) ;
    // Destruction of a buffer provided by the interfaces
    virtual void FreeBuffer(LPVOID aBuffer) ;
    // Manage the library
    static BOOL LoadWabLibrary(void) ;
    static void FreeWabLibrary(void) ;

private :
} ;

// Additional definitions for WAB stuff. These properties are
// only defined with regards to the default character sizes, 
// and not in two _A and _W versions...
#define PR_BUSINESS_ADDRESS_CITY_A                    PR_LOCALITY_A
#define PR_BUSINESS_ADDRESS_COUNTRY_A                 PR_COUNTRY_A
#define PR_BUSINESS_ADDRESS_POSTAL_CODE_A             PR_POSTAL_CODE_A
#define PR_BUSINESS_ADDRESS_STATE_OR_PROVINCE_A       PR_STATE_OR_PROVINCE_A
#define PR_BUSINESS_ADDRESS_STREET_A                  PR_STREET_ADDRESS_A

#define PR_BUSINESS_ADDRESS_CITY_W                    PR_LOCALITY_W
#define PR_BUSINESS_ADDRESS_COUNTRY_W                 PR_COUNTRY_W
#define PR_BUSINESS_ADDRESS_POSTAL_CODE_W             PR_POSTAL_CODE_W
#define PR_BUSINESS_ADDRESS_STATE_OR_PROVINCE_W       PR_STATE_OR_PROVINCE_W
#define PR_BUSINESS_ADDRESS_STREET_W                  PR_STREET_ADDRESS_W

#endif // nsWABAddressBook_h___

