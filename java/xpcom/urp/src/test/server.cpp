#include "nsIServiceManager.h"
#include "nsIComponentManager.h"

#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIPref.h"
#include "plevent.h"
#include "prmem.h"
#include "prnetdb.h"

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

int main( int argc, char *argv[] ) {

        char *connectString = "socket,host=localhost,port=2009";
        if( argc == 2 ) connectString = argv[1];
            
	nsresult rv = NS_InitXPCOM(NULL, NULL);
	NS_ASSERTION( NS_SUCCEEDED(rv), "NS_InitXPCOM failed" );

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
        bcIStub *stub = NULL;
        urpITest *object = new urpTestImpl();
        object->AddRef();
        urpITest *proxy = NULL;
        xpcomStubsAndProxies->GetStub((nsISupports*)object, &stub);
        bcOID oid = orb->RegisterStub(stub);

	urpTransport* trans = new urpAcceptor();
	PRStatus status = trans->Open(connectString);
	if(status == PR_SUCCESS) printf("succes\n");
	else printf("failed\n");
	object->AddRef();
	urpManager* mngr = new urpManager(trans, orb);
	rv = NS_OK;
	while(NS_SUCCEEDED(rv)) {
		rv = mngr->HandleRequest(trans->GetConnection());
	}
	return 1;
}
