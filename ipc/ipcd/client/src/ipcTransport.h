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

#ifndef ipcTransport_h__
#define ipcTransport_h__

#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "prmon.h"

#include "ipcMessage.h"
#include "ipcMessageQ.h"

//----------------------------------------------------------------------------
// ipcTransportObserver interface
//----------------------------------------------------------------------------

class ipcTransportObserver
{
public:
    virtual void OnConnectionLost() = 0;
    virtual void OnMessageAvailable(const ipcMessage *) = 0;
};

//-----------------------------------------------------------------------------
// ipcTransport
//-----------------------------------------------------------------------------

class ipcTransport : public nsISupports
{
public:
    NS_DECL_ISUPPORTS

    ipcTransport()
        : mMonitor(PR_NewMonitor())
        , mObserver(nsnull)
        , mIncomingMsgQ(nsnull)
        , mSyncReplyMsg(nsnull)
        , mSyncWaiting(nsnull)
        , mHaveConnection(PR_FALSE)
        {}

    virtual ~ipcTransport()
    {
        PR_DestroyMonitor(mMonitor);
    }

    nsresult Init(ipcTransportObserver *observer, PRUint32 *clientID);
    nsresult Shutdown();

    // takes ownership of |msg|
    nsresult SendMsg(ipcMessage *msg, PRBool sync = PR_FALSE);

public:
    //
    // internal to implementation
    //
    void OnMessageAvailable(ipcMessage *); // takes ownership

private:
    friend void IPC_OnMessageAvailable(ipcMessage *);
    friend void IPC_OnConnectionEnd(nsresult);

    //
    // helpers
    //
    void ProxyToMainThread(PLHandleEventProc);
    void ProcessIncomingMsgQ();

    PR_STATIC_CALLBACK(void *) ProcessIncomingMsgQ_EventHandler(PLEvent *);
    PR_STATIC_CALLBACK(void *) ConnectionLost_EventHandler(PLEvent *);
    PR_STATIC_CALLBACK(void)   Generic_EventCleanup(PLEvent *);

    nsresult SendMsg_Locked(ipcMessage *msg, PRBool sync, ipcMessage **syncReply);

    //
    // data
    //
    PRMonitor             *mMonitor;
    ipcTransportObserver  *mObserver; // weak reference
    ipcMessageQ           *mIncomingMsgQ;
    ipcMessage            *mSyncReplyMsg;
    PRPackedBool           mSyncWaiting;
    PRPackedBool           mHaveConnection;
};

#endif // !ipcTransport_h__
