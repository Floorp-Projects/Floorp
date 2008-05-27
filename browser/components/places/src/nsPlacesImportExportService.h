#ifndef nsPlacesImportExportService_h__
#define nsPlacesImportExportService_h__

#include "nsIPlacesImportExportService.h"

#include "nsCOMPtr.h"
#include "nsILocalFile.h"
#include "nsIOutputStream.h"
#include "nsIFaviconService.h"
#include "nsIAnnotationService.h"
#include "nsILivemarkService.h"
#include "nsINavHistoryService.h"
#include "nsINavBookmarksService.h"
#include "nsIMicrosummaryService.h"
#include "nsIChannel.h"

class nsPlacesImportExportService : public nsIPlacesImportExportService,
                                    public nsINavHistoryBatchCallback
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPLACESIMPORTEXPORTSERVICE
    NS_DECL_NSINAVHISTORYBATCHCALLBACK
    nsPlacesImportExportService();

  private:
    virtual ~nsPlacesImportExportService();

  protected:
    nsCOMPtr<nsIFaviconService> mFaviconService;
    nsCOMPtr<nsIAnnotationService> mAnnotationService;
    nsCOMPtr<nsINavBookmarksService> mBookmarksService;
    nsCOMPtr<nsINavHistoryService> mHistoryService;
    nsCOMPtr<nsILivemarkService> mLivemarkService;
    nsCOMPtr<nsIMicrosummaryService> mMicrosummaryService;

    nsCOMPtr<nsIChannel> mImportChannel;
    PRBool mIsImportDefaults;

    nsresult ImportHTMLFromFileInternal(nsILocalFile* aFile, PRBool aAllowRootChanges,
                                       PRInt64 aFolder, PRBool aIsImportDefaults);
    nsresult WriteContainer(nsINavHistoryResultNode* aFolder, const nsACString& aIndent, nsIOutputStream* aOutput);
    nsresult WriteContainerHeader(nsINavHistoryResultNode* aFolder, const nsACString& aIndent, nsIOutputStream* aOutput);
    nsresult WriteTitle(nsINavHistoryResultNode* aItem, nsIOutputStream* aOutput);
    nsresult WriteItem(nsINavHistoryResultNode* aItem, const nsACString& aIndent, nsIOutputStream* aOutput);
    nsresult WriteLivemark(nsINavHistoryResultNode* aFolder, const nsACString& aIndent, nsIOutputStream* aOutput);
    nsresult WriteContainerContents(nsINavHistoryResultNode* aFolder, const nsACString& aIndent, nsIOutputStream* aOutput);
    nsresult WriteSeparator(nsINavHistoryResultNode* aItem, const nsACString& aIndent, nsIOutputStream* aOutput);
    nsresult WriteDescription(PRInt64 aId, PRInt32 aType, nsIOutputStream* aOutput);

    inline nsresult EnsureServiceState() {
      NS_ENSURE_STATE(mHistoryService);
      NS_ENSURE_STATE(mFaviconService);
      NS_ENSURE_STATE(mAnnotationService);
      NS_ENSURE_STATE(mBookmarksService);
      NS_ENSURE_STATE(mLivemarkService);
      NS_ENSURE_STATE(mMicrosummaryService);
      return NS_OK;
    }
};

#endif // nsPlacesImportExportService_h__
