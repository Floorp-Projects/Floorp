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

#include "ipcdclient.h"
#include "ipcConnection.h"
#include "ipcConfig.h"
#include "ipcMessageQ.h"
#include "ipcMessageUtils.h"
#include "ipcLog.h"
#include "ipcm.h"

#include "nsIFile.h"
#include "nsEventQueueUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsRefPtrHashtable.h"
#include "nsAutoLock.h"
#include "nsProxyRelease.h"
#include "nsCOMArray.h"

#include "prio.h"
#include "prproces.h"
#include "pratom.h"

/* ------------------------------------------------------------------------- */

#define IPC_REQUEST_TIMEOUT PR_SecondsToInterval(30)

/* ------------------------------------------------------------------------- */

class ipcTargetData
{
public:
  static NS_HIDDEN_(ipcTargetData*) Create();

  // threadsafe addref/release
  NS_HIDDEN_(nsrefcnt) AddRef()  { return PR_AtomicIncrement(&refcnt); }
  NS_HIDDEN_(nsrefcnt) Release() { PRInt32 r = PR_AtomicDecrement(&refcnt); if (r == 0) delete this; return r; }

  NS_HIDDEN_(void) SetObserver(ipcIMessageObserver *aObserver, PRBool aOnCurrentThread);

  // protects access to the members of this class
  PRMonitor *monitor;

  // this may be null
  nsCOMPtr<ipcIMessageObserver> observer;

  // the message observer is called via this event queue
  nsCOMPtr<nsIEventQueue> eventQ;
  
  // incoming messages are added to this list
  ipcMessageQ pendingQ;

  // non-zero if the observer has been disabled (this means that new messages
  // should not be dispatched to the observer until the observer is re-enabled
  // via IPC_EnableMessageObserver).
  PRInt32 observerDisabled;

private:

  ipcTargetData()
    : monitor(PR_NewMonitor())
    , observerDisabled(0)
    , refcnt(0)
    {}

  ~ipcTargetData()
  {
    if (monitor)
      PR_DestroyMonitor(monitor);
  }

  PRInt32 refcnt;
};

ipcTargetData *
ipcTargetData::Create()
{
  ipcTargetData *td = new ipcTargetData;
  if (!td)
    return NULL;

  if (!td->monitor)
  {
    delete td;
    return NULL;
  }
  return td;
}

void
ipcTargetData::SetObserver(ipcIMessageObserver *aObserver, PRBool aOnCurrentThread)
{
  observer = aObserver;

  if (aOnCurrentThread)
    NS_GetCurrentEventQ(getter_AddRefs(eventQ));
  else
    eventQ = nsnull;
}

/* ------------------------------------------------------------------------- */

typedef nsRefPtrHashtable<nsIDHashKey, ipcTargetData> ipcTargetMap; 

class ipcClientState
{
public:
  static NS_HIDDEN_(ipcClientState *) Create();

  ~ipcClientState()
  {
    if (monitor)
      PR_DestroyMonitor(monitor);
  }

  //
  // the monitor protects the targetMap and the connected flag.  
  //
  // NOTE: we use a PRMonitor for this instead of a PRLock because we need
  //       the lock to be re-entrant.  since we don't ever need to wait on
  //       this monitor, it might be worth it to implement a re-entrant
  //       wrapper for PRLock.
  //
  PRMonitor    *monitor;
  ipcTargetMap  targetMap;
  PRBool        connected;

  // our process's client id
  PRUint32      selfID; 

  nsCOMArray<ipcIClientObserver> clientObservers;

private:

  ipcClientState()
    : monitor(PR_NewMonitor())
    , connected(PR_FALSE)
    , selfID(0)
  {}
};

ipcClientState *
ipcClientState::Create()
{
  ipcClientState *cs = new ipcClientState;
  if (!cs)
    return NULL;

  if (!cs->monitor || !cs->targetMap.Init())
  {
    delete cs;
    return NULL;
  }

  return cs;
}

