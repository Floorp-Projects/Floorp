#include "nsIServiceManager.h"
#include "nsIComponentManager.h"

#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIPref.h"
#include "plevent.h"
#include "prmem.h"
#include "prnetdb.h"
#include "prthread.h"

#include "urpManager.h"
#include "urpTransport.h"

#include "urpITest.h"
#include "bcIORBComponent.h"
#include "bcORBComponentCID.h"
#include "urpTestImpl.h"
#include <unistd.h>

#include "nsIModule.h"

#include "bcIXPCOMStubsAndProxies.h"
#include "bcXPCOMStubsAndProxiesCID.h"

static NS_DEFINE_CID(kXPCOMStubsAndProxies,BC_XPCOMSTUBSANDPROXIES_CID);
static NS_DEFINE_CID(kORBCIID,BC_ORBCOMPONENT_CID);
static NS_DEFINE_CID(compManag, NS_COMPONENTMANAGER_CID);

struct localThreadArg {
    urpManager *mgr;
    urpConnection *conn;
    localThreadArg( urpManager *mgr, urpConnection *conn ) {
        this->mgr = mgr;
        this->conn = conn;
    }
};

void thread_start_server( void *arg )
{
    urpManager *manager = ((localThreadArg *)arg)->mgr;
    urpConnection *connection = ((localThreadArg *)arg)->conn;
    nsresult rv = manager->ReadMessage( connection, PR_FALSE );
}

int main( int argc, char *argv[] ) {

        char *connectString = "socket,host=localhost,port=2009";
        if( argc == 2 ) connectString = argv[1];
            
	nsresult rv = NS_InitXPCOM(NULL, NULL);
	NS_ASSERTION( NS_SUCCEEDED(rv), "NS_InitXPCOM failed" );

	(void) nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, nsnull);

	NS_WITH_SERVICE(bcIORBComponent, _orb, kORBCIID, &rv);
                    if (NS_FAILED(rv)) {
printf("NS_WITH_SERVICE(bcXPC in Marshal failed\n");
                    }

	NS_WITH_SERVICE(bcIXPCOMStubsAndProxies, xpcomStubsAndProxies, kXPCOMStubsAndProxies, &rv);
        if (NS_FAILED(rv)) {
           printf("bcXPCOMStubsAndProxie failed\n");
           return -1;
        }

	bcIORB *orb;
        _orb->GetORB(&orb);
        bcIStub *stub = nsnull;
	NS_WITH_SERVICE(nsIComponentManager, mComp, compManag, &rv);
//	nsIComponentManager* cm;
//        rv = NS_GetGlobalComponentManager(&cm);
	if (NS_FAILED(rv)) {
           printf("compManager failed\n");
           return -1;
        }
/*
        urpITest *proxy = nsnull;
        xpcomStubsAndProxies->GetStub((nsISupports*)mComp, &stub);
        bcOID oid = orb->RegisterStub(stub);
*/
	bcOID oid;
	xpcomStubsAndProxies->GetOID(mComp, orb, &oid);

	urpTransport* trans = new urpAcceptor();
	PRStatus status = trans->Open(connectString);
	if(status == PR_SUCCESS) printf("succes %ld\n",oid);
	else printf("failed\n");
	urpManager* mngr = new urpManager(PR_FALSE, orb, nsnull);
	rv = NS_OK;
	localThreadArg *arg = (localThreadArg*)PR_Malloc(sizeof(localThreadArg));
	PRThread *thr;
	while(NS_SUCCEEDED(rv)) {
	    // rv = mngr->HandleRequest(trans->GetConnection());
            urpConnection *conn = trans->GetConnection();
            if( conn != NULL ) {
                // handle request in new thread
//                localThreadArg *arg = new localThreadArg( mngr, conn );
		arg->mgr = mngr;
		arg->conn = conn;
                thr = PR_CreateThread( PR_USER_THREAD,
                                                  thread_start_server,
                                                  arg,
                                                  PR_PRIORITY_NORMAL,
                                                  PR_LOCAL_THREAD,
                                                  PR_UNJOINABLE_THREAD,
                                                  0);
                if( thr == nsnull ) {
                    rv = NS_ERROR_FAILURE;
                }
            } else {
                rv = NS_ERROR_FAILURE;
            }
	}
	return 1;
}
