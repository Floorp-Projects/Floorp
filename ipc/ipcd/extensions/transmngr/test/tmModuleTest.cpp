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

// transaction manager includes
#include "ipcITransactionService.h"
#include "ipcITransactionObserver.h"

// ipc daemon includes
#include "ipcIService.h"

// core & xpcom ns includes
#include "nsDebug.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIComponentRegistrar.h"
#include "nsString.h"

// nspr includes
#include "prmem.h"
#include "plgetopt.h"
#include "nspr.h"
#include "prlog.h"

//////////////////////////////////////////////////////////////////////////////
// Testing/Debug/Logging BEGIN

const int NameSize = 1024;

/* command line options */
PRIntn      optDebug = 0;
char        optMode = 's';
char        *profileName = new char[NameSize];
char        *queueName = new char[NameSize];

char        *data = new char[NameSize];
PRUint32    dataLen = 10;         // includes the null terminator for "test data"

// Testing/Debug/Logging END
//////////////////////////////////////////////////////////////////////////////

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
//static PRInt32 gMsgCount = 0;
static ipcIService *gIpcServ = nsnull;
static ipcITransactionService *gTransServ = nsnull;

//-----------------------------------------------------------------------------

class myTransactionObserver : public ipcITransactionObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_IPCITRANSACTIONOBSERVER

    myTransactionObserver() { }
};

NS_IMPL_ISUPPORTS1(myTransactionObserver, ipcITransactionObserver)

NS_IMETHODIMP myTransactionObserver::OnTransactionAvailable(PRUint32 aQueueID, const PRUint8 *aData, PRUint32 aDataLen)
{
    printf("tmModuleTest: myTransactionObserver::OnTransactionAvailable [%s]\n", aData);
    return NS_OK;
}

NS_IMETHODIMP myTransactionObserver::OnAttachReply(PRUint32 aQueueID, PRUint32 aStatus)
{
    printf("tmModuleTest: myTransactionObserver::OnAttachReply [%d]\n", aStatus);
    return NS_OK;
}

NS_IMETHODIMP myTransactionObserver::OnDetachReply(PRUint32 aQueueID, PRUint32 aStatus)
{
    printf("tmModuleTest: myTransactionObserver::OnDetachReply [%d]\n", aStatus);
    return NS_OK;
}

NS_IMETHODIMP myTransactionObserver::OnFlushReply(PRUint32 aQueueID, PRUint32 aStatus)
{
    printf("tmModuleTest: myTransactionObserver::OnFlushReply [%d]\n", aStatus);
    return NS_OK;
}


//-----------------------------------------------------------------------------