/* ------------------------------------------------------------------------- */

static ipcClientState *gClientState;

static PRBool
GetTarget(const nsID &aTarget, ipcTargetData **td)
{
  nsAutoMonitor mon(gClientState->monitor);
  return gClientState->targetMap.Get(nsIDHashKey(&aTarget).GetKey(), td);
}

static PRBool
PutTarget(const nsID &aTarget, ipcTargetData *td)
{
  nsAutoMonitor mon(gClientState->monitor);
  return gClientState->targetMap.Put(nsIDHashKey(&aTarget).GetKey(), td);
}

static void
DelTarget(const nsID &aTarget)
{
  nsAutoMonitor mon(gClientState->monitor);
  gClientState->targetMap.Remove(nsIDHashKey(&aTarget).GetKey());
}

/* ------------------------------------------------------------------------- */

static nsresult
GetDaemonPath(nsCString &dpath)
{
  nsCOMPtr<nsIFile> file;

  nsresult rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR,
                                       getter_AddRefs(file));
  if (NS_SUCCEEDED(rv))
  {
    rv = file->AppendNative(NS_LITERAL_CSTRING(IPC_DAEMON_APP_NAME));
    if (NS_SUCCEEDED(rv))
      rv = file->GetNativePath(dpath);
  }

  return rv;
}

/* ------------------------------------------------------------------------- */

static void
ProcessPendingQ(const nsID &aTarget)
{
  ipcMessageQ tempQ;

  nsRefPtr<ipcTargetData> td;
  if (GetTarget(aTarget, getter_AddRefs(td)))
  {
    nsAutoMonitor mon(td->monitor);

    // if the observer for this target has been temporarily disabled, then
    // we must not processing any pending messages at this time.

    if (!td->observerDisabled)
      td->pendingQ.MoveTo(tempQ);
  }

  // process pending queue outside monitor
  while (!tempQ.IsEmpty())
  {
    ipcMessage *msg = tempQ.First();

    if (td->observer)
      td->observer->OnMessageAvailable(msg->mMetaData,
                                       msg->Target(),
                                       (const PRUint8 *) msg->Data(),
                                       msg->DataLen());
    else
    {
      // the IPCM target does not have an observer, and therefore any IPCM
      // messages that make it here will simply be dropped.
      NS_ASSERTION(aTarget.Equals(IPCM_TARGET), "unexpected target");
      LOG(("dropping IPCM message: type=%x\n", IPCM_GetType(msg)));
    }
    tempQ.DeleteFirst();
  }
}

/* ------------------------------------------------------------------------- */

// WaitTarget enables support for multiple threads blocking on the same
// message target.  the selector is called while inside the target's monitor.

typedef PRBool (* ipcMessageSelector)(
  void *arg,
  ipcTargetData *td,
  const ipcMessage *msg
);

// selects any
static PRBool
DefaultSelector(void *arg, ipcTargetData *td, const ipcMessage *msg)
{
  return PR_TRUE;
}

