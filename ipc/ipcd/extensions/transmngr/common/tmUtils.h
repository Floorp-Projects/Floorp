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

#ifndef _tmUtils_H_
#define _tmUtils_H_

#include "nscore.h"
#include "nsError.h"
#include "nsID.h"
#include "prlog.h"
#include <stdio.h>

// UUID used to identify the Transaction Module in both daemon and client
// not part of the XPCOM hooks, but rather a means of identifying
// modules withing the IPC daemon.
#define TRANSACTION_MODULE_ID                         \
{ /* c3dfbcd5-f51d-420b-abf4-3bae445b96a9 */          \
    0xc3dfbcd5,                                       \
    0xf51d,                                           \
    0x420b,                                           \
    {0xab, 0xf4, 0x3b, 0xae, 0x44, 0x5b, 0x96, 0xa9}  \
}

//static const nsID kTransModuleID = TRANSACTION_MODULE_ID;

///////////////////////////////////////////////////////////////////////////////
// match NS_ERROR_FOO error code formats
//
// only create new errors for those errors that are specific to TM

#define NS_ERROR_MODULE_TM       27   /* XXX goes in nserror.h -- integrating with ns error codes */

#define TM_ERROR                          NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_TM, 1)
#define TM_ERROR_WRONG_QUEUE              NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_TM, 2)
#define TM_ERROR_NOT_POSTED               NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_TM, 3)
#define TM_ERROR_QUEUE_EXISTS             NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_TM, 4)
#define TM_SUCCESS_DELETE_QUEUE           NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_TM, 6)


// XXX clean up:
#define TM_INVALID_ID 0xFFFFFFFF
#define TM_INVALID 0xFFFFFFFF
#define TM_NO_ID 0xFFFFFFFE

// Transaction Actions
enum {
  TM_ATTACH = 0, 
  TM_ATTACH_REPLY,
  TM_POST,
  TM_POST_REPLY,
  TM_NOTIFY,
  TM_FLUSH,
  TM_FLUSH_REPLY,
  TM_DETACH,
  TM_DETACH_REPLY 
};

#endif

