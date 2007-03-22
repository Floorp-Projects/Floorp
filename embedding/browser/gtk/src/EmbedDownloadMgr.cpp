/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 tw=80 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Christopher Blizzard. Portions created by Christopher Blizzard are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/**
 * Derived from GContentHandler http://landfill.mozilla.org/mxr-test/gnome/source/galeon/mozilla/ContentHandler.cpp
 */
#include "EmbedDownloadMgr.h"
#include "EmbedGtkTools.h"
#ifdef MOZILLA_INTERNAL_API
#include "nsXPIDLString.h"
#else
#include "nsComponentManagerUtils.h"
#endif
#include "nsIChannel.h"
#include "nsIWebProgress.h"
#include "nsIDOMWindow.h"
#include "nsIURI.h"
#include "nsCRT.h"
#include "nsIPromptService.h"
#include "nsIWebProgressListener2.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIFile.h"
#include "nsIDOMWindow.h"
#include "nsIExternalHelperAppService.h"
#include "nsCExternalHandlerService.h"
#include "nsMemory.h"
#include "nsNetError.h"
#include "nsIStreamListener.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsNetCID.h"
#include <unistd.h>
#include <gtkmozembed_download.h>
class EmbedDownloadMgr;
class ProgressListener : public nsIWebProgressListener2
{
public:
    ProgressListener(EmbedDownload *aDownload, nsCAutoString aFilename, nsISupports *aContext) : mFilename (aFilename)
    {
        mDownload = aDownload;
        mContext = aContext;
    };
    ~ProgressListener()
    {
    };
    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSIWEBPROGRESSLISTENER2
    EmbedDownload *mDownload;
    nsISupports *mContext;            /** < The context object */
    nsCOMPtr<nsILocalFile> mDestFile;
    nsCAutoString mFilename;
    nsCAutoString mLocalSaveFileName;
};

NS_IMPL_ISUPPORTS2(ProgressListener, nsIWebProgressListener2, nsIWebProgressListener)
NS_IMPL_ISUPPORTS1(EmbedDownloadMgr, nsIHelperAppLauncherDialog)

EmbedDownloadMgr::EmbedDownloadMgr(void)
{
}

EmbedDownloadMgr::~EmbedDownloadMgr(void)
{
}

nsresult
EmbedDownloadMgr::Init()
{
  return NS_OK;
}

NS_IMETHODIMP
EmbedDownloadMgr::Show(nsIHelperAppLauncher *aLauncher, nsISupports *aContext, PRUint32 aForced)
{
  nsresult rv;
  mContext = aContext;
  mLauncher = aLauncher;
  rv = GetDownloadInfo();
  return NS_OK;
}

NS_METHOD EmbedDownloadMgr::GetDownloadInfo (void)
{
  nsresult rv;
  // create a Download object
  GtkObject* instance = gtk_moz_embed_download_new ();
  EmbedDownload *download = (EmbedDownload *) GTK_MOZ_EMBED_DOWNLOAD(instance)->data;
  // get file mimetype
  rv = mLauncher->GetMIMEInfo (getter_AddRefs(mMIMEInfo));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  nsCAutoString aMimeType;
  rv = mMIMEInfo->GetMIMEType (aMimeType);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  // get file name
  nsCAutoString aTempFileName;
  nsAutoString aSuggestedFileName;
  rv = mLauncher->GetSuggestedFileName (aSuggestedFileName);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  aTempFileName = NS_ConvertUTF16toUTF8 (aSuggestedFileName);
  // get source url (concatened to file name)
  rv = mLauncher->GetSource (getter_AddRefs(mUri));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  rv = mUri->Resolve(NS_LITERAL_CSTRING("."), mSpec);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  rv = mLauncher->GetTargetFile(getter_AddRefs(mDestFileTemp));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  download->file_target = mDestFileTemp;
  // creating a progress listener to follow the download and connecting it to the launcher which controls the download.
  nsCOMPtr<nsIWebProgressListener2> listener = new ProgressListener(download, aTempFileName, mContext);
  rv = mLauncher->SetWebProgressListener(listener);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  // setting download vars to keep control of each download.
  download->parent = instance;
  download->started = 0;
  download->downloaded_size = -1;
  download->launcher = mLauncher;
  download->file_name = g_strdup ((gchar *) aTempFileName.get());
  download->server = g_strconcat(mSpec.get(), (gchar *) download->file_name, NULL);
  download->file_type = g_strdup (aMimeType.get());
  return NS_OK;
}

// it should be used... but it's not possible to do it according ui flow
NS_IMETHODIMP EmbedDownloadMgr::PromptForSaveToFile (nsIHelperAppLauncher *aLauncher,
                                                        nsISupports *aWindowContext,
                                                        const PRUnichar *aDefaultFile,
                                                        const PRUnichar *aSuggestedFileExtension,
                                                        nsILocalFile **_retval)
{
  return NS_OK;
}

