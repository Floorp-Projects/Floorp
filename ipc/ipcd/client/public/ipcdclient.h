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

#ifndef ipcdclient_h__
#define ipcdclient_h__

/*****************************************************************************
 * This file provides a client-side API to the IPC daemon.
 *
 * This API can be used to communicate with other clients of the IPC daemon
 * as well as modules running inside the IPC daemon.
 *
 * This API is meant to be used only on the application's main thread.  It is
 * assumed that callbacks can be dispatched via the main thread's event queue.
 */

#include "nscore.h"
#include "nsID.h"
#include "nsError.h"
#include "ipcIMessageObserver.h"
#include "ipcIClientObserver.h"

/* This API is only provided for the extensions compiled into the IPCDC
 * library, hence this API is hidden in the final DSO. */
#define IPC_METHOD NS_HIDDEN_(nsresult)

/* This value can be used to represent the client id of any client connected
 * to the IPC daemon. */
#define IPC_SENDER_ANY PR_UINT32_MAX

/* This error code can only be returned by OnMessageAvailable, when called by
 * IPC_WaitMessage.  See IPC_WaitMessage for a description of how this error
 * code may be used. */
#define IPC_WAIT_NEXT_MESSAGE \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GENERAL, 10)

/* This error code is returned by IPC_WaitMessage under certain conditions. */
#define IPC_ERROR_WOULD_BLOCK NS_BASE_STREAM_WOULD_BLOCK

/*****************************************************************************
 * Initialization and Shutdown
 */

// XXX limit these to the main thread, and call them from our module's ctor/dtor?

/**
 * Connects this process to the IPC daemon and initializes it for use as a
 * client of the IPC daemon.  This function must be called once before any
 * other methods defined in this file can be used.
 *
 * @returns NS_ERROR_ALREADY_INITIALIZED if IPC_Shutdown was not called since
 * the last time IPC_Init was called.
 */
IPC_METHOD IPC_Init();

/**
 * Disconnects this process from the IPC daemon.  After this function is
 * called, no other methods in this file except for IPC_Init may be called.
 *
 * @returns NS_ERROR_NOT_INITIALIZED if IPC_Init has not been called or if
 * IPC_Init did not return a success code.
 */
IPC_METHOD IPC_Shutdown();


/*****************************************************************************
 * The core messaging API
 */

/**
 * Call this method to define a message target.  A message target is defined
 * by a UUID and a message observer.  This observer is notified asynchronously
 * whenever a message is sent to this target in this process.
 *
 * This function has three main effects:
 *  o  If the message target is already defined, then this function simply
 *     resets its message observer.
 *  o  If the message target is not already defined, then the message target
 *     is defined and the IPC daemon is notified of the existance of this
 *     message target.
 *  o  If null is passed for the message observer, then the message target is
 *     removed, and the daemon is notified of the removal of this message target.
 *
 * If aOnCurrentThread is true, then notifications to the observer will occur
 * on the current thread.  This means that there must be a nsIEventTarget
 * associated with the calling thread.  If aOnCurrentThread is false, then
 * notifications to the observer will occur on a background thread.  In which
 * case, the observer must be threadsafe.
 */
IPC_METHOD IPC_DefineTarget(
  const nsID          &aTarget,
  ipcIMessageObserver *aObserver,
  PRBool               aOnCurrentThread = PR_TRUE
);

/**
 * Call this method to temporarily disable the message observer configured
 * for a message target.
 */
IPC_METHOD IPC_DisableMessageObserver(
  const nsID          &aTarget
);

/**
 * Call this method to re-enable the message observer configured for a
 * message target.
 */
IPC_METHOD IPC_EnableMessageObserver(
  const nsID          &aTarget
);

/**
 * This function sends a message to the IPC daemon asynchronously.  If
 * aReceiverID is non-zero, then the message is forwarded to the client
 * corresponding to that identifier.
 *
 * If there is no client corresponding to aRecieverID, then the IPC daemon will
 * simply drop the message.
 */
IPC_METHOD IPC_SendMessage(
  PRUint32             aReceiverID,
  const nsID          &aTarget,
  const PRUint8       *aData,
  PRUint32             aDataLen
);