static nsresult
WaitTarget(const nsID           &aTarget,
           PRIntervalTime        aTimeout,
           ipcMessage          **aMsg,
           ipcMessageSelector    aSelector = nsnull,
           void                 *aArg = nsnull)
{
  *aMsg = nsnull;

  if (!aSelector)
    aSelector = DefaultSelector;

  nsRefPtr<ipcTargetData> td;
  if (!GetTarget(aTarget, getter_AddRefs(td)))
    return NS_ERROR_INVALID_ARG; // bad aTarget

  PRIntervalTime timeStart = PR_IntervalNow();
  PRIntervalTime timeEnd;
  if (aTimeout == PR_INTERVAL_NO_TIMEOUT)
    timeEnd = aTimeout;
  else if (aTimeout == PR_INTERVAL_NO_WAIT)
    timeEnd = timeStart;
  else
  {
    timeEnd = timeStart + aTimeout;

    // if overflowed, then set to max value
    if (timeEnd < timeStart)
      timeEnd = PR_INTERVAL_NO_TIMEOUT;
  }

  ipcMessage *lastChecked = nsnull, *beforeLastChecked = nsnull;
  nsresult rv = NS_ERROR_FAILURE;

  nsAutoMonitor mon(td->monitor);

  while (gClientState->connected)
  {
    NS_ASSERTION(!lastChecked, "oops");

    //
    // NOTE:
    //
    // we must start at the top of the pending queue, possibly revisiting
    // messages that our selector has already rejected.  this is necessary
    // because the queue may have been modified while we were waiting on
    // the monitor.  the impact of this on performance remains to be seen.
    //
    // one cheap solution is to keep a counter that is incremented each
    // time a message is removed from the pending queue.  that way we can
    // avoid revisiting all messages sometimes.
    //

    lastChecked = td->pendingQ.First();
    beforeLastChecked = nsnull;

    // loop over pending queue until we find a message that our selector likes.
    while (lastChecked)
    {
      if ((aSelector)(aArg, td, lastChecked))
      {
        // remove from pending queue
        if (beforeLastChecked)
          td->pendingQ.RemoveAfter(beforeLastChecked);
        else
          td->pendingQ.RemoveFirst();
        lastChecked->mNext = nsnull;

        *aMsg = lastChecked;
        break;
      }

      beforeLastChecked = lastChecked;
      lastChecked = lastChecked->mNext;
    }
      
    if (*aMsg)
    {
      rv = NS_OK;
      break;
    }

    // make sure we are still connected before waiting...
    if (!gClientState->connected)
    {
      rv = NS_ERROR_ABORT;
      break;
    }

    PRIntervalTime t = PR_IntervalNow();
    if (t > timeEnd) // check if timeout has expired
    {
      rv = IPC_ERROR_WOULD_BLOCK;
      break;
    }
    mon.Wait(timeEnd - t);

    LOG(("woke up from sleep [pendingQempty=%d connected=%d]\n",
          td->pendingQ.IsEmpty(), gClientState->connected));
  }

  return rv;
}

/* ------------------------------------------------------------------------- */

static void
PostEvent(nsIEventTarget *eventTarget, PLEvent *ev)
{
  if (!ev)
    return;

  nsresult rv = eventTarget->PostEvent(ev);
  if (NS_FAILED(rv))
  {
    NS_WARNING("PostEvent failed");
    PL_DestroyEvent(ev);
  }
}

static void
PostEventToMainThread(PLEvent *ev)
{
  nsCOMPtr<nsIEventQueue> eventQ;
  NS_GetMainEventQ(getter_AddRefs(eventQ));
  if (!eventQ)
  {
    NS_WARNING("unable to get reference to main event queue");
    PL_DestroyEvent(ev);
    return;
  }
  PostEvent(eventQ, ev);
}

/* ------------------------------------------------------------------------- */

class ipcEvent_ClientState : public PLEvent
{
public:
  ipcEvent_ClientState(PRUint32 aClientID, PRUint32 aClientState)
    : mClientID(aClientID)
    , mClientState(aClientState)
  {
    PL_InitEvent(this, nsnull, HandleEvent, DestroyEvent);
  }

  PR_STATIC_CALLBACK(void *) HandleEvent(PLEvent *ev)
  {
    // maybe we've been shutdown!
    if (!gClientState)
      return nsnull;

    ipcEvent_ClientState *self = (ipcEvent_ClientState *) ev;

    for (PRInt32 i=0; i<gClientState->clientObservers.Count(); ++i)
      gClientState->clientObservers[i]->OnClientStateChange(self->mClientID,
                                                            self->mClientState);
    return nsnull;
  }

