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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Srilatha Moturi <srilatha@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsmapiregistryutils_h____
#define nsmapiregistryutils_h____

#include <windows.h>
#include <string.h>
#include <winreg.h>

#include "Registry.h"
#include "nsString.h"
#include "nsIStringBundle.h"

class nsMapiRegistryUtils
{
private :
    nsCAutoString m_thisApp ;
    nsAutoString m_brand ;
    nsAutoString m_vendor ;

    nsCOMPtr<nsIStringBundle> m_mapiStringBundle ;

    // sets result to the value of varName (as defined in brand.properties)
    void getVarValue(const PRUnichar * varName, nsAutoString & result);
public :
    nsMapiRegistryUtils() ;

    // returns TRUE if the Mapi32.dll is smart dll.
    PRBool isSmartDll();
    // returns TRUE if the Mapi32.dll is a Mozilla dll.
    PRBool isMozDll();

    // Returns the (fully-qualified) name of this executable.
    const char * thisApplication() ; 
    // This returns the brand name for this application
    const PRUnichar * brandName() ;
    // This returns the vendor name of this application
    const nsString& vendorName();
    // verifyRestrictedAccess - Returns PR_TRUE if this user only has restricted access
    // to the registry keys we need to modify.
    PRBool verifyRestrictedAccess() ;

    // set the Windows registry key
nsresult SetRegistryKey(HKEY baseKey, const char * keyName, 
                        const char * valueName, char * value);
    // delete a registry key
nsresult DeleteRegistryValue(HKEY baseKey, const char * keyName, 
                        const char * valueName);
    // get a Windows registry key
    void GetRegistryKey(HKEY baseKey, const char * keyName, 
                         const char * valueName, nsCAutoString & value) ;

    // Returns TRUE if the current application is default mail client.
PRBool IsDefaultMailClient();
    // Sets Mozilla as default Mail Client
    nsresult setDefaultMailClient() ;
    // Removes Mozilla as the default Mail client and restores the previous setting
    nsresult unsetDefaultMailClient() ;

    // Saves the current setting of the default Mail Client in 
    // HKEY_LOCAL_MACHINE\\Software\\Mozilla\\Desktop
nsresult saveDefaultMailClient();
    // Saves the current user setting of the default Mail Client in 
    // HKEY_LOCAL_MACHINE\\Software\\Mozilla\\Desktop
nsresult saveUserDefaultMailClient();

    nsresult setMailtoProtocolHandler();
    nsresult setNewsProtocolHandler();

nsresult CopyMozMapiToWinSysDir();
nsresult RestoreBackedUpMapiDll();

    // Returns FALSE if showMapiDialog is set to 0.
    PRBool getShowDialog() ;

    // create a string bundle for MAPI messages
    nsresult MakeMapiStringBundle(nsIStringBundle ** aMapiStringBundle) ;
    // display an error dialog for MAPI messages
    nsresult ShowMapiErrorDialog() ;
} ;

#endif
