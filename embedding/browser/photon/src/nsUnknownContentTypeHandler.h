#ifndef NSUNKNOWNCONTENTTYPEHANDLER_EMB
#define NSUNKNOWNCONTENTTYPEHANDLER_EMB

#include "nsIHelperAppLauncherDialog.h"
#include "nsIExternalHelperAppService.h"
#include "nsIWebProgressListener.h"
#include "nsIWebBrowserPersist.h"
#include "nsWeakReference.h"
#include "nsIWindowWatcher.h"
#include "WebBrowserContainer.h"


static NS_DEFINE_CID( kCID, NS_IHELPERAPPLAUNCHERDIALOG_IID );

class nsUnknownContentTypeHandler : public nsIHelperAppLauncherDialog {

public:

    nsUnknownContentTypeHandler();
    virtual ~nsUnknownContentTypeHandler();



    // This class implements the nsISupports interface functions.
    NS_DECL_ISUPPORTS

    // This class implements the nsIHelperAppLauncherDialog interface functions.
    NS_DECL_NSIHELPERAPPLAUNCHERDIALOG

private:
		CWebBrowserContainer* GetWebBrowser(nsIDOMWindow *aWindow);
}; // nsUnknownContentTypeHandler


class nsWebProgressListener : public nsIWebProgressListener,
															public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBPROGRESSLISTENER

  nsWebProgressListener();
  virtual ~nsWebProgressListener();
};


int Init_nsUnknownContentTypeHandler_Factory( );

#endif