  PR_STATIC_CALLBACK(void) DestroyEvent(PLEvent *ev)
  {
    delete (ipcEvent_ClientState *) ev;
  }

private:
  PRUint32 mClientID;
  PRUint32 mClientState;
};

/* ------------------------------------------------------------------------- */

class ipcEvent_ProcessPendingQ : public PLEvent
{
public:
  ipcEvent_ProcessPendingQ(const nsID &aTarget)
    : mTarget(aTarget)
  {
    PL_InitEvent(this, nsnull, HandleEvent, DestroyEvent);
  }

  PR_STATIC_CALLBACK(void *) HandleEvent(PLEvent *ev)
  {
    ProcessPendingQ(((ipcEvent_ProcessPendingQ *) ev)->mTarget);
    return nsnull;
  }

  PR_STATIC_CALLBACK(void) DestroyEvent(PLEvent *ev)
  {
    delete (ipcEvent_ProcessPendingQ *) ev;
  }

private:
  const nsID mTarget;
};

static void
CallProcessPendingQ(const nsID &target, ipcTargetData *td)
{
  // we assume that we are inside td's monitor 

  PLEvent *ev = new ipcEvent_ProcessPendingQ(target);
  if (!ev)
    return;

  nsresult rv;

  if (td->eventQ)
    rv = td->eventQ->PostEvent(ev);
  else
    rv = IPC_DoCallback((ipcCallbackFunc) PL_HandleEvent, ev);

  if (NS_FAILED(rv))
    PL_DestroyEvent(ev);
}

/* ------------------------------------------------------------------------- */

static void
DisableMessageObserver(const nsID &aTarget)
{
  nsRefPtr<ipcTargetData> td;
  if (GetTarget(aTarget, getter_AddRefs(td)))
  {
    nsAutoMonitor mon(td->monitor);
    ++td->observerDisabled;
  }
}

static void
EnableMessageObserver(const nsID &aTarget)
{
  nsRefPtr<ipcTargetData> td;
  if (GetTarget(aTarget, getter_AddRefs(td)))
  {
    nsAutoMonitor mon(td->monitor);
    if (td->observerDisabled > 0 && --td->observerDisabled == 0)
      if (!td->pendingQ.IsEmpty())
        CallProcessPendingQ(aTarget, td);
  }
}

/* ------------------------------------------------------------------------- */

// selects the next IPCM message with matching request index
static PRBool
WaitIPCMResponseSelector(void *arg, ipcTargetData *td, const ipcMessage *msg)
{
  PRUint32 requestIndex = *(PRUint32 *) arg;
  return IPCM_GetRequestIndex(msg) == requestIndex;
}

// wait for an IPCM response message.  if responseMsg is null, then it is
// assumed that the caller does not care to get a reference to the 
// response itself.  if the response is an IPCM_MSG_ACK_RESULT, then the
// status code is mapped to a nsresult and returned by this function.
static nsresult
WaitIPCMResponse(PRUint32 requestIndex, ipcMessage **responseMsg = nsnull)
{
  ipcMessage *msg;

  nsresult rv = WaitTarget(IPCM_TARGET, IPC_REQUEST_TIMEOUT, &msg,
                           WaitIPCMResponseSelector, &requestIndex);
  if (NS_FAILED(rv))
    return rv;

  if (IPCM_GetType(msg) == IPCM_MSG_ACK_RESULT)
  {
    ipcMessageCast<ipcmMessageResult> result(msg);
    if (result->Status() < 0)
      rv = NS_ERROR_FAILURE; // XXX nsresult_from_ipcm_result()
    else
      rv = NS_OK;
  }

  if (responseMsg)
    *responseMsg = msg;
  else
    delete msg;

  return rv;
}

