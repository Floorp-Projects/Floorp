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

#include <stdlib.h>
#include <string.h>
#include "prlog.h"
#include "ipcLockProtocol.h"

//-----------------------------------------------------------------------------

static inline PRUint8 get_opcode(const PRUint8 *buf)
{
    return (buf[0] & 0x0f);
}

static inline PRUint8 get_flags(const PRUint8 *buf)
{
    return (buf[0] & 0xf0) >> 4;
}

static inline const char *get_key(const PRUint8 *buf)
{
    return ((const char *) buf) + 1;
}

//-----------------------------------------------------------------------------

PRUint8 *
IPC_FlattenLockMsg(const ipcLockMsg *msg, PRUint32 *bufLen)
{
    PRUint32 len = 1                 // header byte
                 + strlen(msg->key)  // key
                 + 1;                // null terminator

    PRUint8 *buf = (PRUint8 *) ::operator new(len);
    if (!buf)
        return NULL;

    buf[0] = (msg->opcode | (msg->flags << 4));

    memcpy(&buf[1], msg->key, len - 1);
    *bufLen = len;
    return buf;
}

void
IPC_UnflattenLockMsg(const PRUint8 *buf, PRUint32 bufLen, ipcLockMsg *msg)
{
    PR_ASSERT(bufLen > 2); // malformed buffer otherwise
    msg->opcode = get_opcode(buf);
    msg->flags = get_flags(buf);
    msg->key = get_key(buf);
}
