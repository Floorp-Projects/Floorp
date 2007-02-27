/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Thunderbird Windows Integration.
 *
 * The Initial Developer of the Original Code is
 *   Scott MacGregor <mscott@mozilla.org>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsMailWinIntegration_h_
#define nsMailWinIntegration_h_

#include "nsIShellService.h"
#include "nsIObserver.h"
#include "nsIGenericFactory.h"
#include "nsString.h"

#include <ole2.h>
#include <windows.h>

#define NS_MAILWININTEGRATION_CID \
{0x2ebbe84, 0xc179, 0x4598, {0xaf, 0x18, 0x1b, 0xf2, 0xc4, 0xbc, 0x1d, 0xf9}}

typedef struct {
  char* keyName;
  char* valueName;
  char* valueData;

  PRInt32 flags;
} SETTING;

class nsWindowsShellService : public nsIShellService
{
public:
  nsWindowsShellService();
  virtual ~nsWindowsShellService() {};
  NS_HIDDEN_(nsresult) Init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISHELLSERVICE

protected:
  void SetRegKey(const char* aKeyName, const char* aValueName, 
                 const char* aValue, PRBool aHKLMOnly);
  DWORD DeleteRegKey(HKEY baseKey, const char *keyName);
  DWORD DeleteRegKeyDefaultValue(HKEY baseKey, const char *keyName);

  PRBool TestForDefault(SETTING aSettings[], PRInt32 aSize);
  void setKeysForSettings(SETTING aSettings[], PRInt32 aSize, 
                          const char * aAppname);
  nsresult setDefaultMail();
  nsresult setDefaultNews();

  PRBool IsDefaultClientVista(PRBool aStartupCheck, PRUint16 aApps, PRBool* aIsDefaultClient);
  PRBool SetDefaultClientVista(PRUint16 aApps);

private:
  PRBool mCheckedThisSession;
  nsCString mAppLongPath;
  nsCString mAppShortPath;
  nsCString mMapiDLLPath;
  nsCString mUninstallPath;
  nsXPIDLString mBrandFullName;
  nsXPIDLString mBrandShortName;
 };

#endif
