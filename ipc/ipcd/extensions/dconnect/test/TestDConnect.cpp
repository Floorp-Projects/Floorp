#include "ipcIService.h"
#include "ipcIDConnectService.h"
#include "ipcCID.h"

#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIComponentRegistrar.h"

#include "nsIURL.h"
#include "nsNetCID.h"

#include "nsString.h"
#include "prmem.h"

#define RETURN_IF_FAILED(rv, step) \
    PR_BEGIN_MACRO \
    if (NS_FAILED(rv)) { \
        printf("*** %s failed: rv=%x\n", step, rv); \
        return rv;\
    } \
    PR_END_MACRO

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);
static nsIEventQueue* gEventQ = nsnull;
static PRBool gKeepRunning = PR_TRUE;

static ipcIService *gIpcServ = nsnull;

static nsresult DoTest()
{
  nsresult rv;

  nsCOMPtr<ipcIDConnectService> dcon = do_GetService("@mozilla.org/ipc/dconnect-service;1", &rv);
  RETURN_IF_FAILED(rv, "getting dconnect service");

  PRUint32 remoteClientID = 1;

  nsCOMPtr<nsIURL> url;
  rv = dcon->CreateInstance(remoteClientID, kStandardURLCID, NS_GET_IID(nsIURL), getter_AddRefs(url)); 
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupports> sup = do_QueryInterface(url, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  printf("*** calling QueryInterface\n");
  nsCOMPtr<nsIURI> uri = do_QueryInterface(url, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_NAMED_LITERAL_CSTRING(spec, "http://www.mozilla.org/");

  printf("*** calling SetSpec\n");
  rv = uri->SetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString buf;
  rv = uri->GetSpec(buf);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!buf.Equals(spec))
  {
    printf("*** GetSpec erroneously returned [%s]\n", buf.get());
    return NS_ERROR_FAILURE;
  }

  PRBool match;
  rv = uri->SchemeIs("http", &match);
  if (NS_FAILED(rv) || !match)
  {
    printf("*** SchemeIs test failed [rv=%x]\n", rv);
    return NS_ERROR_FAILURE;
  }

  nsCString resolvedSpec;
  rv = uri->Resolve(NS_LITERAL_CSTRING("index.html"), resolvedSpec);
  if (NS_FAILED(rv) || !resolvedSpec.EqualsLiteral("http://www.mozilla.org/index.html"))
  {
    printf("*** Resolve test failed [rv=%x result=%s]\n", rv, resolvedSpec.get());
    return NS_ERROR_FAILURE;
  }

  PRInt32 port;
  rv = uri->GetPort(&port);
  if (NS_FAILED(rv) || port != -1)
  {
    printf("*** GetPort test failed [rv=%x port=%d]\n", rv, port);
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> clonedURI;
  rv = uri->Clone(getter_AddRefs(clonedURI));
  if (NS_FAILED(rv) || !clonedURI)
  {
    printf("*** Clone test failed [rv=%x]\n", rv);
    return NS_ERROR_FAILURE;
  }

  rv = clonedURI->GetSpec(buf);
  if (NS_FAILED(rv) || !buf.Equals(spec))
  {
    printf("*** Clone test (2) failed [rv=%x buf=%s]\n", rv, buf.get());
    return NS_ERROR_FAILURE;
  }

  url = do_QueryInterface(clonedURI, &rv);
  if (NS_FAILED(rv))
  {
    printf("*** QI test failed [rv=%x]\n", rv);
    return rv;
  }

  rv = url->SetFilePath(NS_LITERAL_CSTRING("/foo/bar/x.y.html?what"));
  if (NS_FAILED(rv))
  {
    printf("*** SetFilePath test failed [rv=%x]\n", rv);
    return rv;
  }

  printf("*** DoTest completed successfully :-)\n");
  return NS_OK;
}

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

    nsCOMPtr<ipcIService> ipcServ(do_GetService(IPC_SERVICE_CONTRACTID, &rv));
    RETURN_IF_FAILED(rv, "do_GetService(ipcServ)");
    NS_ADDREF(gIpcServ = ipcServ);

    if (argc > 1) {
        printf("*** using client name [%s]\n", argv[1]);
        gIpcServ->AddName(argv[1]);
    }

    rv = DoTest();
    RETURN_IF_FAILED(rv, "DoTest()");

    PLEvent *ev;
    while (gKeepRunning)
    {
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
}
