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
 * The Original Code is Mozilla Transaction Manager.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Gaunt <jgaunt@netscape.com>
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

#include "tmIPCModule.h"
#include "tmTransaction.h"
#include "tmTransactionManager.h"

///////////////////////////////////////////////////////////////////////////////
// IPC Daemon hookup stuff

// functionpointer array giving access to this module from the IPC daemon
static ipcModuleMethods gTransMethods =
{
    IPC_MODULE_METHODS_VERSION,
    tmIPCModule::Init,
    tmIPCModule::Shutdown,
    tmIPCModule::HandleMsg
};

static ipcModuleEntry gTransModuleEntry[] =
{
    { TRANSACTION_MODULE_ID, &gTransMethods }
};

IPC_IMPL_GETMODULES(TransactionModule, gTransModuleEntry)

static const nsID kTransModuleID = TRANSACTION_MODULE_ID;

///////////////////////////////////////////////////////////////////////////////
// Define global variable

tmTransactionManager *tmIPCModule::tm;

///////////////////////////////////////////////////////////////////////////////
// IPC Module API

void
tmIPCModule::Init() {
  if (!tm)
    InitInternal();
}

void
tmIPCModule::Shutdown() {
  if (tm) {
    delete tm;
    tm = nsnull;
  }
}

// straight pass-through, don't check args, let the TM do it.
void
tmIPCModule::HandleMsg(ipcClientHandle client, const nsID &target, 
                       const void *data, PRUint32 dataLen) {

  // make sure the trans mngr is there
  if (!tm && (InitInternal() < 0))
    return;

  // create the transaction
  tmTransaction *trans = new tmTransaction();

  // initialize it
  if (trans) {
    if(NS_SUCCEEDED(trans->Init(IPC_GetClientID(client),  // who owns it
                                TM_INVALID_ID,            // in data
                                TM_INVALID,               // in data
                                TM_INVALID,               // in data
                                (PRUint8 *)data,          // raw message
                                dataLen))) {              // length of message
      // pass it on to the trans mngr
      tm->HandleTransaction(trans);
    }
    else
      delete trans;
  }
}

///////////////////////////////////////////////////////////////////////////////
// tmIPCModule API

// straight pass-through, don't check args, let the TS & TM do it.
void
tmIPCModule::SendMsg(PRUint32 aDestClientIPCID, tmTransaction *aTransaction) {

  IPC_SendMsg(aDestClientIPCID,
              kTransModuleID,
              (void *)aTransaction->GetRawMessage(),
              aTransaction->GetRawMessageLength());
}

///////////////////////////////////////////////////////////////////////////////
// Protected Methods

PRInt32
tmIPCModule::InitInternal() {

  tm = new tmTransactionManager();
  if (tm)
    return tm->Init();
  return -1;
}