// make an IPCM request and wait for a response.
static nsresult
MakeIPCMRequest(ipcMessage *msg, ipcMessage **responseMsg = nsnull)
{
  if (!msg)
    return NS_ERROR_OUT_OF_MEMORY;

  PRUint32 requestIndex = IPCM_GetRequestIndex(msg);

  // suppress 'ProcessPendingQ' for IPCM messages until we receive the
  // response to this IPCM request.  if we did not do this then there
  // would be a race condition leading to the possible removal of our
  // response from the pendingQ between sending the request and waiting
  // for the response.
  DisableMessageObserver(IPCM_TARGET);

  nsresult rv = IPC_SendMsg(msg);
  if (NS_SUCCEEDED(rv))
    rv = WaitIPCMResponse(requestIndex, responseMsg);

  EnableMessageObserver(IPCM_TARGET);
  return rv;
}

/* ------------------------------------------------------------------------- */

static void
RemoveTarget(const nsID &aTarget, PRBool aNotifyDaemon)
{
  DelTarget(aTarget);

  if (aNotifyDaemon)
  {
    nsresult rv = MakeIPCMRequest(new ipcmMessageClientDelTarget(aTarget));
    if (NS_FAILED(rv))
      LOG(("failed to delete target: rv=%x\n", rv));
  }
}

static nsresult
DefineTarget(const nsID           &aTarget,
             ipcIMessageObserver  *aObserver,
             PRBool                aOnCurrentThread,
             PRBool                aNotifyDaemon,
             ipcTargetData       **aResult)
{
  nsresult rv;

  nsRefPtr<ipcTargetData> td( ipcTargetData::Create() );
  if (!td)
    return NS_ERROR_OUT_OF_MEMORY;
  td->SetObserver(aObserver, aOnCurrentThread);

  if (!PutTarget(aTarget, td))
    return NS_ERROR_OUT_OF_MEMORY;

  if (aNotifyDaemon)
  {
    rv = MakeIPCMRequest(new ipcmMessageClientAddTarget(aTarget));
    if (NS_FAILED(rv))
    {
      LOG(("failed to add target: rv=%x\n", rv));
      RemoveTarget(aTarget, PR_FALSE);
      return rv;
    }
  }

  if (aResult)
    NS_ADDREF(*aResult = td);
  return NS_OK;
}

/* ------------------------------------------------------------------------- */

static nsresult
TryConnect()
{
  nsCAutoString dpath;
  nsresult rv = GetDaemonPath(dpath);
  if (NS_FAILED(rv))
    return rv;
  
  rv = IPC_Connect(dpath.get());
  if (NS_FAILED(rv))
    return rv;

  gClientState->connected = PR_TRUE;

  rv = DefineTarget(IPCM_TARGET, nsnull, PR_FALSE, PR_FALSE, nsnull);
  if (NS_FAILED(rv))
    return rv;

  ipcMessage *msg;

  // send CLIENT_HELLO and wait for CLIENT_ID response...
  rv = MakeIPCMRequest(new ipcmMessageClientHello(), &msg);
  if (NS_FAILED(rv))
    return rv;

  if (IPCM_GetType(msg) == IPCM_MSG_ACK_CLIENT_ID)
    gClientState->selfID = ipcMessageCast<ipcmMessageClientID>(msg)->ClientID();
  else
  {
    LOG(("unexpected response from CLIENT_HELLO message: type=%x!\n",
        IPCM_GetType(msg)));
    rv = NS_ERROR_UNEXPECTED;
  }

  delete msg;
  return rv;
}

nsresult
IPC_Init()
{
  NS_ENSURE_TRUE(!gClientState, NS_ERROR_ALREADY_INITIALIZED);

  IPC_InitLog(">>>");

  gClientState = ipcClientState::Create();
  if (!gClientState)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = TryConnect();
  if (NS_FAILED(rv))
    IPC_Shutdown();

  return rv;
}

nsresult
IPC_Shutdown()
{
  NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

  if (gClientState->connected)
    IPC_Disconnect();

  delete gClientState;
  gClientState = nsnull;

  return NS_OK;
}

