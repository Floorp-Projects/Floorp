#define NS_IMPL_IDS
#include "nsISupports.h"
#include "nsRepository.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsString.h"

static NS_DEFINE_IID(kIObserverServiceIID, NS_IOBSERVERSERVICE_IID);
static NS_DEFINE_IID(kObserverServiceCID, NS_OBSERVERSERVICE_CID);


static NS_DEFINE_IID(kIObserverIID, NS_IOBSERVER_IID);
static NS_DEFINE_IID(kObserverCID, NS_OBSERVER_CID);

#define BASE_DLL   "raptorbase.dll"

int main(int argc, char *argv[])
{

	nsIObserverService *anObserverService = NULL;
    nsresult rv;

    nsComponentManager::RegisterComponent(kObserverServiceCID,
                                           "ObserverService", 
                                           NS_OBSERVERSERVICE_PROGID,
                                           BASE_DLL,PR_FALSE, PR_FALSE);

	nsresult res = nsRepository::CreateInstance(kObserverServiceCID,
												NULL,
												kIObserverServiceIID,
												(void **) &anObserverService);

	
	if (res == NS_OK) {

        nsString  aTopic("htmlparser");

        nsIObserver *anObserver;
        nsIObserver *aObserver = nsnull;
        nsIObserver *bObserver = nsnull;
            
		nsresult res = nsRepository::CreateInstance(kObserverCID,
												NULL,
												kIObserverIID,
												(void **) &anObserver);
                                                

        rv = NS_NewObserver(&aObserver);
		if (NS_FAILED(rv)) return rv;
    
        
        rv = anObserverService->AddObserver(&aObserver, &aTopic);
		if (NS_FAILED(rv)) return rv;
 
		
        rv = NS_NewObserver(&bObserver);
		if (NS_FAILED(rv)) return rv;

        rv = anObserverService->AddObserver(&bObserver, &aTopic);
		if (NS_FAILED(rv)) return rv;
 
		nsIEnumerator* e;
		rv = anObserverService->EnumerateObserverList(&e, &aTopic);
		if (NS_FAILED(rv)) return rv;
		nsISupports *inst;
 
        for (e->First(); e->IsDone() != NS_OK; e->Next()) {
            rv = e->CurrentItem(&inst);
            if (NS_SUCCEEDED(rv)) {
              rv = inst->QueryInterface(nsIObserver::GetIID(), (void**)&anObserver);
            }
            rv = anObserver->Notify(nsnull);
         }

        rv = anObserverService->RemoveObserver(&aObserver, &aTopic);
		if (NS_FAILED(rv)) return rv;

        rv = anObserverService->RemoveObserver(&bObserver, &aTopic);
		if (NS_FAILED(rv)) return rv;

  
        
	}
 	return NS_OK;
}

