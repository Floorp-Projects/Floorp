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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
 *
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

#include "nsIObserver.h"

#define NS_CONTENTHTTPSTARTUP_CONTRACTID \
  "@mozilla.org/content/http-startup;1"

/* c2f6ef7e-afd5-4be4-a1f5-c824efa4231b */
#define NS_CONTENTHTTPSTARTUP_CID \
{ 0xc2f6ef7e, 0xafd5, 0x4be4, \
    {0xa1, 0xf5, 0xc8, 0x24, 0xef, 0xa4, 0x23, 0x1b} }

struct nsModuleComponentInfo;

class nsContentHTTPStartup : public nsIObserver
{
public:
    nsContentHTTPStartup() { NS_INIT_ISUPPORTS(); }
    virtual ~nsContentHTTPStartup() {}
  
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

public:
    static NS_IMETHODIMP
    RegisterHTTPStartup(nsIComponentManager*         aCompMgr,
                        nsIFile*                     aPath,
                        const char*                  aRegistryLocation,
                        const char*                  aComponentType,
                        const nsModuleComponentInfo* aInfo);

    static NS_IMETHODIMP
    UnregisterHTTPStartup(nsIComponentManager*         aCompMgr,
                          nsIFile*                     aPath,
                          const char*                  aRegistryLocation,
                          const nsModuleComponentInfo* aInfo);
  
private:
    nsresult setUserAgent();
  
};
