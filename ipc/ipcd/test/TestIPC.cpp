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

#include "ipcIService.h"
#include "ipcIMessageObserver.h"
#include "ipcILockService.h"
#include "ipcCID.h"
#include "ipcLockCID.h"

#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIComponentRegistrar.h"

#include "nsString.h"
#include "prmem.h"

static const nsID kIPCMTargetID =
{ /* 753ca8ff-c8c2-4601-b115-8c2944da1150 */
    0x753ca8ff,
    0xc8c2,
    0x4601,
    {0xb1, 0x15, 0x8c, 0x29, 0x44, 0xda, 0x11, 0x50}
};

static const nsID kTestTargetID =
{ /* e628fc6e-a6a7-48c7-adba-f241d1128fb8 */
    0xe628fc6e,
    0xa6a7,
    0x48c7,
    {0xad, 0xba, 0xf2, 0x41, 0xd1, 0x12, 0x8f, 0xb8}
};

#define RETURN_IF_FAILED(rv, step) \
    PR_BEGIN_MACRO \
    if (NS_FAILED(rv)) { \
        printf("*** %s failed: rv=%x\n", step, rv); \
        return rv;\
    } \
    PR_END_MACRO

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static nsIEventQueue* gEventQ = nsnull;
static PRBool gKeepRunning = PR_TRUE;
static ipcIService *gIpcServ = nsnull;
static ipcILockService *gIpcLockServ = nsnull;

static void
SendMsg(ipcIService *ipc, PRUint32 cID, const nsID &target, const char *data, PRUint32 dataLen, PRBool sync = PR_FALSE)
{
    printf("*** sending message: [to-client=%u dataLen=%u]\n", cID, dataLen);

    nsresult rv;

    rv = ipc->SendMessage(cID, target, (const PRUint8 *) data, dataLen);
    if (NS_FAILED(rv)) {
        printf("*** sending message failed: rv=%x\n", rv);
        return;
    }

    if (sync) {
        rv = ipc->WaitMessage(cID, target, nsnull, PR_UINT32_MAX);
        if (NS_FAILED(rv))
            printf("*** waiting for message failed: rv=%x\n", rv);
    }
}

//-----------------------------------------------------------------------------

class myIpcMessageObserver : public ipcIMessageObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_IPCIMESSAGEOBSERVER
};
NS_IMPL_ISUPPORTS1(myIpcMessageObserver, ipcIMessageObserver)

NS_IMETHODIMP
myIpcMessageObserver::OnMessageAvailable(PRUint32 sender, const nsID &target, const PRUint8 *data, PRUint32 dataLen)
{
    printf("*** got message: [sender=%u data=%s]\n", sender, (const char *) data);
    return NS_OK;
}

//-----------------------------------------------------------------------------

#if 0
class myIpcClientQueryHandler : public ipcIClientQueryHandler
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_IPCICLIENTQUERYHANDLER
};

NS_IMPL_ISUPPORTS1(myIpcClientQueryHandler, ipcIClientQueryHandler)

NS_IMETHODIMP
myIpcClientQueryHandler::OnQueryComplete(PRUint32 aQueryID,
                                         nsresult aStatus,
                                         PRUint32 aClientID,
                                         const char **aNames,
                                         PRUint32 aNameCount,
                                         const nsID **aTargets,
                                         PRUint32 aTargetCount)
{
    printf("*** query complete [queryID=%u status=0x%x clientID=%u]\n",
            aQueryID, aStatus, aClientID);

    PRUint32 i;
    printf("***  names:\n");
    for (i = 0; i < aNameCount; ++i)
        printf("***    %d={%s}\n", i, aNames[i]);
    printf("***  targets:\n");
    for (i = 0; i < aTargetCount; ++i) {
        const char *trailer;
        if (aTargets[i]->Equals(kTestTargetID))
            trailer = " (TEST_TARGET_ID)";
        else if (aTargets[i]->Equals(kIPCMTargetID))
            trailer = " (IPCM_TARGET_ID)";
        else
            trailer = " (unknown)";
        char *str = aTargets[i]->ToString();
        printf("***    %d=%s%s\n", i, str, trailer);
        PR_Free(str);
    }

    if (aClientID != 0) {
        const char hello[] = "hello friend!";
        SendMsg(gIpcServ, aClientID, kTestTargetID, hello, sizeof(hello));
    }

    return NS_OK;
}
#endif

//-----------------------------------------------------------------------------

#if 0
class myIpcLockNotify : public ipcILockNotify
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_IPCILOCKNOTIFY
};

NS_IMPL_ISUPPORTS1(myIpcLockNotify, ipcILockNotify)

NS_IMETHODIMP
myIpcLockNotify::OnAcquireLockComplete(const char *lockName, nsresult status)
{
    printf("*** OnAcquireLockComplete [lock=%s status=%x]\n", lockName, status);
    gIpcLockServ->ReleaseLock(lockName);
    return NS_OK;
}
#endif

//-----------------------------------------------------------------------------