/**
 * This function blocks the calling thread until a message for the given target
 * is received (optionally from the specified client).
 *
 * The aSenderID parameter is interpreted as follows:
 *  o  If aSenderID is 0, then this function waits for a message to be sent by
 *     the IPC daemon.
 *  o  If aSenderID is IPC_SENDER_ANY, then this function waits for a message
 *     to be sent from any source.
 *  o  Otherwise, this function waits for a message to be sent by the client
 *     with ID given by aSenderID.  If aSenderID does not identify a valid
 *     client, then this function will return an error.
 *
 * The aObserver parameter is interpreted as follows:
 *  o  If aObserver is null, then the default message observer for the target
 *     is invoked when the next message is received.
 *  o  Otherwise, aObserver will be inovked when the next message is received.
 *
 * The aTimeout parameter is interpreted as follows:
 *  o  If aTimeout is PR_INTERVAL_NO_TIMEOUT, then this function will block
 *     until a matching message is received.
 *  o  If aTimeout is PR_INTERVAL_NO_WAIT, then this function will only inspect
 *     the current queue of messages.  If no matching message is found, then
 *     IPC_ERROR_WOULD_BLOCK is returned.
 *  o  Otherwise, aTimeout specifies the maximum amount of time to wait for a
 *     matching message to be received.  If no matching message is found after
 *     the timeout expires, then IPC_ERROR_WOULD_BLOCK is returned.
 *
 * If aObserver's OnMessageAvailable function returns IPC_WAIT_NEXT_MESSAGE,
 * then the function will continue blocking until the next matching message
 * is received.  Bypassed messages will be dispatched to the default message
 * observer when the event queue, associated with the thread that called
 * IPC_DefineTarget, is processed.
 *
 * This function runs the risk of hanging the calling thread indefinitely if
 * no matching message is ever received.
 */
IPC_METHOD IPC_WaitMessage(
  PRUint32             aSenderID,
  const nsID          &aTarget,
  ipcIMessageObserver *aObserver = nsnull,
  PRIntervalTime       aTimeout = PR_INTERVAL_NO_TIMEOUT
);

/*****************************************************************************/

/**
 * Returns the "ClientID" of the current process.
 */
IPC_METHOD IPC_GetID(
  PRUint32 *aClientID
);

/**
 * Adds a new name for the current process.  The IPC daemon is notified of this
 * change, which allows other processes to discover this process by the given
 * name.
 */
IPC_METHOD IPC_AddName(
  const char *aName
);

/**
 * Removes a name associated with the current process.
 */
IPC_METHOD IPC_RemoveName(
  const char *aName
);

/**
 * Adds client observer.  Will be called on the main thread.
 */
IPC_METHOD IPC_AddClientObserver(
  ipcIClientObserver *aObserver
);

/**
 * Removes client observer.
 */
IPC_METHOD IPC_RemoveClientObserver(
  ipcIClientObserver *aObserver
);

/**
 * Resolves the given client name to a client ID of a process connected to
 * the IPC daemon.
 */
IPC_METHOD IPC_ResolveClientName(
  const char *aName,
  PRUint32   *aClientID
);

/**
 * Tests whether the client is connected to the IPC daemon.
 */
IPC_METHOD IPC_ClientExists(
  PRUint32  aClientID,
  PRBool   *aResult
);

/*****************************************************************************/

/**
 * This class can be used to temporarily disable the default message observer
 * defined for a particular message target.
 */
class ipcDisableMessageObserverForScope
{
public:
  ipcDisableMessageObserverForScope(const nsID &aTarget)
    : mTarget(aTarget)
  {
    IPC_DisableMessageObserver(mTarget);
  }

  ~ipcDisableMessageObserverForScope()
  {
    IPC_EnableMessageObserver(mTarget);
  }

private:
  const nsID &mTarget;
};

#define IPC_DISABLE_MESSAGE_OBSERVER_FOR_SCOPE(_t) \
  ipcDisableMessageObserverForScope ipc_dmo_for_scope##_t(_t)

#endif /* ipcdclient_h__ */
