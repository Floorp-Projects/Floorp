#ifndef NSUNKNOWNCONTENTTYPEHANDLER_EMB
#define NSUNKNOWNCONTENTTYPEHANDLER_EMB

#include "nsIHelperAppLauncherDialog.h"
#include "nsIExternalHelperAppService.h"
#include "nsIWebProgressListener.h"
#include "nsIWebBrowserPersist.h"
#include "nsWeakReference.h"
#include "nsIWindowWatcher.h"
#include "nsIEmbeddingSiteWindow.h"
#include "PtMozilla.h"


static NS_DEFINE_CID( kCID, NS_IHELPERAPPLAUNCHERDIALOG_IID );

class nsUnknownContentTypeHandler : public nsIHelperAppLauncherDialog, nsIWebProgressListener {

public:

    nsUnknownContentTypeHandler();
    virtual ~nsUnknownContentTypeHandler();



    // This class implements the nsISupports interface functions.
    NS_DECL_ISUPPORTS

    // This class implements the nsIHelperAppLauncherDialog interface functions.
    NS_DECL_NSIHELPERAPPLAUNCHERDIALOG
		NS_DECL_NSIWEBPROGRESSLISTENER

		NS_IMETHOD ReportDownload( int type, int current, int total, char *message );

private:
		PtWidget_t* GetWebBrowser(nsIDOMWindow *aWindow);
		PtMozillaWidget_t *mMozillaWidget;
		char *mURL;
		long mDownloadTicket;

}; // nsUnknownContentTypeHandler

int Init_nsUnknownContentTypeHandler_Factory( );

/* download related */
typedef struct Download_ {
	int download_ticket;
	nsCOMPtr<nsIHelperAppLauncher> mLauncher;
	nsUnknownContentTypeHandler *unknown;
	} Download_t;
void AddDownload( PtMozillaWidget_t *moz, int download_ticket, nsIHelperAppLauncher* aLauncher, nsUnknownContentTypeHandler *unknown );
void RemoveDownload( PtMozillaWidget_t *moz, int download_ticket );
Download_t *FindDownload( PtMozillaWidget_t *moz, int download_ticket );

#endif