int main(int argc, char **argv)
{
    nsresult rv;

    {
        nsCOMPtr<nsIServiceManager> servMan;
        NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
        nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
        NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
        if (registrar)
            registrar->AutoRegister(nsnull);

        // Create the Event Queue for this thread...
        nsCOMPtr<nsIEventQueueService> eqs =
                 do_GetService(kEventQueueServiceCID, &rv);
        RETURN_IF_FAILED(rv, "do_GetService(EventQueueService)");

        rv = eqs->CreateMonitoredThreadEventQueue();
        RETURN_IF_FAILED(rv, "CreateMonitoredThreadEventQueue");

        rv = eqs->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);
        RETURN_IF_FAILED(rv, "GetThreadEventQueue");

        printf("*** getting ipc service\n");
        nsCOMPtr<ipcIService> ipcServ(do_GetService(IPC_SERVICE_CONTRACTID, &rv));
        RETURN_IF_FAILED(rv, "do_GetService(ipcServ)");
        NS_ADDREF(gIpcServ = ipcServ);

        if (argc > 1) {
            printf("*** using client name [%s]\n", argv[1]);
            gIpcServ->AddName(argv[1]);
        }

        ipcServ->DefineTarget(kTestTargetID, new myIpcMessageObserver(), PR_TRUE);

        const char data[] =
                "01 this is a really long message.\n"
                "02 this is a really long message.\n"
                "03 this is a really long message.\n"
                "04 this is a really long message.\n"
                "05 this is a really long message.\n"
                "06 this is a really long message.\n"
                "07 this is a really long message.\n"
                "08 this is a really long message.\n"
                "09 this is a really long message.\n"
                "10 this is a really long message.\n"
                "11 this is a really long message.\n"
                "12 this is a really long message.\n"
                "13 this is a really long message.\n"
                "14 this is a really long message.\n"
                "15 this is a really long message.\n"
                "16 this is a really long message.\n"
                "17 this is a really long message.\n"
                "18 this is a really long message.\n"
                "19 this is a really long message.\n"
                "20 this is a really long message.\n"
                "21 this is a really long message.\n"
                "22 this is a really long message.\n"
                "23 this is a really long message.\n"
                "24 this is a really long message.\n"
                "25 this is a really long message.\n"
                "26 this is a really long message.\n"
                "27 this is a really long message.\n"
                "28 this is a really long message.\n"
                "29 this is a really long message.\n"
                "30 this is a really long message.\n"
                "31 this is a really long message.\n"
                "32 this is a really long message.\n"
                "33 this is a really long message.\n"
                "34 this is a really long message.\n"
                "35 this is a really long message.\n"
                "36 this is a really long message.\n"
                "37 this is a really long message.\n"
                "38 this is a really long message.\n"
                "39 this is a really long message.\n"
                "40 this is a really long message.\n"
                "41 this is a really long message.\n"
                "42 this is a really long message.\n"
                "43 this is a really long message.\n"
                "44 this is a really long message.\n"
                "45 this is a really long message.\n"
                "46 this is a really long message.\n"
                "47 this is a really long message.\n"
                "48 this is a really long message.\n"
                "49 this is a really long message.\n"
                "50 this is a really long message.\n"
                "51 this is a really long message.\n"
                "52 this is a really long message.\n"
                "53 this is a really long message.\n"
                "54 this is a really long message.\n"
                "55 this is a really long message.\n"
                "56 this is a really long message.\n"
                "57 this is a really long message.\n"
                "58 this is a really long message.\n"
                "59 this is a really long message.\n"
                "60 this is a really long message.\n";
        SendMsg(ipcServ, 0, kTestTargetID, data, sizeof(data), PR_TRUE);

//        PRUint32 queryID;
//        nsCOMPtr<ipcIClientQueryHandler> handler(new myIpcClientQueryHandler());
//        ipcServ->QueryClientByName("foopy", handler, PR_FALSE, &queryID);

        PRUint32 foopyID;
        nsresult foopyRv = ipcServ->ResolveClientName("foopy", &foopyID);
        printf("*** query for 'foopy' returned [rv=%x id=%u]\n", foopyRv, foopyID);

        if (NS_SUCCEEDED(foopyRv)) {
            const char hello[] = "hello friend!";
            SendMsg(ipcServ, foopyID, kTestTargetID, hello, sizeof(hello));
        }

        //
        // test lock service
        //
        nsCOMPtr<ipcILockService> lockService = do_GetService(IPC_LOCKSERVICE_CONTRACTID, &rv);
        RETURN_IF_FAILED(rv, "do_GetService(ipcLockServ)");
        NS_ADDREF(gIpcLockServ = lockService);

        //nsCOMPtr<ipcILockNotify> notify(new myIpcLockNotify());
        gIpcLockServ->AcquireLock("blah", PR_TRUE);

        rv = gIpcLockServ->AcquireLock("foo", PR_TRUE);
        printf("*** sync AcquireLock returned [rv=%x]\n", rv);

        PLEvent *ev;
        while (gKeepRunning) {
            gEventQ->WaitForEvent(&ev);
            gEventQ->HandleEvent(ev);
        }

        NS_RELEASE(gIpcServ);

        printf("*** processing remaining events\n");

        // process any remaining events
        while (NS_SUCCEEDED(gEventQ->GetEvent(&ev)) && ev)
            gEventQ->HandleEvent(ev);

        printf("*** done\n");
    } // this scopes the nsCOMPtrs

    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    rv = NS_ShutdownXPCOM(nsnull);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");

    return 0;
}
