#define NS_IMPL_IDS
#include "nsISupports.h"
#include "nsIServiceManager.h"
#include "nsICookieService.h"


static NS_DEFINE_IID(kICookieServiceIID, NS_ICOOKIESERVICE_IID);
static NS_DEFINE_IID(kCookieServiceCID, NS_COOKIESERVICE_CID);



int main(PRInt32 argc, char *argv[])
{

    nsICookieService *cookieService = NULL;

    nsresult rv;

    rv = nsServiceManager::GetService(kCookieServiceCID,
                                    kICookieServiceIID,
                                    (nsISupports **)&cookieService);

	if (rv == NS_OK) {
     
	}
     
	return 0;
}