// nsIWebProgressListener Functions
// all these methods must be here due to nsIWebProgressListenr/2 inheritance
NS_IMETHODIMP ProgressListener::OnStatusChange (nsIWebProgress *aWebProgress,
                                                    nsIRequest *aRequest, nsresult aStatus,
                                                    const PRUnichar *aMessage)
{
  if (NS_SUCCEEDED (aStatus))
    return NS_OK;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP ProgressListener::OnStateChange (nsIWebProgress *aWebProgress,
                                                    nsIRequest *aRequest, PRUint32 aStateFlags,
                                                    nsresult aStatus)
{
  if (NS_SUCCEEDED (aStatus))
    return NS_OK;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP ProgressListener::OnProgressChange (nsIWebProgress *aWebProgress,
                                                    nsIRequest *aRequest, PRInt32 aCurSelfProgress,
                                                    PRInt32 aMaxSelfProgress, PRInt32 aCurTotalProgress,
                                                    PRInt32 aMaxTotalProgress)
{
  return OnProgressChange64 (aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress);
}

NS_IMETHODIMP ProgressListener::OnLocationChange (nsIWebProgress *aWebProgress,
                                                    nsIRequest *aRequest, nsIURI *location)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP ProgressListener::OnSecurityChange (nsIWebProgress *aWebProgress,
                                                    nsIRequest *aRequest, PRUint32 state)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIWebProgressListener 2
NS_IMETHODIMP ProgressListener::OnProgressChange64 (nsIWebProgress *aWebProgress,
                                                    nsIRequest *aRequest, PRInt64 aCurSelfProgress,
                                                    PRInt64 aMaxSelfProgress, PRInt64 aCurTotalProgress,
                                                    PRInt64 aMaxTotalProgress)
{
  nsresult rv;

  if (!mDownload) return NS_OK;
  if (mDownload->started == 0) {
    mDownload->request = aRequest;
    mDownload->started = 1;
    // it might not work when there is more than one browser window opened
    mDownload->file_size = aMaxSelfProgress;
    nsCOMPtr<nsIDOMWindow> parentDOMWindow = do_GetInterface (mContext);
    mDownload->gtkMozEmbedParentWidget = GetGtkWidgetForDOMWindow(parentDOMWindow);
    if (mDownload->gtkMozEmbedParentWidget) {
      gtk_signal_emit(GTK_OBJECT(mDownload->gtkMozEmbedParentWidget),
                                    moz_embed_signals[DOWNLOAD_REQUEST],
                                    mDownload->server,
                                    mDownload->file_name,
                                    mDownload->file_type,
                                    (gulong) mDownload->file_size,
                                    1);
    }
  }
  if (mDownload->started == 1) {
    // emit signal to get download progress and displays on download list dialog
    gtk_signal_emit(GTK_OBJECT(mDownload->parent),
                    moz_embed_download_signals[DOWNLOAD_STARTED_SIGNAL], &mDownload->file_name_with_path);
    // in this case, user has canceled download from UI.
    if (!mDownload->file_name_with_path) {
      gtk_moz_embed_download_do_command (GTK_MOZ_EMBED_DOWNLOAD (mDownload->parent), GTK_MOZ_EMBED_DOWNLOAD_CANCEL);
      return NS_OK;
    }
    // FIXME: Clean up this code bellow, please :)
    gchar *localUrl = nsnull, *localFileName = nsnull;
    // second step - the target file will be created
    if (g_str_has_prefix (mDownload->file_name_with_path, FILE_SCHEME)) {
      // if user has chosen to save file (contains file:// prefix)
      gchar *localUrlWithFileName = (g_strsplit (mDownload->file_name_with_path, FILE_SCHEME, -1))[1];
      gint i;
      gchar **localUrlSplitted = (char **) (g_strsplit(localUrlWithFileName, SLASH, -1));
      for(i = 0; localUrlSplitted[i]; i++);
          localFileName = localUrlSplitted[i-1];
      localUrl = (gchar *) (g_strsplit(localUrlWithFileName, localFileName, -1))[0];
    } else {
      // if user has chosen to open with application (in /var/tmp)
      localUrl = (char *) (g_strsplit (mDownload->file_name_with_path, mFilename.get(), -1))[0];
      localFileName = g_strdup ((gchar *) mFilename.get());
    }
    nsCAutoString localSavePath;
    if (localUrl) {
      localSavePath.Assign(localUrl);
      g_free (localUrl);
      localUrl = nsnull;
    }
    if (localFileName) {
      mLocalSaveFileName.Assign(localFileName);
      g_free (localFileName);
      localFileName = nsnull;
    }
    // create the file where the download will be moved to
    mDestFile = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
    mDestFile->InitWithNativePath(localSavePath);
    mDestFile->Create(nsIFile::NORMAL_FILE_TYPE, 0777);
    mDownload->started = 2;
  }
  // when download finishes (bytes downloaded = total file size), emit completed signal
  // FIXME: if you don't have aMaxSelfProgress available, the signal won't be emitted
  if (aCurSelfProgress == aMaxSelfProgress) {
    // signal to confirm that download has finished
    gtk_signal_emit(GTK_OBJECT(mDownload->parent),
                    moz_embed_download_signals[DOWNLOAD_COMPLETED_SIGNAL]);
  } else {
    // emit signal to get download progress and displays on download list dialog
    gtk_signal_emit(GTK_OBJECT(mDownload->parent),
                    moz_embed_download_signals[DOWNLOAD_PROGRESS_SIGNAL],
                    (gulong) aCurSelfProgress, (gulong) aMaxSelfProgress, 1);
  }
  // storing current downloaded size.
  mDownload->downloaded_size = (gulong) aCurSelfProgress;
  // moving the target file to the right place.
  rv = mDownload->file_target->MoveToNative (mDestFile, mLocalSaveFileName);
  return NS_OK;
}

NS_IMETHODIMP ProgressListener::OnRefreshAttempted(nsIWebProgress *aWebProgress,
                                                   nsIURI *aUri, PRInt32 aDelay,
                                                   PRBool aSameUri,
                                                   PRBool *allowRefresh)
{
  *allowRefresh = PR_TRUE;
  return NS_OK;
}
