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

#ifndef ipcModule_h__
#define ipcModule_h__

#include "nsID.h"

//
// a client handle is used to efficiently reference a client instance object
// used by the daemon to represent a connection with a particular client app.
//
// modules should treat it as an opaque type.
//
typedef class ipcClient *ipcClientHandle;

//-----------------------------------------------------------------------------
// interface implemented by the module:
//-----------------------------------------------------------------------------

//
// the version of ipcModuleMethods data structure.
//
#define IPC_MODULE_METHODS_VERSION (1<<16) // 1.0

//
// each module defines the following structure:
//
struct ipcModuleMethods
{
    //
    // this field holds the version of the data structure, which is always the
    // value of IPC_MODULE_METHODS_VERSION against which the module was built.
    //
    PRUint32 version;

    //
    // called after this module is registered.
    //
    void (* init) (void);

    //
    // called when this module will no longer be accessed.
    //
    void (* shutdown) (void);

    //
    // called when a new message arrives for this module.
    //
    // params:
    //   client  - an opaque "handle" to an object representing the client that
    //             sent the message.  modules should not store the value of this
    //             beyond the duration fo this function call.  (e.g., the handle
    //             may be invalid after this function call returns.)  modules
    //             wishing to hold onto a reference to a "client" should store
    //             the client's ID (see IPC_GetClientID).
    //   target  - message target
    //   data    - message data
    //   dataLen - message data length
    //
    void (* handleMsg) (ipcClientHandle client,
                        const nsID     &target,
                        const void     *data,
                        PRUint32        dataLen);

    //
    // called when a new client connects to the IPC daemon.
    //
    void (* clientUp) (ipcClientHandle client);

    //
    // called when a client disconnects from the IPC daemon.
    //
    void (* clientDown) (ipcClientHandle client);
};

//-----------------------------------------------------------------------------
// interface implemented by the daemon:
//-----------------------------------------------------------------------------

//
// the version of ipcDaemonMethods data structure.
//
#define IPC_DAEMON_METHODS_VERSION (1<<16) // 1.0

//
// enumeration functions may return FALSE to stop enumeration.
//
typedef PRBool (* ipcClientEnumFunc)       (void *closure, ipcClientHandle client, PRUint32 clientID);
typedef PRBool (* ipcClientNameEnumFunc)   (void *closure, ipcClientHandle client, const char *name);
typedef PRBool (* ipcClientTargetEnumFunc) (void *closure, ipcClientHandle client, const nsID &target);

//
// the daemon provides the following structure:
//
struct ipcDaemonMethods
{
    PRUint32 version;

    //
    // called to send a message to another module.
    //
    // params:
    //   client  - identifies the client from which this message originated.
    //   target  - message target
    //   data    - message data
    //   dataLen - message data length
    //
    // returns:
    //   PR_SUCCESS if message was dispatched.
    //   PR_FAILURE if message could not be dispatched (possibly because
    //              no module is registered for the given message target).
    //
    PRStatus (* dispatchMsg) (ipcClientHandle client, 
                              const nsID     &target,
                              const void     *data,
                              PRUint32        dataLen);

    //
    // called to send a message to a particular client or to broadcast a
    // message to all clients.
    //
    // params:
    //   client  - if null, then broadcast message to all clients.  otherwise,
    //             send message to the client specified.
    //   target  - message target
    //   data    - message data
    //   dataLen - message data length
    //
    // returns:
    //   PR_SUCCESS if message was sent (or queued up to be sent later).
    //   PR_FAILURE if message could not be sent (possibly because the client
    //              does not have a registered observer for the msg's target).
    //
    PRStatus (* sendMsg) (ipcClientHandle client,
                          const nsID     &target,
                          const void     *data,
                          PRUint32        dataLen);

    //
    // called to lookup a client handle given its client ID.  each client has
    // a unique ID.
    //
    ipcClientHandle (* getClientByID) (PRUint32 clientID);

    //
    // called to lookup a client by name or alias.  names are not necessary
    // unique to individual clients.  this function returns the client first
    // registered under the given name.
    //
    ipcClientHandle (* getClientByName) (const char *name);

    //
    // called to enumerate all clients.
    //
    void (* enumClients) (ipcClientEnumFunc func, void *closure);

    //
    // returns the client ID of the specified client.
    //
    PRUint32 (* getClientID) (ipcClientHandle client);

    //
    // functions for inspecting the names and targets defined for a particular
    // client instance.
    //
    PRBool (* clientHasName)     (ipcClientHandle client, const char *name);
    PRBool (* clientHasTarget)   (ipcClientHandle client, const nsID &target);
    void   (* enumClientNames)   (ipcClientHandle client, ipcClientNameEnumFunc func, void *closure);
    void   (* enumClientTargets) (ipcClientHandle client, ipcClientTargetEnumFunc func, void *closure);
};

//-----------------------------------------------------------------------------
// interface exported by a DSO implementing one or more modules:
//-----------------------------------------------------------------------------

struct ipcModuleEntry
{
    //
    // identifies the message target of this module.
    //
    nsID target;

    //
    // module methods
    //
    ipcModuleMethods *methods;
};

//-----------------------------------------------------------------------------

#define IPC_EXPORT extern "C" NS_EXPORT

//
// IPC_EXPORT int IPC_GetModules(const ipcDaemonMethods *, const ipcModuleEntry **);
//
// params:
//   methods - the daemon's methods
//   entries - the module entries defined by the DSO
//
// returns:
//   length of the |entries| array.
//
typedef int (* ipcGetModulesFunc) (const ipcDaemonMethods *methods, const ipcModuleEntry **entries);

#endif // !ipcModule_h__
