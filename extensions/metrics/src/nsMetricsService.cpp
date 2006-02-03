/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is the Metrics extension.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
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

#include "nsMetricsService.h"
#include "nsMetricsEvent.h"
#include "nsXPCOM.h"
#include "nsServiceManagerUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsNetUtil.h"
#include "nsIObserverService.h"
#include "nsIUpdateService.h"
#include "nsIUploadChannel.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIObserver.h"
#include "nsILocalFile.h"

// Flush the event log whenever its size exceeds this amount of bytes.
#define NS_EVENTLOG_FLUSH_POINT 4096

//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS5_CI(nsMetricsService, nsIMetricsService,
                      nsIStreamListener, nsIRequestObserver,
                      nsIObserver, nsITimerCallback)

NS_IMETHODIMP
nsMetricsService::LogCustomEvent(const nsACString &data)
{
  nsMetricsEvent event(NS_METRICS_CUSTOM_EVENT);
  PRUint32 len = data.Length();
  NS_ENSURE_ARG(len < PR_UINT16_MAX);
  event.PutInt16(len);
  event.PutBytes(data.BeginReading(), len);
  return LogEvent(event);
}

NS_IMETHODIMP
nsMetricsService::LogEvent(const nsMetricsEvent &event)
{
  if (mSuspendCount != 0)  // Ignore events while suspended
    return NS_OK;

  NS_ENSURE_ARG(PRUint32(event.Type()) < PR_UINT8_MAX);

  // The event header consists of the following fields:
  PRUint8 type;        // 8-bit event type
  PRUint32 timestamp;  // 32-bit timestamp (seconds since the epoch)

  type = (PRUint8) event.Type();
  timestamp = PRUint32(PR_Now() / PR_USEC_PER_SEC);

  mEventLog.AppendElement(type);
  mEventLog.AppendElements((PRUint8 *) &timestamp, sizeof(timestamp));
  mEventLog.AppendElements(event.Buffer(), event.BufferLength());

  if (mEventLog.Length() > NS_EVENTLOG_FLUSH_POINT)
    FlushData();
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsService::Upload()
{
  if (mUploading)  // Ignore new uploads issued while uploading.
    return NS_OK;

  // We suspend logging until the upload completes.

  nsresult rv = FlushData();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = UploadData();
  if (NS_SUCCEEDED(rv)) {
    mUploading = PR_TRUE;
    Suspend();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMetricsService::Suspend()
{
  mSuspendCount++;
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsService::Resume()
{
  if (mSuspendCount > 0)
    mSuspendCount--;
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsService::OnStartRequest(nsIRequest *request, nsISupports *context)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsService::OnStopRequest(nsIRequest *request, nsISupports *context,
                                nsresult status)
{
  nsCOMPtr<nsILocalFile> dataFile;
  GetDataFile(&dataFile);
  if (dataFile)
    dataFile->Remove(PR_FALSE);

  Resume();
  mUploading = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsService::OnDataAvailable(nsIRequest *request, nsISupports *context,
                                  nsIInputStream *stream, PRUint32 offset,
                                  PRUint32 count)
{
  // We don't expect to receive any data from an upload.
  return NS_ERROR_ABORT;
}

NS_IMETHODIMP
nsMetricsService::Observe(nsISupports *subject, const char *topic,
                          const PRUnichar *data)
{
  if (strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    FlushData();
  } else if (strcmp(topic, "app-startup") == 0) {
    PRInt32 interval = 86400000;  // 24 hours
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefs)
      prefs->GetIntPref("metrics.upload.interval", &interval);

    nsCOMPtr<nsIUpdateTimerManager> mgr =
        do_GetService("@mozilla.org/updates/timer-manager;1");
    if (mgr)
      mgr->RegisterTimer(NS_LITERAL_STRING("metrics-upload"), this, interval);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMetricsService::Notify(nsITimer *timer)
{
  // OK, we are ready to upload!
  Upload();
  return NS_OK;
}

nsresult
nsMetricsService::Init()
{
  nsresult rv;

  // Hook ourselves up to receive the xpcom shutdown event so we can properly
  // flush our data to disk.
  nsCOMPtr<nsIObserverService> obsSvc =
      do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = obsSvc->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  
  return NS_OK;
}

nsresult
nsMetricsService::GetDataFile(nsCOMPtr<nsILocalFile> *result)
{
  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = file->AppendNative(NS_LITERAL_CSTRING("metrics.dat"));
  NS_ENSURE_SUCCESS(rv, rv);

  *result = do_QueryInterface(file, &rv);
  return rv;
}

nsresult
nsMetricsService::OpenDataFile(PRUint32 flags, PRFileDesc **fd)
{
  nsCOMPtr<nsILocalFile> dataFile;
  nsresult rv = GetDataFile(&dataFile);
  NS_ENSURE_SUCCESS(rv, rv);

  return dataFile->OpenNSPRFileDesc(flags, 0600, fd);
}

nsresult
nsMetricsService::FlushData()
{
  nsresult rv;

  PRFileDesc *fd;
  rv = OpenDataFile(PR_WRONLY | PR_APPEND | PR_CREATE_FILE, &fd);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 num = mEventLog.Length();
  PRBool succeeded = ( PR_Write(fd, mEventLog.Elements(), num) == num );

  PR_Close(fd);
  NS_ENSURE_STATE(succeeded);

  mEventLog.SetLength(0);
  return NS_OK;
}

nsresult
nsMetricsService::UploadData()
{
  // TODO: 1) Submit a request to the server to figure out how much data to
  //          upload.  For now, we just submit all of the data.
  //       2) Prepare a data stream for upload that is prefixed with a PROFILE
  //          event.
 
  PRBool enable = PR_FALSE;
  nsXPIDLCString spec;
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    prefs->GetBoolPref("metrics.upload.enable", &enable);
    prefs->GetCharPref("metrics.upload.uri", getter_Copies(spec));
  }
  if (!enable || spec.IsEmpty())
    return NS_ERROR_ABORT;

  nsCOMPtr<nsILocalFile> file;
  nsresult rv = GetDataFile(&file);
  NS_ENSURE_SUCCESS(rv, rv);

  // NOTE: nsIUploadChannel requires a buffered stream to upload...

  nsCOMPtr<nsIInputStream> fileStream;
  NS_NewLocalFileInputStream(getter_AddRefs(fileStream), file);
  NS_ENSURE_STATE(fileStream);

  PRUint32 streamLen;
  rv = fileStream->Available(&streamLen);
  NS_ENSURE_SUCCESS(rv, rv);

  if (streamLen == 0)
    return NS_ERROR_ABORT;

  nsCOMPtr<nsIInputStream> uploadStream;
  NS_NewBufferedInputStream(getter_AddRefs(uploadStream), fileStream, 4096);
  NS_ENSURE_STATE(uploadStream);

  nsCOMPtr<nsIIOService> ios = do_GetIOService();
  NS_ENSURE_STATE(ios);

  nsCOMPtr<nsIChannel> channel;
  ios->NewChannel(spec, nsnull, nsnull, getter_AddRefs(channel));
  NS_ENSURE_STATE(channel); 

  nsCOMPtr<nsIUploadChannel> uploadChannel = do_QueryInterface(channel);
  NS_ENSURE_STATE(uploadChannel); 

  NS_NAMED_LITERAL_CSTRING(binaryType, "application/vnd.mozilla.metrics");
  rv = uploadChannel->SetUploadStream(uploadStream, binaryType, -1);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel);
  NS_ENSURE_STATE(httpChannel);
  rv = httpChannel->SetRequestMethod(NS_LITERAL_CSTRING("POST"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = channel->AsyncOpen(this, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}
