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

#ifndef __SYSTEM_PREF_SERVICE_H__
#define __SYSTEM_PREF_SERVICE_H__

#include "prlink.h"
#include "nsVoidArray.h"
#include "nsWeakPtr.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranch2.h"

class GConfProxy;

////////////////////////////////////////////////////////////////////////////
// nsSystemPrefService provide a interface for read system prefs. It is
// platform related. This directory (system-pref/gconf) impls it for gconf
// on the gconf platform.
////////////////////////////////////////////////////////////////////////////

class nsSystemPrefService : public nsIPrefBranch2
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPREFBRANCH
    NS_DECL_NSIPREFBRANCH2

    nsSystemPrefService();
    virtual ~nsSystemPrefService();
    nsresult Init();

    void OnPrefChange(PRUint32 aPrefAtom, void *aData);

private:
    PRBool mInitialized;
    GConfProxy *mGConf;

    //listeners
    nsAutoVoidArray *mObservers;
};

#define NS_SYSTEMPREF_SERVICE_CID                  \
  { /* {94f1de09-d0e5-4ca8-94c2-98b049316b7f} */       \
    0x94f1de09,                                        \
    0xd0e5,                                            \
    0x4ca8,                                            \
    { 0x94, 0xc2, 0x98, 0xb0, 0x49, 0x31, 0x6b, 0x7f } \
  }

#define NS_SYSTEMPREF_SERVICE_CONTRACTID "@mozilla.org/system-preference-service;1"
#define NS_SYSTEMPREF_SERVICE_CLASSNAME "System Preferences Service"

#define NS_SYSTEMPREF_PREFCHANGE_TOPIC_ID "nsSystemPrefService:pref-changed"

#endif  /* __SYSTEM_PREF_SERVICE_H__ */
