#include "d:\progra~1\devstudio\vc\include\string.h"    // XXX HACK, needs to be removed.
#include "XSLProcessor.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"

static NS_DEFINE_CID(kXSLProcessorCID, MITRE_XSL_PROCESSOR_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* aServMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
    nsresult rv;
    nsIGenericFactory* fact;
    if (aClass.Equals(kXSLProcessorCID))
        rv = NS_NewGenericFactory(&fact, XSLProcessor::Create);
    else 
        rv = NS_ERROR_FAILURE;

    if (NS_SUCCEEDED(rv))
        *aFactory = fact;
    return rv;
}

extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(nsISupports* aServMgr , const char* aPath)
{
    nsresult rv;

    NS_WITH_SERVICE1(nsIComponentManager, compMgr,
                     aServMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    printf("Registering XSL Processor...\n");
    rv = compMgr->RegisterComponent(kXSLProcessorCID, 
      "Transformiix XSL Processor", 
      "component://netscape/document-transformer?type=text/xsl",
      aPath, PR_TRUE, PR_TRUE);

    return rv;
}

extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
    nsresult rv;
    NS_WITH_SERVICE1(nsIComponentManager, compMgr,
                     aServMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->UnregisterComponent(kXSLProcessorCID, aPath);

    return rv;
}
