#include "GeckoWindowCreator.h"
#include "GeckoContainer.h"

#include "nsIWebBrowserChrome.h"

GeckoWindowCreator::GeckoWindowCreator()
{
}

GeckoWindowCreator::~GeckoWindowCreator()
{
}

NS_IMPL_ISUPPORTS1(GeckoWindowCreator, nsIWindowCreator)

NS_IMETHODIMP
GeckoWindowCreator::CreateChromeWindow(nsIWebBrowserChrome *parent,
                                  PRUint32 chromeFlags,
                                  nsIWebBrowserChrome **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
//  AppCallbacks::CreateBrowserWindow(PRInt32(chromeFlags), parent, _retval);
    // TODO QI nsIGeckoContainer
    *_retval = nsnull;
    return *_retval ? NS_OK : NS_ERROR_FAILURE;
}