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
 * Srilatha Moturi <srilatha@netscape.com>
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

#ifndef nsmapiregistryutils_h____
#define nsmapiregistryutils_h____

#include <windows.h>
#include <string.h>
#include <winreg.h>

#include "Registry.h"
#include "nsString.h"

// This function returns the (fully-qualified) name of this executable.
static nsCString thisApplication();

// returns the brandName of the current application
static nsCString brandName();

//Sets the registry key for basekey/keyname valuename.
nsresult SetRegistryKey(HKEY baseKey, const char * keyName, 
                        const char * valueName, char * value);

// Gets the current regiistry setting for the key.
nsCString GetRegistryKey(HKEY baseKey, const char * keyName, 
                         const char * valueName);
// Returns PR_TRUE if this user only has restricted access
// to the registry keys we need to modify.
PRBool verifyRestrictedAccess();

// Returns true if mozilla is the default mail client
// by checking the registry.
PRBool IsDefaultMailClient();

// Save the current setting for the default mail client.
nsresult saveDefaultMailClient();

// Saves the current user setting for the default mail client.
nsresult saveUserDefaultMailClient();

// Sets mozilla as the default mail client	
nsresult setDefaultMailClient();

// unsets mozilla and resets the default mail client setting to previous one
nsresult unsetDefaultMailClient();

// returns true if we need to show the mail integration dialog.
PRBool getShowDialog();

#endif
