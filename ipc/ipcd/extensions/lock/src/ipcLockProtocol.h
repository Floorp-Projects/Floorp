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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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

#ifndef ipcLockProtocol_h__
#define ipcLockProtocol_h__

#include "prtypes.h"

//
// ipc lock message format:
//
//   +----------------------------------+
//   | opcode : 4 bits                  |
//   +----------------------------------+
//   | flags  : 4 bits                  |
//   +----------------------------------+
//   | key    : null terminated string  |
//   +----------------------------------+
//

// lock opcodes
#define IPC_LOCK_OP_ACQUIRE          1
#define IPC_LOCK_OP_RELEASE          2
#define IPC_LOCK_OP_STATUS_ACQUIRED  3
#define IPC_LOCK_OP_STATUS_FAILED    4
#define IPC_LOCK_OP_STATUS_BUSY      5

// lock flags
#define IPC_LOCK_FL_NONBLOCKING 1

// data structure for representing lock request message
struct ipcLockMsg
{
    PRUint8      opcode;
    PRUint8      flags;
    const char * key;
};

//
// flatten a lock message
//
// returns a malloc'd buffer containing the flattened message.  on return,
// bufLen contains the length of the flattened message.
//
PRUint8 *IPC_FlattenLockMsg(const ipcLockMsg *msg, PRUint32 *bufLen);

//
// unflatten a lock message.  upon return, msg->key points into buf, so
// buf must not be deallocated until after msg is no longer needed.
//
void IPC_UnflattenLockMsg(const PRUint8 *buf, PRUint32 bufLen, ipcLockMsg *msg);

//
// TargetID for message passing
//
#define IPC_LOCK_TARGETID \
{ /* 703ada8a-2d38-4d5d-9d39-03d1ccceb567 */         \
    0x703ada8a,                                      \
    0x2d38,                                          \
    0x4d5d,                                          \
    {0x9d, 0x39, 0x03, 0xd1, 0xcc, 0xce, 0xb5, 0x67} \
}

#endif // !ipcLockProtocol_h__
