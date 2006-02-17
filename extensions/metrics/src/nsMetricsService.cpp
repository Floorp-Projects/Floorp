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
#include "nsLoadCollector.h"
#include "nsWindowCollector.h"
#include "nsIPropertyBag.h"
#include "nsIProperty.h"
#include "nsIVariant.h"
#include "nsIDOMElement.h"
#include "nsIDOMSerializer.h"
#include "nsMultiplexInputStream.h"
#include "nsStringStream.h"
#include "nsVariant.h"

// Flush the event log whenever its size exceeds this number of events.
#define NS_EVENTLOG_FLUSH_POINT 64

nsMetricsService* nsMetricsService::sMetricsService = nsnull;
#ifdef PR_LOGGING
PRLogModuleInfo *gMetricsLog;
#endif

//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS5_CI(nsMetricsService, nsIMetricsService,
                      nsIStreamListener, nsIRequestObserver,
                      nsIObserver, nsITimerCallback)

NS_IMETHODIMP
nsMetricsService::LogEvent(const nsAString &eventNS,
                           const nsAString &eventName,
                           nsIPropertyBag *eventProperties)
{
  NS_ENSURE_ARG_POINTER(eventProperties);

  if (mSuspendCount != 0)  // Ignore events while suspended
    return NS_OK;

  // Create a DOM element for the event and append it to our document.
  nsCOMPtr<nsIDOMElement> eventElement;
  nsresult rv = mDocument->CreateElementNS(eventNS, eventName,
                                           getter_AddRefs(eventElement));
  NS_ENSURE_SUCCESS(rv, rv);

  // Attach the given properties as attributes.
  nsCOMPtr<nsISimpleEnumerator> enumerator;
  rv = eventProperties->GetEnumerator(getter_AddRefs(enumerator));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupports> propertySupports;
  while (NS_SUCCEEDED(enumerator->GetNext(getter_AddRefs(propertySupports)))) {
    nsCOMPtr<nsIProperty> property = do_QueryInterface(propertySupports);
    if (!property) {
      NS_WARNING("PropertyBag enumerator has non-nsIProperty elements");
      continue;
    }

    nsAutoString name;
    rv = property->GetName(name);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to get property name");
      continue;
    }

    nsCOMPtr<nsIVariant> value;
    rv = property->GetValue(getter_AddRefs(value));
    if (NS_FAILED(rv) || !value) {
      NS_WARNING("Failed to get property value");
      continue;
    }

    nsAutoString valueString;
    rv = value->GetAsDOMString(valueString);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to convert property value to string");
      continue;
    }

    rv = eventElement->SetAttribute(name, valueString);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to set attribute value");
    }
    continue;
  }

  // Add the event timestamp
  nsAutoString timeString;
  timeString.AppendInt(PR_Now() / PR_USEC_PER_SEC);

  nsCOMPtr<nsIDOMNode> outChild;
  rv = mRoot->AppendChild(eventElement, getter_AddRefs(outChild));
  NS_ENSURE_SUCCESS(rv, rv);

  if (++mEventCount > NS_EVENTLOG_FLUSH_POINT)
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
    nsLoadCollector::Shutdown();
    nsWindowCollector::Shutdown();
  } else if (strcmp(topic, "profile-after-change") == 0) {
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
#ifdef PR_LOGGING
  gMetricsLog = PR_NewLogModule("nsMetricsService");
#endif

  nsresult rv;

  // Create an XML document to serve as the owner document for elements.
  mDocument = do_CreateInstance("@mozilla.org/xml/xml-document;1");
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);

  // Create a root log element.
  rv = CreateRoot();
  NS_ENSURE_SUCCESS(rv, rv);

  // Start up the collectors
  rv = nsWindowCollector::Startup();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = nsLoadCollector::Startup();
  NS_ENSURE_SUCCESS(rv, rv);

  // Hook ourselves up to observe notifications last.  This ensures that
  // we don't end up with an extra reference to the metrics service if
  // any of the above initialization fails.

  // Hook ourselves up to receive the xpcom shutdown event so we can properly
  // flush our data to disk.
  nsCOMPtr<nsIObserverService> obsSvc =
      do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = obsSvc->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = obsSvc->AddObserver(this, "profile-after-change", PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsMetricsService::CreateRoot()
{
  nsresult rv;
  nsCOMPtr<nsIDOMElement> root;
  rv = mDocument->CreateElementNS(NS_LITERAL_STRING(NS_METRICS_NAMESPACE),
                                  NS_LITERAL_STRING("log"),
                                  getter_AddRefs(root));
  NS_ENSURE_SUCCESS(rv, rv);

  mRoot = root;
  return NS_OK;
}

nsresult
nsMetricsService::GetDataFile(nsCOMPtr<nsILocalFile> *result)
{
  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = file->AppendNative(NS_LITERAL_CSTRING("metrics.xml"));
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

  // Serialize our document, then strip off the root start and end tags,
  // and write it out.

  nsCOMPtr<nsIDOMSerializer> ds =
    do_CreateInstance(NS_XMLSERIALIZER_CONTRACTID);
  NS_ENSURE_TRUE(ds, NS_ERROR_UNEXPECTED);

  nsAutoString docText;
  rv = ds->SerializeToString(mRoot, docText);
  NS_ENSURE_SUCCESS(rv, rv);

  // The first '>' will be the end of the root start tag.
  docText.Cut(0, docText.FindChar('>') + 1);

  // The last '<' will be the beginning of the root end tag.
  PRInt32 start = docText.RFindChar('<');
  docText.Cut(start, docText.Length() - start);

  NS_ConvertUTF16toUTF8 utf8Doc(docText);
  PRInt32 num = utf8Doc.Length();
  PRBool succeeded = ( PR_Write(fd, utf8Doc.get(), num) == num );

  PR_Close(fd);
  NS_ENSURE_STATE(succeeded);

  // Create a new mRoot
  rv = CreateRoot();
  NS_ENSURE_SUCCESS(rv, rv);

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

  // Construct a full XML document using the header, file contents, and
  // footer.
#define METRICS_XML_HEAD "<?xml version=\"1.0\"?>\n" \
                         "<log xmlns=\"http://www.mozilla.org/metrics\">\n"
#define METRICS_XML_TAIL "</log>"

  nsCOMPtr<nsIMultiplexInputStream> miStream =
    do_CreateInstance(NS_MULTIPLEXINPUTSTREAM_CONTRACTID);
  NS_ENSURE_STATE(miStream);

  nsCOMPtr<nsIInputStream> stringStream;
  rv = NS_NewCStringInputStream(getter_AddRefs(stringStream),
                                NS_LITERAL_CSTRING(METRICS_XML_HEAD));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = miStream->AppendStream(stringStream);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = miStream->AppendStream(fileStream);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = NS_NewCStringInputStream(getter_AddRefs(stringStream),
                                NS_LITERAL_CSTRING(METRICS_XML_TAIL));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = miStream->AppendStream(stringStream);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> uploadStream;
  NS_NewBufferedInputStream(getter_AddRefs(uploadStream), miStream, 4096);
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

/* static */ nsresult
nsMetricsUtils::PutUint16(nsIWritablePropertyBag *bag,
                          const nsAString &propertyName,
                          PRUint16 propertyValue)
{
  nsCOMPtr<nsIWritableVariant> var = new nsVariant();
  NS_ENSURE_TRUE(var, NS_ERROR_OUT_OF_MEMORY);
  var->SetAsUint16(propertyValue);
  return bag->SetProperty(propertyName, var);
}

/* static */ nsresult
nsMetricsUtils::NewPropertyBag(nsHashPropertyBag **result)
{
  nsRefPtr<nsHashPropertyBag> bag = new nsHashPropertyBag();
  NS_ENSURE_TRUE(bag, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = bag->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  bag.swap(*result);
  return NS_OK;
}