int main(PRInt32 argc, char *argv[])
{
  nsresult rv;

  // default string values
  strcpy(profileName, "defaultProfile");
  strcpy(queueName, "defaultQueue");
  strcpy(data, "test data");

  { // scope the command line option gathering (needed for some reason)

    // Get command line options
    PLOptStatus os;
    PLOptState *opt = PL_CreateOptState(argc, argv, "bdfhlp:q:");

    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
	    if (PL_OPT_BAD == os) continue;
      switch (opt->option)
      {
        case 'b':  /* broadcast a bunch of messages */
          printf("tmModuleTest: broadcaster\n");
          optMode = 'b';
          break;
        case 'd':  /* debug mode */
          printf("tmModuleTest: debugging baby\n");
          optDebug = 1;
          break;
        case 'f':  /* broadcast and flush */
          printf("tmModuleTest: flusher\n");
          optMode = 'f';
          break;
        case 'h':  /* broadcast and detach */
          printf("tmModuleTest: hit-n-run\n");
          optMode = 'h';
          break;
        case 'l':  /* don't broadcast, just listen */
          printf("tmModuleTest: listener\n");
          optMode = 'l';
          break;
        case 'p':  /* set the profile name */
          strcpy(profileName, opt->value);
          printf("tmModuleTest: profilename:%s\n",profileName);
          break;
        case 'q':  /* set the queue name */
          strcpy(queueName, opt->value);
          printf("tmModuleTest: queuename:%s\n",queueName);
          break;
        default:
          printf("tmModuleTest: default\n");
          break;
      }
    }
    PL_DestroyOptState(opt);
  } // scope the command line option gathering (needed for some reason)

  { // scope the nsCOMPtrs

    printf("tmModuleTest: Starting xpcom\n");
   
    // xpcom startup stuff
    nsCOMPtr<nsIServiceManager> servMan;
    NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
    nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
    NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
    if (registrar)
      registrar->AutoRegister(nsnull);

    // Create the Event Queue for this thread...
    nsCOMPtr<nsIEventQueueService> eqs = do_GetService(kEventQueueServiceCID, &rv);
    RETURN_IF_FAILED(rv, "do_GetService(EventQueueService)");

    rv = eqs->CreateMonitoredThreadEventQueue();
    RETURN_IF_FAILED(rv, "CreateMonitoredThreadEventQueue");

    rv = eqs->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);
    RETURN_IF_FAILED(rv, "GetThreadEventQueue");

    // Need to make sure the ipc system has been started
    printf("tmModuleTest: getting ipc service\n");
    nsCOMPtr<ipcIService> ipcServ(do_GetService("@mozilla.org/ipc/service;1", &rv));
    RETURN_IF_FAILED(rv, "do_GetService(ipcServ)");
    NS_ADDREF(gIpcServ = ipcServ);

    // Get the transaction service
    printf("tmModuleTest: getting transaction service\n");
    nsCOMPtr<ipcITransactionService> transServ(do_GetService("@mozilla.org/ipc/transaction-service;1", &rv));
    RETURN_IF_FAILED(rv, "do_GetService(transServ)");
    NS_ADDREF(gTransServ = transServ);

    // transaction specifc startup stuff, done for all cases

    nsCOMPtr<ipcITransactionObserver> observ = new myTransactionObserver();

    // initialize the transaction service with a specific profile
    gTransServ->Init(nsDependentCString(profileName));
    printf("tmModuleTest: using profileName [%s]\n", profileName);

    // attach to the queue in the transaction manager
    gTransServ->Attach(nsDependentCString(queueName), observ, PR_TRUE);
    printf("tmModuleTest: observing queue [%s]\n", queueName);


    // run specific patterns based on the mode 
    int i = 0;       // wasn't working inside the cases
    switch (optMode)
    {
      case 's':
        printf("tmModuleTest: start standard\n");
        // post a couple events
        for (; i < 5 ; i++) {
          gTransServ->PostTransaction(nsDependentCString(queueName), (PRUint8 *)data, dataLen);
        }
        // listen for events
        while (gKeepRunning)
          gEventQ->ProcessPendingEvents();
        printf("tmModuleTest: end standard\n");
        break;
      case 'b':
        printf("tmModuleTest: start broadcast\n");
        // post a BUNCH of messages
        for (; i < 50; i++) {
          gTransServ->PostTransaction(nsDependentCString(queueName), (PRUint8 *)data, dataLen);
        }
        // listen for events
        while (gKeepRunning)
          gEventQ->ProcessPendingEvents();
        printf("tmModuleTest: end broadcast\n");
        break;
      case 'f':
        printf("tmModuleTest: start flush\n");
        // post a couple events
        for (; i < 5; i++) {
          gTransServ->PostTransaction(nsDependentCString(queueName), (PRUint8 *)data, dataLen);
        }
        // flush the queue
        gTransServ->Flush(nsDependentCString(queueName), PR_TRUE);
        // post a couple events
        for (i=0; i < 8; i++) {
          gTransServ->PostTransaction(nsDependentCString(queueName), (PRUint8 *)data, dataLen);
        }
        // listen for events
        while (gKeepRunning)
          gEventQ->ProcessPendingEvents();
        // detach
        gTransServ->Detach(nsDependentCString(queueName));
        printf("tmModuleTest: end flush\n");
        break;
      case 'h':
        printf("tmModuleTest: start hit-n-run\n");
        // post a couple events
        for (; i < 5; i++) {
          gTransServ->PostTransaction(nsDependentCString(queueName), (PRUint8 *)data, dataLen);
        }
        // detach
        gTransServ->Detach(nsDependentCString(queueName));
        printf("tmModuleTest: end hit-n-run\n");
        break;
      case 'l':
        printf("tmModuleTest: start listener\n");
        // listen for events
        while (gKeepRunning)
          gEventQ->ProcessPendingEvents();
        printf("tmModuleTest: end listener\n");
        break;
      default :
        printf("tmModuleTest: start & end default\n");
        break;
    }

    // shutdown process

    NS_RELEASE(gTransServ);
    NS_RELEASE(gIpcServ);

    printf("tmModuleTest: processing remaining events\n");

    // process any remaining events
    PLEvent *ev;
    while (NS_SUCCEEDED(gEventQ->GetEvent(&ev)) && ev)
      gEventQ->HandleEvent(ev);

    printf("tmModuleTest: done\n");
  } // this scopes the nsCOMPtrs

  // helps with shutdown on some cases
  PR_Sleep(PR_SecondsToInterval(4));

  // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
  rv = NS_ShutdownXPCOM(nsnull);
  NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");

  return 0;
}
