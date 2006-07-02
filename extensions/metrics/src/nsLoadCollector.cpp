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
 *  Brian Ryner <bryner@brianryner.com>
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

// This must be before any #includes to enable logging in release builds
#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif

#include "nsLoadCollector.h"
#include "nsWindowCollector.h"
#include "nsMetricsService.h"
#include "nsCOMPtr.h"
#include "nsCURILoader.h"
#include "nsIServiceManager.h"
#include "nsIWebProgress.h"
#include "nsIDocShell.h"
#include "nsIChannel.h"
#include "nsIDOMWindow.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIURI.h"
#include "nsIDOMDocument.h"

// Hack around internal string usage in nsIDocument.h on the branch
#ifdef MOZILLA_1_8_BRANCH
// nsAFlat[C]String is a deprecated synonym for ns[C]String
typedef nsString nsAFlatString;
typedef nsCString nsAFlatCString;
// nsXPIDLCString is a subclass of nsCString, but they're equivalent for the
// purposes of nsIDocument.h.
typedef nsCString nsXPIDLCString;
// This utility method isn't in the string glue on the branch.
inline void CopyASCIItoUCS2(const nsACString &src, nsAString &dest) {
  NS_CStringToUTF16(src, NS_CSTRING_ENCODING_ASCII, dest);
}
// Suppress inclusion of these headers
#define nsAString_h___
#define nsString_h___
#define nsReadableUtils_h___
#endif
#include "nsIDocument.h"
#ifdef MOZILLA_1_8_BRANCH
#undef nsAString_h___
#undef nsString_h___
#undef nsReadableUtils_h___
#endif

// This is needed to gain access to the LOAD_ defines in this file.
#define MOZILLA_INTERNAL_API
#include "nsDocShellLoadTypes.h"
#undef MOZILLA_INTERNAL_API

//-----------------------------------------------------------------------------

#if defined(__linux)
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
static FILE *sProcFP;
static void GetMemUsage_Shutdown() {
  if (sProcFP) {
    fclose(sProcFP);
    sProcFP = NULL;
  }
}
#elif defined(XP_WIN)
#include <windows.h>
#if _MSC_VER > 1200
#include <psapi.h>
#else
typedef struct _PROCESS_MEMORY_COUNTERS {
  DWORD cb;
  DWORD PageFaultCount;
  SIZE_T PeakWorkingSetSize;
  SIZE_T WorkingSetSize;
  SIZE_T QuotaPeakPagedPoolUsage;
  SIZE_T QuotaPagedPoolUsage;
  SIZE_T QuotaPeakNonPagedPoolUsage;
  SIZE_T QuotaNonPagedPoolUsage;
  SIZE_T PagefileUsage;
  SIZE_T PeakPagefileUsage;
} PROCESS_MEMORY_COUNTERS;
typedef PROCESS_MEMORY_COUNTERS *PPROCESS_MEMORY_COUNTERS;
#endif
typedef BOOL (WINAPI * GETPROCESSMEMORYINFO_FUNC)(
    HANDLE process, PPROCESS_MEMORY_COUNTERS counters, DWORD cb);
static HMODULE sPSModule;
static HANDLE sProcess;
static GETPROCESSMEMORYINFO_FUNC sGetMemInfo;
static void GetMemUsage_Shutdown() {
  if (sProcess) {
    CloseHandle(sProcess);
    sProcess = NULL;
  }
  if (sPSModule) {
    FreeLibrary(sPSModule);
    sPSModule = NULL;
  }
  sGetMemInfo = NULL;
}
#elif defined(XP_MACOSX)
#include <mach/mach.h>
#include <mach/task.h>
static void GetMemUsage_Shutdown() {
}
#endif

struct MemUsage {
  PRInt64 total;
  PRInt64 resident;
};

