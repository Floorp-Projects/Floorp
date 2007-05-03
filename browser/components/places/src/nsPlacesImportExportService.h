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

class nsPlacesImportExportService : public nsIPlacesImportExportService
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPLACESIMPORTEXPORTSERVICE
    nsPlacesImportExportService();

  private:
    virtual ~nsPlacesImportExportService();

  protected:
    nsCOMPtr<nsIFaviconService> mFaviconService;
    nsCOMPtr<nsIAnnotationService> mAnnotationService;
    nsCOMPtr<nsINavBookmarksService> mBookmarksService;
    nsCOMPtr<nsINavHistoryService> mHistoryService;
    nsCOMPtr<nsILivemarkService> mLivemarkService;

    PRInt64 mPlacesRoot;
    PRInt64 mBookmarksRoot;
    PRInt64 mToolbarFolder;

    nsresult ImportHTMLFromFileInternal(nsILocalFile* aFile, PRBool aAllowRootChanges,
                                       PRInt64 aFolder, PRBool aIsImportDefaults);
    nsresult WriteContainer(PRInt64 aFolder, const nsACString& aIndent, nsIOutputStream* aOutput);
    nsresult WriteContainerHeader(PRInt64 aFolder, const nsACString& aIndent, nsIOutputStream* aOutput);
    nsresult WriteContainerTitle(PRInt64 aFolder, nsIOutputStream* aOutput);
    nsresult WriteItem(nsINavHistoryResultNode* aItem, const nsACString& aIndent, nsIOutputStream* aOutput);
    nsresult WriteLivemark(PRInt64 aFolderId, const nsACString& aIndent, nsIOutputStream* aOutput);
    nsresult WriteContainerContents(PRInt64 aFolder, const nsACString& aIndent, nsIOutputStream* aOutput);

    nsresult ArchiveBookmarksFile(PRInt32 aNumberOfBackups, PRBool aForceArchive);
};

#endif // nsPlacesImportExportService_h__
