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
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
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

#ifndef ipcConnection_h__
#define ipcConnection_h__

#include "nscore.h"

class ipcMessage;

#define IPC_METHOD_PRIVATE_(type)   NS_HIDDEN_(type)
#define IPC_METHOD_PRIVATE          IPC_METHOD_PRIVATE_(nsresult)

/* ------------------------------------------------------------------------- */
/* Platform specific IPC connection API.
 */

typedef void (* ipcCallbackFunc)(void *);

/**
 * IPC_Connect
 *
 * This function causes a connection to the IPC daemon to be established.
 * If a connection already exists, then this function will be ignored.
 *
 * @param daemonPath
 *        Specifies the path to the IPC daemon executable.
 *
 * NOTE: This function must be called on the main thread.
 */
IPC_METHOD_PRIVATE IPC_Connect(const char *daemonPath);

/**
 * IPC_Disconnect
 *
 * This function causes a connection to the IPC daemon to be closed.  Any
 * unsent messages (IPC_SendMsg puts messages on a queue) will be sent to the
 * IPC daemon before the connection is closed.
 *
 * NOTE: This function must be called on the main thread.
 */
IPC_METHOD_PRIVATE IPC_Disconnect();

/**
 * IPC_SendMsg
 *
 * This function sends a message to the IPC daemon.  Typically, the message
 * is put on a queue, to be delivered asynchronously to the IPC daemon.  The
 * ipcMessage object will be deleted when IPC_SendMsg is done with it.  The
 * caller must not touch |msg| after passing it to IPC_SendMsg.
 *
 * IPC_SendMsg cannot be called before IPC_Connect or after IPC_Disconnect.
 *
 * NOTE: This function may be called on any thread.
 */
IPC_METHOD_PRIVATE IPC_SendMsg(ipcMessage *msg);

/**
 * IPC_DoCallback
 *
 * This function executes a callback function on the same background thread
 * that calls IPC_OnConnectionEnd and IPC_OnMessageAvailable.
 *
 * If this function succeeds, then the caller is guaranteed that |func| will
 * be called.  This guarantee is important because it allows the caller to
 * free any memory associated with |arg| once |func| has been called.
 *
 * NOTE: This function may be called on any thread.
 */
IPC_METHOD_PRIVATE IPC_DoCallback(ipcCallbackFunc func, void *arg);

/* ------------------------------------------------------------------------- */
/* Cross-platform IPC connection methods.
 */

/**
 * IPC_SpawnDaemon
 *
 * This function launches the IPC daemon process.  It is called by the platform
 * specific IPC_Connect implementation.  It should not return until the daemon
 * process is ready to receive a client connection or an error occurs.
 *
 * @param daemonPath
 *        Specifies the path to the IPC daemon executable.
 */
IPC_METHOD_PRIVATE IPC_SpawnDaemon(const char *daemonPath);

/* ------------------------------------------------------------------------- */
/* IPC connection callbacks (not implemented by the connection code).
 *
 * NOTE: These functions execute on a background thread!!
 */

/**
 * IPC_OnConnectionEnd
 *
 * This function is called whenever the connection to the IPC daemon has been
 * terminated.  If terminated due to an abnormal error, then the error will be
 * described by the |error| parameter.  If |error| is NS_OK, then it means the
 * connection was closed in response to a call to IPC_Disconnect.
 */
IPC_METHOD_PRIVATE_(void) IPC_OnConnectionEnd(nsresult error);

/**
 * IPC_OnMessageAvailable
 *
 * This function is called whenever an incoming message is read from the IPC
 * daemon.  The ipcMessage object, |msg|, must be deleted by the implementation
 * of IPC_OnMessageAvailable when the object is no longer needed.
 */
IPC_METHOD_PRIVATE_(void) IPC_OnMessageAvailable(ipcMessage *msg);

#endif // ipcConnection_h__