/* ------------------------------------------------------------------------- */

nsresult
IPC_DefineTarget(const nsID          &aTarget,
                 ipcIMessageObserver *aObserver,
                 PRBool               aOnCurrentThread)
{
  NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

  // do not permit the re-definition of the IPCM protocol's target.
  if (aTarget.Equals(IPCM_TARGET))
    return NS_ERROR_INVALID_ARG;

  nsresult rv;

  nsRefPtr<ipcTargetData> td;
  if (GetTarget(aTarget, getter_AddRefs(td)))
  {
    // clear out observer before removing target since we want to ensure that
    // the observer is released on the main thread.
    {
      nsAutoMonitor mon(td->monitor);
      td->SetObserver(aObserver, aOnCurrentThread);
    }

    // remove target outside of td's monitor to avoid holding the monitor
    // while entering the client state's monitor.
    if (!aObserver)
      RemoveTarget(aTarget, PR_TRUE);

    rv = NS_OK;
  }
  else
  {
    if (aObserver)
      rv = DefineTarget(aTarget, aObserver, aOnCurrentThread, PR_TRUE, nsnull);
    else
      rv = NS_ERROR_INVALID_ARG; // unknown target
  }

  return rv;
}

nsresult
IPC_DisableMessageObserver(const nsID &aTarget)
{
  NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

  // do not permit modifications to the IPCM protocol's target.
  if (aTarget.Equals(IPCM_TARGET))
    return NS_ERROR_INVALID_ARG;

  DisableMessageObserver(aTarget);
  return NS_OK;
}

nsresult
IPC_EnableMessageObserver(const nsID &aTarget)
{
  NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

  // do not permit modifications to the IPCM protocol's target.
  if (aTarget.Equals(IPCM_TARGET))
    return NS_ERROR_INVALID_ARG;

  EnableMessageObserver(aTarget);
  return NS_OK;
}

nsresult
IPC_SendMessage(PRUint32       aReceiverID,
                const nsID    &aTarget,
                const PRUint8 *aData,
                PRUint32       aDataLen)
{
  NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

  // do not permit sending IPCM messages
  if (aTarget.Equals(IPCM_TARGET))
    return NS_ERROR_INVALID_ARG;

  nsresult rv;
  if (aReceiverID == 0)
  {
    ipcMessage *msg = new ipcMessage(aTarget, (const char *) aData, aDataLen);
    if (!msg)
      return NS_ERROR_OUT_OF_MEMORY;

    rv = IPC_SendMsg(msg);
  }
  else
    rv = MakeIPCMRequest(new ipcmMessageForward(IPCM_MSG_REQ_FORWARD,
                                                aReceiverID,
                                                aTarget,
                                                (const char *) aData,
                                                aDataLen));

  return rv;
}

struct WaitMessageSelectorData
{
  PRUint32             senderID;
  ipcIMessageObserver *observer;
};

static PRBool WaitMessageSelector(void *arg, ipcTargetData *td, const ipcMessage *msg)
{
  WaitMessageSelectorData *data = (WaitMessageSelectorData *) arg;

  nsresult rv = IPC_WAIT_NEXT_MESSAGE;

  if (msg->mMetaData == data->senderID)
  {
    ipcIMessageObserver *obs = data->observer;
    if (!obs)
      obs = td->observer;
    NS_ASSERTION(obs, "must at least have a default observer");

    rv = obs->OnMessageAvailable(msg->mMetaData,
                                 msg->Target(),
                                 (const PRUint8 *) msg->Data(),
                                 msg->DataLen());
  }

  // stop iterating if we got a match that the observer accepted.
  return rv != IPC_WAIT_NEXT_MESSAGE;
}

