/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2003 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Bolian Yin (bolian.yin@sun.com)
 *
 * Contributor(s):
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

#ifndef __SYSTEM_PREF_H__
#define __SYSTEM_PREF_H__

#include "nsCOMPtr.h"
#include "nsXPCOM.h"
#include "nsCRT.h"
#include "nsIAppStartupNotifier.h"
#include "nsICategoryManager.h"
#include "nsIServiceManager.h"
#include "nsWeakReference.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch2.h"

#include <nsIObserver.h>

union MozPrefValue;
struct SysPrefItem;

//////////////////////////////////////////////////////////////////////////
//
// nsSystemPref, as an extension of mozilla pref service, reads some mozilla
// prefs from host system when the feature is enabled ("config.system-pref").
//
// nsSystemPref listens on NS_PREFSERVICE_READ_TOPIC_ID. When notified,
// nsSystemPref will start the nsSystemPrefService (platform specific) to
// read all the interested prefs (listed in sSysPrefList table) from system
// and lock these prefs from user's modification. 
//
// This feature will make mozilla integrated better into host platforms. If
// users want to change the prefs read from system, the system provided pref
// editor (i.e. gconf-editor in gnome) should be used.
//////////////////////////////////////////////////////////////////////////

class nsSystemPref : public nsIObserver,
                     public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    nsSystemPref();
    virtual ~nsSystemPref();
    nsresult Init(void);

private:
    // funcs used to load system prefs and save mozilla default prefs
    nsresult UseSystemPrefs();
    nsresult ReadSystemPref(const char *aPrefName);
    nsresult SaveMozDefaultPref(const char *aPrefName,
                                MozPrefValue *aPrefVal,
                                bool *aLocked);

    // funcs used to load mozilla default prefs
    nsresult UseMozillaPrefs();
    nsresult RestoreMozDefaultPref(const char *aPrefName,
                                   MozPrefValue *aPrefVal,
                                   bool aLocked);

    nsCOMPtr<nsIPrefBranch2>  mSysPrefService;
    bool mEnabled;  // system pref is enabled or not
    SysPrefItem *mSysPrefs;
};

#define NS_SYSTEMPREF_CID                  \
  { /* {549abb24-7c9d-4aba-915e-7ce0b716b32f} */       \
    0x549abb24,                                        \
    0x7c9d,                                            \
    0x4aba,                                            \
    { 0x91, 0x5e, 0x7c, 0xe0, 0xb7, 0x16, 0xb3, 0x2f } \
  }

#define NS_SYSTEMPREF_CONTRACTID "@mozilla.org/system-preferences;1"
#define NS_SYSTEMPREF_CLASSNAME "System Preferences"

#endif  /* __SYSTEM_PREF_H__ */
