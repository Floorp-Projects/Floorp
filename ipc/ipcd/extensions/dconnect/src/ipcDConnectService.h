/* vim:set ts=2 sw=2 et cindent: */
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
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

#include "ipcIDConnectService.h"
#include "ipcdclient.h"

#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"

#include "nsCOMPtr.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "xptcall.h"
#include "xptinfo.h"

class DConnectInstance;
typedef nsClassHashtable<nsVoidPtrHashKey, DConnectInstance> DConnectInstanceSet;

class ipcDConnectService : public ipcIDConnectService
                         , public ipcIMessageObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IPCIDCONNECTSERVICE
  NS_DECL_IPCIMESSAGEOBSERVER

  NS_HIDDEN_(nsresult) Init();

  NS_HIDDEN_(nsresult) GetInterfaceInfo(const nsID &iid, nsIInterfaceInfo **);
  NS_HIDDEN_(nsresult) GetIIDForMethodParam(nsIInterfaceInfo *iinfo,
                                            const nsXPTMethodInfo *methodInfo,
                                            const nsXPTParamInfo &paramInfo,
                                            const nsXPTType &type,
                                            PRUint16 methodIndex,
                                            PRUint8 paramIndex,
                                            nsXPTCMiniVariant *dispatchParams,
                                            PRBool isFullVariantArray,
                                            nsID &result);

  NS_HIDDEN_(nsresult) StoreInstance(DConnectInstance *);
  NS_HIDDEN_(void)     DeleteInstance(DConnectInstance *);

private:

  NS_HIDDEN ~ipcDConnectService();

  NS_HIDDEN_(void) OnSetup(PRUint32 peer, const struct DConnectSetup *, PRUint32 opLen);
  NS_HIDDEN_(void) OnRelease(PRUint32 peer, const struct DConnectRelease *);
  NS_HIDDEN_(void) OnInvoke(PRUint32 peer, const struct DConnectInvoke *, PRUint32 opLen);

private:
  nsCOMPtr<nsIInterfaceInfoManager> mIIM;

  // table of local object instances allocated on behalf of a peer
  DConnectInstanceSet mInstances;
};

#define IPC_DCONNECTSERVICE_CLASSNAME \
  "ipcDConnectService"
#define IPC_DCONNECTSERVICE_CONTRACTID \
  "@mozilla.org/ipc/dconnect-service;1"
#define IPC_DCONNECTSERVICE_CID                    \
{ /* 63a5d9dc-4828-425a-bd50-bd10a4b26f2c */       \
  0x63a5d9dc,                                      \
  0x4828,                                          \
  0x425a,                                          \
  {0xbd, 0x50, 0xbd, 0x10, 0xa4, 0xb2, 0x6f, 0x2c} \
}