nsresult
IPC_WaitMessage(PRUint32             aSenderID,
                const nsID          &aTarget,
                ipcIMessageObserver *aObserver,
                PRIntervalTime       aTimeout)
{
  NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

  // do not permit waiting for IPCM messages
  if (aTarget.Equals(IPCM_TARGET))
    return NS_ERROR_INVALID_ARG;

  WaitMessageSelectorData data = { aSenderID, aObserver };

  ipcMessage *msg;
  nsresult rv = WaitTarget(aTarget, aTimeout, &msg, WaitMessageSelector, &data);
  if (NS_FAILED(rv))
    return rv;
  delete msg;

  return NS_OK;
}

/* ------------------------------------------------------------------------- */

nsresult
IPC_GetID(PRUint32 *aClientID)
{
  NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

  *aClientID = gClientState->selfID;
  return NS_OK;
}

nsresult
IPC_AddName(const char *aName)
{
  NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

  return MakeIPCMRequest(new ipcmMessageClientAddName(aName));
}

nsresult
IPC_RemoveName(const char *aName)
{
  NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

  return MakeIPCMRequest(new ipcmMessageClientDelName(aName));
}

/* ------------------------------------------------------------------------- */

nsresult
IPC_AddClientObserver(ipcIClientObserver *aObserver)
{
  NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

  return gClientState->clientObservers.AppendObject(aObserver)
      ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult
IPC_RemoveClientObserver(ipcIClientObserver *aObserver)
{
  NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

  for (PRInt32 i = 0; i < gClientState->clientObservers.Count(); ++i)
  {
    if (gClientState->clientObservers[i] == aObserver)
      gClientState->clientObservers.RemoveObjectAt(i);
  }

  return NS_OK;
}

/* ------------------------------------------------------------------------- */

// this function could be called on any thread
nsresult
IPC_ResolveClientName(const char *aName, PRUint32 *aClientID)
{
  NS_ENSURE_TRUE(gClientState, NS_ERROR_NOT_INITIALIZED);

  ipcMessage *msg;

  nsresult rv = MakeIPCMRequest(new ipcmMessageQueryClientByName(aName), &msg);
  if (NS_FAILED(rv))
    return rv;

  if (IPCM_GetType(msg) == IPCM_MSG_ACK_CLIENT_ID)
    *aClientID = ipcMessageCast<ipcmMessageClientID>(msg)->ClientID();
  else
  {
    LOG(("unexpected IPCM response: type=%x\n", IPCM_GetType(msg)));
    rv = NS_ERROR_UNEXPECTED;
  }
    
  delete msg;
  return rv;
}

/* ------------------------------------------------------------------------- */

nsresult
IPC_ClientExists(PRUint32 aClientID, PRBool *aResult)
{
  // this is a bit of a hack.  we forward a PING to the specified client.
  // the assumption is that the forwarding will only succeed if the client
  // exists, so we wait for the RESULT message corresponding to the FORWARD
  // request.  if that gives a successful status, then we know that the
  // client exists.

  ipcmMessagePing ping;

  return MakeIPCMRequest(new ipcmMessageForward(IPCM_MSG_REQ_FORWARD,
                                                aClientID,
                                                IPCM_TARGET,
                                                ping.Data(),
                                                ping.DataLen()));
}

/* ------------------------------------------------------------------------- */

nsresult
IPC_SpawnDaemon(const char *path)
{
  PRFileDesc *readable = nsnull, *writable = nsnull;
  PRProcessAttr *attr = nsnull;
  nsresult rv = NS_ERROR_FAILURE;
  char *const argv[] = { (char *const) path, nsnull };
  char c;

  // setup an anonymous pipe that we can use to determine when the daemon
  // process has started up.  the daemon will write a char to the pipe, and
  // when we read it, we'll know to proceed with trying to connect to the
  // daemon.

  if (PR_CreatePipe(&readable, &writable) != PR_SUCCESS)
    goto end;
  PR_SetFDInheritable(writable, PR_TRUE);

  attr = PR_NewProcessAttr();
  if (!attr)
    goto end;

  if (PR_ProcessAttrSetInheritableFD(attr, writable, IPC_STARTUP_PIPE_NAME) != PR_SUCCESS)
    goto end;

  if (PR_CreateProcessDetached(path, argv, nsnull, attr) != PR_SUCCESS)
    goto end;

  if ((PR_Read(readable, &c, 1) != 1) && (c != IPC_STARTUP_PIPE_MAGIC))
    goto end;

  rv = NS_OK;
end:
  if (readable)
    PR_Close(readable);
  if (writable)
    PR_Close(writable);
  if (attr)
    PR_DestroyProcessAttr(attr);
  return rv;
}

/* ------------------------------------------------------------------------- */

PR_STATIC_CALLBACK(PLDHashOperator)
EnumerateTargetMapAndNotify(const nsID    &aKey,
                            ipcTargetData *aData,
                            void          *aClosure)
{
  nsAutoMonitor mon(aData->monitor);

  // wake up anyone waiting on this target.
  mon.NotifyAll();

  return PL_DHASH_NEXT;
}

// called on a background thread
void
IPC_OnConnectionEnd(nsresult error)
{
  // now, go through the target map, and tickle each monitor.  that should
  // unblock any calls to WaitTarget.

  nsAutoMonitor mon(gClientState->monitor);
  gClientState->connected = PR_FALSE;
  gClientState->targetMap.EnumerateRead(EnumerateTargetMapAndNotify, nsnull);
}

/* ------------------------------------------------------------------------- */

#ifdef IPC_LOGGING
#include "prprf.h"
#include <ctype.h>
#endif

// called on a background thread
void
IPC_OnMessageAvailable(ipcMessage *msg)
{
#ifdef IPC_LOGGING
  if (LOG_ENABLED())
  {
    char *targetStr = msg->Target().ToString();
    LOG(("got message for target: %s\n", targetStr));
    nsMemory::Free(targetStr);

    IPC_LogBinary((const PRUint8 *) msg->Data(), msg->DataLen());
  }
#endif

  if (msg->Target().Equals(IPCM_TARGET))
  {
    switch (IPCM_GetType(msg))
    {
      // if this is a forwarded message, then post the inner message instead.
      case IPCM_MSG_PSH_FORWARD:
      {
        ipcMessageCast<ipcmMessageForward> fwd(msg);
        ipcMessage *innerMsg = new ipcMessage(fwd->InnerTarget(),
                                              fwd->InnerData(),
                                              fwd->InnerDataLen());
        // store the sender's client id in the meta-data field of the message.
        innerMsg->mMetaData = fwd->ClientID();

        delete msg;

        // recurse so we can handle forwarded IPCM messages
        IPC_OnMessageAvailable(innerMsg);
        return;
      }
      case IPCM_MSG_PSH_CLIENT_STATE:
      {
        ipcMessageCast<ipcmMessageClientState> status(msg);
        PostEventToMainThread(new ipcEvent_ClientState(status->ClientID(),
                                                       status->ClientState()));
        return;
      }
    }
  }

  nsRefPtr<ipcTargetData> td;
  if (GetTarget(msg->Target(), getter_AddRefs(td)))
  {
    nsAutoMonitor mon(td->monitor);

    // we only want to dispatch a 'ProcessPendingQ' event if we have not
    // already done so.
    PRBool dispatchEvent = td->pendingQ.IsEmpty();

    // put this message on our pending queue
    td->pendingQ.Append(msg);

    // make copy of target since |msg| may end up pointing to free'd memory
    // once we notify the monitor.
    const nsID target = msg->Target();

    LOG(("placed message on pending queue for target and notifying all...\n"));

    // wake up anyone waiting on this queue
    mon.NotifyAll();

    // proxy call to target's message procedure
    if (dispatchEvent)
      CallProcessPendingQ(target, td);
  }
  else
  {
    NS_WARNING("message target is undefined");
  }
}
