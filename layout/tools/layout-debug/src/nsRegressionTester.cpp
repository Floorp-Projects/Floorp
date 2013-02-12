/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISupports.h"
#include "nsRegressionTester.h"

#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIWindowWatcher.h"
#include "nsVoidArray.h"
#include "nsPIDOMWindow.h"
#include "nsIPresShell.h"
#include "nsIURI.h"
#include "nsISimpleEnumerator.h"
#include "nsIDocShell.h"
#include "nsIContentViewer.h"
#include "nsIContentViewerFile.h"
#include "nsIFrame.h"
#include "nsStyleStruct.h"
#include "nsIFrameUtil.h"
#include "nsLayoutCID.h"
#include "nsNetUtil.h"
#include "nsIFile.h"
#include "nsViewManager.h"
#include "nsView.h"



static NS_DEFINE_CID(kFrameUtilCID, NS_FRAME_UTIL_CID);


nsRegressionTester::nsRegressionTester()
{
}

nsRegressionTester::~nsRegressionTester() 
{
}

NS_IMPL_ISUPPORTS1(nsRegressionTester, nsILayoutRegressionTester)

NS_IMETHODIMP
nsRegressionTester::DumpFrameModel(nsIDOMWindow *aWindowToDump,
                                   nsIFile *aDestFile,
                                   uint32_t aFlagsMask, int32_t *aResult)
{
  NS_ENSURE_ARG(aWindowToDump);
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = DUMP_RESULT_ERROR;

#ifndef DEBUG
  return NS_ERROR_NOT_AVAILABLE;
#else
  nsresult    rv = NS_ERROR_NOT_AVAILABLE;
  uint32_t    busyFlags;
  bool        stillLoading;

  nsCOMPtr<nsIDocShell> docShell;
  rv = GetDocShellFromWindow(aWindowToDump, getter_AddRefs(docShell));
  if (NS_FAILED(rv)) return rv;

  // find out if the document is loaded
  docShell->GetBusyFlags(&busyFlags);
  stillLoading = busyFlags & (nsIDocShell::BUSY_FLAGS_BUSY |
                              nsIDocShell::BUSY_FLAGS_PAGE_LOADING);
  if (stillLoading)
  {
    *aResult = DUMP_RESULT_LOADING;
    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();

  nsIFrame* root = presShell->GetRootFrame();

  FILE* fp = stdout;
  if (aDestFile)
  {
    rv = aDestFile->OpenANSIFileDesc("w", &fp);
    if (NS_FAILED(rv)) return rv;
  }
  if (aFlagsMask & DUMP_FLAGS_MASK_PRINT_MODE) {
    nsCOMPtr <nsIContentViewer> viewer;
    docShell->GetContentViewer(getter_AddRefs(viewer));
    if (viewer){
      nsCOMPtr<nsIContentViewerFile> viewerFile = do_QueryInterface(viewer);
      if (viewerFile) {
         viewerFile->Print(true, fp, nullptr);
      }
    }
  }
  else {
    root->DumpRegressionData(presShell->GetPresContext(), fp, 0);
  }
  if (fp != stdout)
    fclose(fp);
  *aResult = DUMP_RESULT_COMPLETED;
  return NS_OK;
#endif
}

NS_IMETHODIMP
nsRegressionTester::CompareFrameModels(nsIFile *aBaseFile, nsIFile *aVerFile,
                                       uint32_t aFlags, bool *aResult)
{
  NS_ENSURE_ARG(aBaseFile);
  NS_ENSURE_ARG(aVerFile);
  NS_ENSURE_ARG_POINTER(aResult);
  
  *aResult = false;
  
  nsresult rv;
  FILE* baseFile;
  rv = aBaseFile->OpenANSIFileDesc("r", &baseFile);
  if (NS_FAILED(rv)) return rv;

  FILE* verFile;
  rv = aVerFile->OpenANSIFileDesc("r", &verFile);
  if (NS_FAILED(rv)) {
    fclose(baseFile);
    return rv;
  }

  nsCOMPtr<nsIFrameUtil> frameUtil = do_CreateInstance(kFrameUtilCID, &rv);
  if (NS_SUCCEEDED(rv))
  {
    int32_t outputLevel = (aFlags == COMPARE_FLAGS_VERBOSE) ? 0 : 1;
    rv = frameUtil->CompareRegressionData(baseFile, verFile, outputLevel);
    // CompareRegressionData closes |baseFile| and |verFile|.
  } else {
    fclose(verFile);          
    fclose(baseFile);
  }

  *aResult = NS_FAILED(rv);
  return NS_OK;
}

nsresult
nsRegressionTester::GetDocShellFromWindow(nsIDOMWindow* inWindow, nsIDocShell** outShell)
{
  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(inWindow));
  if (!window) return NS_ERROR_FAILURE;

  *outShell = window->GetDocShell();
  NS_IF_ADDREF(*outShell);

  return NS_OK;
}