// This method should be incorporated into NSPR
static PRBool GetMemUsage(MemUsage *result)
{
  PRBool setResult = PR_FALSE;
#if defined(__linux)
  // Read /proc/<pid>/statm, and look at the first and second fields, which
  // report the program size and the number of resident pages for this process,
  // respectively.
 
  char buf[256];
  if (!sProcFP) {
    pid_t pid = getpid();
    snprintf(buf, sizeof(buf), "/proc/%d/statm", pid);
    sProcFP = fopen(buf, "rb");
  }
  if (sProcFP) {
    int vmsize, vmrss;

    int count = fscanf(sProcFP, "%d %d", &vmsize, &vmrss);
    rewind(sProcFP);

    if (count == 2) {
      static int ps = getpagesize();
      result->total = PRInt64(vmsize) * ps;
      result->resident = PRInt64(vmrss) * ps;
      setResult = PR_TRUE;
    }
  }
#elif defined(XP_WIN)
  // Use GetProcessMemoryInfo, which only works on WinNT and later.

  if (!sGetMemInfo) {
    sPSModule = LoadLibrary("psapi.dll");
    if (sPSModule) {
      sGetMemInfo = (GETPROCESSMEMORYINFO_FUNC)
          GetProcAddress(sPSModule, "GetProcessMemoryInfo");
      if (sGetMemInfo)
        sProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                               FALSE, GetCurrentProcessId());
      // Don't leave ourselves partially initialized.
      if (!sProcess)
        GetMemUsage_Shutdown();
    }
  }
  if (sGetMemInfo) {
    PROCESS_MEMORY_COUNTERS pmc;
    if (sGetMemInfo(sProcess, &pmc, sizeof(pmc))) {
      result->total = PRInt64(pmc.PagefileUsage);
      result->resident = PRInt64(pmc.WorkingSetSize);
      setResult = PR_TRUE;
    }
  }
#elif defined(XP_MACOSX)
  // Use task_info

  task_basic_info_data_t ti;
  mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
  kern_return_t error = task_info(mach_task_self(), TASK_BASIC_INFO,
                                  (task_info_t) &ti, &count);
  if (error == KERN_SUCCESS) {
    result->total = PRInt64(ti.virtual_size);
    result->resident = PRInt64(ti.resident_size);
    setResult = PR_TRUE;
  }
#endif
  return setResult;
}

//-----------------------------------------------------------------------------

nsLoadCollector::nsLoadCollector()
    : mNextDocID(0)
{
  mDocumentMap.Init(16);
}

nsLoadCollector::~nsLoadCollector()
{
  GetMemUsage_Shutdown();
}

NS_IMPL_ISUPPORTS5(nsLoadCollector, nsIMetricsCollector,
                   nsIWebProgressListener, nsISupportsWeakReference,
                   nsIDocumentObserver, nsIMutationObserver)

