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

#ifndef _tmIPCModule_H_
#define _tmIPCModule_H_

#include "ipcModuleUtil.h"
#include "tmUtils.h"

// forward declarations
class tmTransaction;
class tmTransactionManager;

/**
  * Basically an interface between the tmTransactionManager and the IPC
  *   daemon. Does little else than format the data from one party into
  *   a format understandable to the other.
  *
  * The reason for this class is to try and abstract the transportation 
  *   layer the transaction service uses. By using this class the Transaction
  *   Manager itself only needs to know that clients are identified by 
  *   PRUint32 IDs. 
  */
class tmIPCModule
{
public:

  ////////////////////////////////////////////////////////////////////////////
  // ipcModule API - called from IPC daemon

  /**
    * Clean up the TM
    */
  static void Shutdown();

  /**
    * Check the TM, create it if neccessary.
    */
  static void Init();

  /**
    * Receives a message from the IPC daemon, creates a transaction and sends
    *   it to the TM to deal with.
    */
  static void HandleMsg(ipcClientHandle client,
                        const nsID     &target,
                        const void     *data,
                        PRUint32        dataLen);

  ////////////////////////////////////////////////////////////////////////////
  // tmIPCModule API - called from tmTransactionManager

  /**
    * Sends the message to the IPC daemon to be deliverd to the client arg.
    */
  static void SendMsg(PRUint32 aDestClientIPCID, tmTransaction *aTransaction);

protected:

  /**
    * tm should be null coming into this method. This does NOT delete tm
    *   first. 
    *
    * @returns NS_OK if everything succeeds
    * @returns -1 if initialization fails
    */
  static PRInt32 InitInternal();

  static tmTransactionManager *tm;

};

#endif


