/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsISupports.h"
#include "nsRegressionTester.h"

#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIWindowWatcher.h"
#include "nsVoidArray.h"
#include "prmem.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDOMWindowInternal.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIURI.h"
#include "nsIDOMHTMLDocument.h"
#include "nsISimpleEnumerator.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocShell.h"
#include "nsIFrameDebug.h"
#include "nsIFrame.h"
#include "nsStyleStruct.h"
#include "nsIFrameUtil.h"
#include "nsLayoutCID.h"
#include "nsNetUtil.h"
#include "nsILocalFile.h"
#include "nsIPrefService.h"
#include "nsIViewManager.h"
#include "nsIView.h"



static NS_DEFINE_CID(kFrameUtilCID, NS_FRAME_UTIL_CID);


nsRegressionTester::nsRegressionTester()
{
}

nsRegressionTester::~nsRegressionTester() 
{
}

NS_IMPL_ISUPPORTS1(nsRegressionTester, nsILayoutRegressionTester)

NS_IMETHODIMP
nsRegressionTester::DumpFrameModel(nsIDOMWindow *aWindowToDump, nsILocalFile *aDestFile, PRUint32 aFlagsMask, PRInt32 *aResult) 
{
  NS_ENSURE_ARG(aWindowToDump);
  NS_ENSURE_ARG_POINTER(aResult);

  nsresult    rv = NS_ERROR_NOT_AVAILABLE;
  PRUint32    busyFlags;
  PRBool      stillLoading;

  *aResult = DUMP_RESULT_ERROR;
  
  nsCOMPtr<nsIDocShell> docShell;
  rv = GetDocShellFromWindow(aWindowToDump, getter_AddRefs(docShell));
  if (NS_FAILED(rv)) return rv;

  // find out if the document is loaded
  docShell->GetBusyFlags(&busyFlags);
  stillLoading = busyFlags && (nsIDocShell::BUSY_FLAGS_BUSY | nsIDocShell::BUSY_FLAGS_PAGE_LOADING);
  if (stillLoading)
  {
    *aResult = DUMP_RESULT_LOADING;
    return NS_OK;
  }
  
  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));

  nsIFrame*       root;
  presShell->GetRootFrame(&root);

  nsIFrameDebug*  fdbg;
  rv = CallQueryInterface(root, &fdbg);
  if (NS_FAILED(rv)) return rv;

  PRBool  dumpStyle = (aFlagsMask & DUMP_FLAGS_MASK_DUMP_STYLE) != 0;

  FILE* fp = stdout;
  if (aDestFile)
  {
    rv = aDestFile->OpenANSIFileDesc("w", &fp);
    if (NS_FAILED(rv)) return rv;
  }
  
  nsCOMPtr<nsPresContext> presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));

  fdbg->DumpRegressionData(presContext, fp, 0, dumpStyle);
  if (fp != stdout)
    fclose(fp);

  *aResult = DUMP_RESULT_COMPLETED;
  return NS_OK;
}

NS_IMETHODIMP
nsRegressionTester::CompareFrameModels(nsILocalFile *aBaseFile, nsILocalFile *aVerFile, PRUint32 aFlags, PRInt32 *aResult) 
{
  NS_ENSURE_ARG(aBaseFile);
  NS_ENSURE_ARG(aVerFile);
  NS_ENSURE_ARG_POINTER(aResult);
  
  *aResult = NS_OK;
  
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
    PRInt32 outputLevel = (aFlags == COMPARE_FLAGS_VERBOSE) ? 0 : 1;
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
  nsCOMPtr<nsIScriptGlobalObject> scriptObj(do_QueryInterface(inWindow));
  if (!scriptObj) return NS_ERROR_FAILURE;

  *outShell = scriptObj->GetDocShell();
  NS_IF_ADDREF(*outShell);

  return NS_OK;
}