NS_IMETHODIMP
nsLoadCollector::OnStateChange(nsIWebProgress *webProgress,
                               nsIRequest *request,
                               PRUint32 flags,
                               nsresult status)
{
  NS_ASSERTION(flags & STATE_IS_DOCUMENT,
               "incorrect state change notification");

#ifdef PR_LOGGING
  if (MS_LOG_ENABLED()) {
    nsCString name;
    request->GetName(name);

    MS_LOG(("LoadCollector: progress = %p, request = %p [%s], flags = %x, status = %x",
            webProgress, request, name.get(), flags, status));
  }
#endif

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  if (!channel) {
    // We don't care about non-channel requests
    return NS_OK;
  }

  nsresult rv;
  if (flags & STATE_START) {
    RequestEntry entry;
    NS_ASSERTION(!mRequestMap.Get(request, &entry), "duplicate STATE_START");

    nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(webProgress);
    nsCOMPtr<nsIDOMWindow> window = do_GetInterface(docShell);
    if (!window) {
      // We don't really care about windowless loads
      return NS_OK;
    }

    rv = nsMetricsUtils::NewPropertyBag(getter_AddRefs(entry.properties));
    NS_ENSURE_SUCCESS(rv, rv);
    nsIWritablePropertyBag2 *props = entry.properties;

    rv = props->SetPropertyAsUint32(NS_LITERAL_STRING("window"),
                                    nsMetricsService::GetWindowID(window));
    NS_ENSURE_SUCCESS(rv, rv);

    if (flags & STATE_RESTORING) {
      rv = props->SetPropertyAsBool(NS_LITERAL_STRING("bfCacheHit"), PR_TRUE);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsString origin;
    PRUint32 loadType;
    docShell->GetLoadType(&loadType);

    switch (loadType) {
    case LOAD_NORMAL:
    case LOAD_NORMAL_REPLACE:
    case LOAD_BYPASS_HISTORY:
      origin = NS_LITERAL_STRING("typed");
      break;
    case LOAD_NORMAL_EXTERNAL:
      origin = NS_LITERAL_STRING("external");
      break;
    case LOAD_HISTORY:
      origin = NS_LITERAL_STRING("session-history");
      break;
    case LOAD_RELOAD_NORMAL:
    case LOAD_RELOAD_BYPASS_CACHE:
    case LOAD_RELOAD_BYPASS_PROXY:
    case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
    case LOAD_RELOAD_CHARSET_CHANGE:
      origin = NS_LITERAL_STRING("reload");
      break;
    case LOAD_LINK:
      origin = NS_LITERAL_STRING("link");
      break;
    case LOAD_REFRESH:
      origin = NS_LITERAL_STRING("refresh");
      break;
    default:
      break;
    }
    if (!origin.IsEmpty()) {
      rv = props->SetPropertyAsAString(NS_LITERAL_STRING("origin"), origin);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    entry.startTime = PR_Now();
    NS_ENSURE_TRUE(mRequestMap.Put(request, entry), NS_ERROR_OUT_OF_MEMORY);
  } else if (flags & STATE_STOP) {
    RequestEntry entry;
    if (mRequestMap.Get(request, &entry)) {
      mRequestMap.Remove(request);

      // Log a <document action="load"> event
      nsIWritablePropertyBag2 *props = entry.properties;
      rv = props->SetPropertyAsACString(NS_LITERAL_STRING("action"),
                                        NS_LITERAL_CSTRING("load"));
      NS_ENSURE_SUCCESS(rv, rv);
      
      // Compute the load time now that we have the end time.
      PRInt64 loadTime = (PR_Now() - entry.startTime) / PR_USEC_PER_MSEC;
      rv = props->SetPropertyAsUint64(NS_LITERAL_STRING("loadtime"), loadTime);
      NS_ENSURE_SUCCESS(rv, rv);

      MemUsage mu;
      if (GetMemUsage(&mu)) {
        rv = props->SetPropertyAsUint64(NS_LITERAL_STRING("memtotal"), mu.total);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = props->SetPropertyAsUint64(NS_LITERAL_STRING("memresident"), mu.resident);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Look up the document id, or assign a new one
      nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(webProgress);
      nsCOMPtr<nsIDOMWindow> window = do_GetInterface(docShell);
      if (!window) {
        MS_LOG(("Couldn't get window"));
        return NS_ERROR_UNEXPECTED;
      }

      nsCOMPtr<nsIDOMDocument> domDoc;
      window->GetDocument(getter_AddRefs(domDoc));
      nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
      if (!doc) {
        MS_LOG(("Couldn't get document"));
        return NS_ERROR_UNEXPECTED;
      }

      // If this was a load of a chrome document, hash the URL of the document
      // so it can be identified.

      nsMetricsService *ms = nsMetricsService::get();
      DocumentEntry docEntry;
      if (!mDocumentMap.Get(doc, &docEntry)) {
        docEntry.docID = mNextDocID++;

        if (!ms->WindowMap().Get(window, &docEntry.windowID)) {
          MS_LOG(("Window not in the window map"));
          return NS_ERROR_UNEXPECTED;
        }

        NS_ENSURE_TRUE(mDocumentMap.Put(doc, docEntry),
                       NS_ERROR_OUT_OF_MEMORY);
      }
      doc->AddObserver(this);  // set up to log the document destroy

      rv = props->SetPropertyAsUint32(NS_LITERAL_STRING("docid"),
                                      docEntry.docID);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
      if (channel) {
        nsCOMPtr<nsIURI> uri;
        channel->GetURI(getter_AddRefs(uri));
        if (uri) {
          PRBool isChrome = PR_FALSE;
          uri->SchemeIs("chrome", &isChrome);
          if (isChrome) {
            nsCString spec;
            uri->GetSpec(spec);

            nsCString hashedSpec;
            rv = ms->HashUTF8(spec, hashedSpec);
            NS_ENSURE_SUCCESS(rv, rv);

            rv = props->SetPropertyAsACString(NS_LITERAL_STRING("urlhash"),
                                              hashedSpec);
            NS_ENSURE_SUCCESS(rv, rv);
          }
        }
      }

      rv = ms->LogEvent(NS_LITERAL_STRING("document"), props);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      NS_WARNING("STATE_STOP without STATE_START");
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLoadCollector::OnProgressChange(nsIWebProgress *webProgress,
                                  nsIRequest *request,
                                  PRInt32 curSelfProgress,
                                  PRInt32 maxSelfProgress,
                                  PRInt32 curTotalProgress,
                                  PRInt32 maxTotalProgress)
{
  return NS_OK;
}

NS_IMETHODIMP
nsLoadCollector::OnLocationChange(nsIWebProgress *webProgress,
                                  nsIRequest *request, nsIURI *location)
{
  return NS_OK;
}

NS_IMETHODIMP
nsLoadCollector::OnStatusChange(nsIWebProgress *webProgress,
                                nsIRequest *request,
                                nsresult status, const PRUnichar *messaage)
{
  return NS_OK;
}

NS_IMETHODIMP
nsLoadCollector::OnSecurityChange(nsIWebProgress *webProgress,
                                  nsIRequest *request, PRUint32 state)
{
  return NS_OK;
}

nsresult
nsLoadCollector::Init()
{
  NS_ENSURE_TRUE(mRequestMap.Init(32), NS_ERROR_OUT_OF_MEMORY);
  return NS_OK;
}

NS_IMETHODIMP
nsLoadCollector::OnAttach()
{
  // Attach the LoadCollector as a global web progress listener
  nsCOMPtr<nsIWebProgress> progress =
    do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID);
  NS_ENSURE_STATE(progress);
  
  nsresult rv = progress->AddProgressListener(
      this, nsIWebProgress::NOTIFY_STATE_DOCUMENT);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

/* static */ PLDHashOperator PR_CALLBACK
nsLoadCollector::RemoveDocumentFromMap(const nsIDocument *document,
                                       DocumentEntry &entry, void *userData)
{
  nsIDocument *mutable_doc = NS_CONST_CAST(nsIDocument*, document);
  mutable_doc->RemoveObserver(NS_STATIC_CAST(nsLoadCollector*, userData));
  return PL_DHASH_REMOVE;
}

NS_IMETHODIMP
nsLoadCollector::OnDetach()
{
  // Clear the request and document maps so we start fresh
  // next time we're attached
  mRequestMap.Clear();
  mDocumentMap.Enumerate(RemoveDocumentFromMap, this);

  // Remove the progress listener
  nsCOMPtr<nsIWebProgress> progress =
    do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID);
  NS_ENSURE_STATE(progress);
  
  nsresult rv = progress->RemoveProgressListener(this);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsLoadCollector::OnNewLog()
{
  return NS_OK;
}

NS_IMPL_NSIDOCUMENTOBSERVER_LOAD_STUB(nsLoadCollector)
#ifdef MOZILLA_1_8_BRANCH
NS_IMPL_NSIDOCUMENTOBSERVER_REFLOW_STUB(nsLoadCollector)
#endif
NS_IMPL_NSIDOCUMENTOBSERVER_STATE_STUB(nsLoadCollector)
NS_IMPL_NSIDOCUMENTOBSERVER_CONTENT(nsLoadCollector)
NS_IMPL_NSIDOCUMENTOBSERVER_STYLE_STUB(nsLoadCollector)

void
nsLoadCollector::BeginUpdate(nsIDocument *document, nsUpdateType updateType)
{
}

void
nsLoadCollector::EndUpdate(nsIDocument *document, nsUpdateType updateType)
{
}

void
nsLoadCollector::DocumentWillBeDestroyed(nsIDocument *document)
{
  // Look up the document to get its id.
  DocumentEntry entry;
  if (!mDocumentMap.Get(document, &entry)) {
    MS_LOG(("Document not in map!"));
    return;
  }

  mDocumentMap.Remove(document);

  nsCOMPtr<nsIWritablePropertyBag2> props;
  nsMetricsUtils::NewPropertyBag(getter_AddRefs(props));
  if (!props) {
    return;
  }

  props->SetPropertyAsACString(NS_LITERAL_STRING("action"),
                               NS_LITERAL_CSTRING("destroy"));
  props->SetPropertyAsUint32(NS_LITERAL_STRING("docid"), entry.docID);
  props->SetPropertyAsUint32(NS_LITERAL_STRING("window"), entry.windowID);

  MemUsage mu;
  if (GetMemUsage(&mu)) {
    props->SetPropertyAsUint64(NS_LITERAL_STRING("memtotal"), mu.total);
    props->SetPropertyAsUint64(NS_LITERAL_STRING("memresident"), mu.resident);
  }

  nsMetricsService *ms = nsMetricsService::get();
  if (ms) {
    ms->LogEvent(NS_LITERAL_STRING("document"), props);
#ifdef PR_LOGGING
    nsIURI *uri = document->GetDocumentURI();
    if (uri) {
      nsCString spec;
      uri->GetSpec(spec);
      MS_LOG(("LoadCollector: Logged document destroy for %s\n", spec.get()));
    }
#endif
  }
}
