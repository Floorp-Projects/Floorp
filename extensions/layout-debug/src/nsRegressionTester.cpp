/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
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
#include "nsIStyleSet.h"



static NS_DEFINE_CID(kFrameUtilCID, NS_FRAME_UTIL_CID);
static NS_DEFINE_CID(kLayoutDebuggerCID, NS_LAYOUT_DEBUGGER_CID);


nsRegressionTester::nsRegressionTester()
{
}

nsRegressionTester::~nsRegressionTester() 
{
}

NS_IMPL_ISUPPORTS1(nsRegressionTester, nsILayoutRegressionTester)

NS_IMETHODIMP
nsRegressionTester::OutputTextToFile(nsILocalFile *aFile, PRBool aTruncateFile, const char *aOutputString) 
{
  NS_ENSURE_ARG(aOutputString);

  FILE* fp = stdout;
  if (aFile)
  {
    const char* options = (aTruncateFile) ? "wt" : "at";
    nsresult rv = aFile->OpenANSIFileDesc(options, &fp);
    if (NS_FAILED(rv)) return rv;
  }

  fprintf(fp, aOutputString);
  fprintf(fp, "\n");
  if (fp != stdout)
    fclose(fp);
  return NS_OK;
}


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
  
  nsCOMPtr<nsIPresContext> presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));

  fdbg->DumpRegressionData(presContext, fp, 0, dumpStyle);
  if (fp != stdout)
    fclose(fp);

  *aResult = DUMP_RESULT_COMPLETED;
  return NS_OK;
}

/* void dumpContent (in nsIDOMWindow aWindow, in nsILocalFile aDestFile); */
NS_IMETHODIMP
nsRegressionTester::DumpContent(nsIDOMWindow *aWindow, nsILocalFile *aDestFile)
{
  NS_ENSURE_ARG(aWindow);

  nsCOMPtr<nsIDocShell> docShell;
  nsresult rv = GetDocShellFromWindow(aWindow, getter_AddRefs(docShell));
  if (NS_FAILED(rv)) return rv;

  FILE* fp = stdout;
  if (aDestFile)
  {
    rv = aDestFile->OpenANSIFileDesc("w", &fp);
    if (NS_FAILED(rv)) return rv;
  }
  
  DumpContentRecurse(docShell, fp);
  DumpMultipleWebShells(aWindow, fp);

  if (fp != stdout)
    fclose(fp);
  return NS_OK;
}

/* void dumpFrames (in nsIDOMWindow aWindow, in nsILocalFile aDestFile); */
NS_IMETHODIMP
nsRegressionTester::DumpFrames(nsIDOMWindow *aWindow, nsILocalFile *aDestFile)
{
  NS_ENSURE_ARG(aWindow);

  nsCOMPtr<nsIDocShell> docShell;
  nsresult rv = GetDocShellFromWindow(aWindow, getter_AddRefs(docShell));
  if (NS_FAILED(rv)) return rv;

  FILE* fp = stdout;
  if (aDestFile)
  {
    rv = aDestFile->OpenANSIFileDesc("w", &fp);
    if (NS_FAILED(rv)) return rv;
  }
  
  DumpFramesRecurse(docShell, fp);
  DumpMultipleWebShells(aWindow, fp);

  if (fp != stdout)
    fclose(fp);
  return NS_OK;
}

NS_IMETHODIMP
nsRegressionTester::DumpViews(nsIDOMWindow *aWindow, nsILocalFile *aDestFile)
{
  NS_ENSURE_ARG(aWindow);

  nsCOMPtr<nsIDocShell> docShell;
  nsresult rv = GetDocShellFromWindow(aWindow, getter_AddRefs(docShell));
  if (NS_FAILED(rv)) return rv;

  FILE* fp = stdout;
  if (aDestFile)
  {
    rv = aDestFile->OpenANSIFileDesc("w", &fp);
    if (NS_FAILED(rv)) return rv;
  }
  
  DumpViewsRecurse(docShell, fp);
  DumpMultipleWebShells(aWindow, fp);

  if (fp != stdout)
    fclose(fp);
  return NS_OK;
}

/* void dumpWebShells (in nsIDOMWindow aWindow, in nsILocalFile aDestFile); */
NS_IMETHODIMP
nsRegressionTester::DumpWebShells(nsIDOMWindow *aWindow, nsILocalFile *aDestFile)
{
  NS_ENSURE_ARG(aWindow);

  nsCOMPtr<nsIDocShell> docShell;
  nsresult rv = GetDocShellFromWindow(aWindow, getter_AddRefs(docShell));
  nsCOMPtr<nsIDocShellTreeItem> shellAsItem(do_QueryInterface(docShell));
  if (!shellAsItem) return NS_ERROR_FAILURE;
  
  FILE* outFile = stdout;
  if (aDestFile)
  {
    rv = aDestFile->OpenANSIFileDesc("w", &outFile);
    if (NS_FAILED(rv)) return rv;
  }
  
  DumpAWebShell(shellAsItem, outFile);
  if (outFile != stdout)
    fclose(outFile);

  return NS_OK;
}

NS_IMETHODIMP
nsRegressionTester::DumpStyleSheets(nsIDOMWindow *aWindow, nsILocalFile *aDestFile)
{
  NS_ENSURE_ARG(aWindow);

  nsCOMPtr<nsIPresShell> presShell;
  nsresult rv = GetPresShellFromWindow(aWindow, getter_AddRefs(presShell));
  if (NS_FAILED(rv)) return rv;

  FILE* outFile = stdout;
  if (aDestFile)
  {
    rv = aDestFile->OpenANSIFileDesc("w", &outFile);
    if (NS_FAILED(rv)) return rv;
  }
  
  nsCOMPtr<nsIStyleSet> styleSet;
  presShell->GetStyleSet(getter_AddRefs(styleSet));
  if (styleSet)
    styleSet->List(outFile);
  else
    fputs("null style set\n", outFile);

  if (outFile != stdout)
    fclose(outFile);
  return NS_OK;
}

NS_IMETHODIMP
nsRegressionTester::DumpStyleContexts(nsIDOMWindow *aWindow, nsILocalFile *aDestFile)
{
  NS_ENSURE_ARG(aWindow);

  nsCOMPtr<nsIPresShell> presShell;
  nsresult rv = GetPresShellFromWindow(aWindow, getter_AddRefs(presShell));
  if (NS_FAILED(rv)) return rv;

  FILE* outFile = stdout;
  if (aDestFile)
  {
    rv = aDestFile->OpenANSIFileDesc("w", &outFile);
    if (NS_FAILED(rv)) return rv;
  }
  
  nsCOMPtr<nsIStyleSet> styleSet;
  presShell->GetStyleSet(getter_AddRefs(styleSet));
  if (styleSet)
  {
    nsIFrame* root;
    presShell->GetRootFrame(&root);
    if (root)
      fputs("null root frame\n", outFile);
    else
      styleSet->ListContexts(root, outFile);
  }
  else
    fputs("null style set\n", outFile);

  if (outFile != stdout)
    fclose(outFile);
  return NS_OK;
}

/* void dumpReflowStats (in nsIDOMWindow aWindow, in nsILocalFile aDestFile); */
NS_IMETHODIMP
nsRegressionTester::DumpReflowStats(nsIDOMWindow *aWindow, nsILocalFile* /* aDestFile */)
{
  NS_ENSURE_ARG(aWindow);

  nsCOMPtr<nsIPresShell> presShell;
  nsresult rv = GetPresShellFromWindow(aWindow, getter_AddRefs(presShell));
  if (NS_FAILED(rv)) return rv;

#ifdef MOZ_REFLOW_PERF
  presShell->DumpReflows();
#else
  fprintf(stdout, "***********************************\n");
  fprintf(stdout, "Sorry, you haven't built with MOZ_REFLOW_PERF=1\n");
  fprintf(stdout, "***********************************\n");
#endif

  return  NS_OK;
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
  }
  fclose(verFile);          
  fclose(baseFile);

  *aResult = NS_FAILED(rv);
  return NS_OK;
}

NS_IMETHODIMP
nsRegressionTester::GetShowFrameBorders(PRBool *aShowFrameBorders)
{
  NS_ENSURE_ARG_POINTER(aShowFrameBorders);
  NS_ENSURE_SUCCESS(EnsureLayoutDebugger(), NS_ERROR_FAILURE);
  return mLayoutDebugger->GetShowFrameBorders(aShowFrameBorders);
}

NS_IMETHODIMP
nsRegressionTester::SetShowFrameBorders(PRBool aShowFrameBorders)
{
  NS_ENSURE_SUCCESS(EnsureLayoutDebugger(), NS_ERROR_FAILURE);
  nsresult rv = mLayoutDebugger->SetShowFrameBorders(aShowFrameBorders);
  RefreshAllWindows();
  return rv;
}

NS_IMETHODIMP
nsRegressionTester::GetShowEventTargetFrameBorder(PRBool *aShowEventTargetFrameBorder)
{
  NS_ENSURE_ARG_POINTER(aShowEventTargetFrameBorder);
  NS_ENSURE_SUCCESS(EnsureLayoutDebugger(), NS_ERROR_FAILURE);
  return mLayoutDebugger->GetShowEventTargetFrameBorder(aShowEventTargetFrameBorder);
}

NS_IMETHODIMP
nsRegressionTester::SetShowEventTargetFrameBorder(PRBool aShowEventTargetFrameBorder)
{
  NS_ENSURE_SUCCESS(EnsureLayoutDebugger(), NS_ERROR_FAILURE);
  nsresult rv = mLayoutDebugger->SetShowEventTargetFrameBorder(aShowEventTargetFrameBorder);
  RefreshAllWindows();
  return rv;
}

NS_IMETHODIMP
nsRegressionTester::SetShowReflowStats(nsIDOMWindow *aWindow, PRBool inShow)
{
  nsCOMPtr<nsIPresShell> presShell;
  nsresult rv = GetPresShellFromWindow(aWindow, getter_AddRefs(presShell));
  if (NS_FAILED(rv)) return rv;
  
#ifdef MOZ_REFLOW_PERF
  presShell->SetPaintFrameCount(inShow);
#else
  printf("***********************************************\n");
  printf("Sorry, you haven't built with MOZ_REFLOW_PERF=1\n");
  printf("***********************************************\n");
#endif

  return NS_OK;
}

nsresult
nsRegressionTester::EnsureLayoutDebugger()
{
  if (!mLayoutDebugger)
  {
    nsresult rv;
    mLayoutDebugger = do_CreateInstance(kLayoutDebuggerCID, &rv);
    if (NS_FAILED(rv))
      return rv;
  }
  
  return NS_OK;
}

nsresult
nsRegressionTester::RefreshAllWindows()
{
  nsresult rv;
  // hack. Toggle the underline links pref to get stuff to redisplay
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefBranch)
  {
    PRBool underlineLinksPref;
    rv = prefBranch->GetBoolPref("browser.underline_anchors", &underlineLinksPref);
    if (NS_SUCCEEDED(rv))
    {
      prefBranch->SetBoolPref("browser.underline_anchors", !underlineLinksPref);
      prefBranch->SetBoolPref("browser.underline_anchors", underlineLinksPref);
    }
  }
  return NS_OK;
}


nsresult
nsRegressionTester::GetDocShellFromWindow(nsIDOMWindow* inWindow, nsIDocShell** outShell)
{
  nsCOMPtr<nsIScriptGlobalObject> scriptObj(do_QueryInterface(inWindow));
  if (!scriptObj) return NS_ERROR_FAILURE;
  
  return scriptObj->GetDocShell(outShell);
}


nsresult
nsRegressionTester::GetPresShellFromWindow(nsIDOMWindow* inWindow, nsIPresShell** outShell)
{
  nsCOMPtr<nsIDocShell> docShell;
  GetDocShellFromWindow(inWindow, getter_AddRefs(docShell));
  if (!docShell) return NS_ERROR_FAILURE;

  nsresult rv =  docShell->GetPresShell(outShell);
  if (NS_FAILED(rv)) return rv;
  if (!*outShell) return NS_ERROR_FAILURE;
  
  return NS_OK;
}

#if 0
#pragma mark -
#endif

void 
nsRegressionTester::DumpMultipleWebShells(nsIDOMWindow* aWindow, FILE* aOut)
{
  nsCOMPtr<nsIDocShell> docShell;
  GetDocShellFromWindow(aWindow, getter_AddRefs(docShell));
  if (!docShell) return;

  PRInt32 count;
  nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(docShell));
  if (docShellAsNode) {
    docShellAsNode->GetChildCount(&count);
    if (count > 0) {
      nsCOMPtr<nsIDocShellTreeItem> docShellAsStupidItem(do_QueryInterface(docShell));
      fprintf(aOut, "webshells= \n");
      DumpAWebShell(docShellAsStupidItem, aOut);
    }
  }
}


void
nsRegressionTester::DumpAWebShell(nsIDocShellTreeItem* aShellItem, FILE* aOut, PRInt32 aIndent)
{
  nsXPIDLString name;
  nsAutoString str;
  nsCOMPtr<nsIDocShellTreeItem> parent;
  PRInt32 i;

  for (i = aIndent; --i >= 0; ) fprintf(aOut, "  ");

  fprintf(aOut, "%p '", aShellItem);
  aShellItem->GetName(getter_Copies(name));
  aShellItem->GetSameTypeParent(getter_AddRefs(parent));
  str.Assign(name);
  fputs(NS_LossyConvertUCS2toASCII(str).get(), aOut);
  fprintf(aOut, "' parent=%p <\n", parent.get());

  aIndent++;
  nsCOMPtr<nsIDocShellTreeNode> shellAsNode(do_QueryInterface(aShellItem));
  
  PRInt32 numChildren;
  shellAsNode->GetChildCount(&numChildren);
  for (i = 0; i < numChildren; i++) {
    nsCOMPtr<nsIDocShellTreeItem> child;
    shellAsNode->GetChildAt(i, getter_AddRefs(child));
    if (child) {
      DumpAWebShell(child, aOut, aIndent);
    }
  }
  aIndent--;
  for (i = aIndent; --i >= 0; ) fprintf(aOut, "  ");
  fputs(">\n", aOut);
}


void
nsRegressionTester::DumpContentRecurse(nsIDocShell* inDocShell, FILE* inDestFile)
{
  if (inDocShell)
  {
    fprintf(inDestFile, "docshell=%p \n", inDocShell);
    nsCOMPtr<nsIPresShell> presShell;
    inDocShell->GetPresShell(getter_AddRefs(presShell));
    if (presShell)
    {
      nsCOMPtr<nsIDocument> doc;
      presShell->GetDocument(getter_AddRefs(doc));
      if (doc) 
      {
        nsCOMPtr<nsIContent> root;
        doc->GetRootContent(getter_AddRefs(root));
        if (root)
          root->List(inDestFile);
      }
    }
    else
    {
      fputs("null pres shell\n", inDestFile);
    }

    // dump the frames of the sub documents
    nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(inDocShell));
    PRInt32 numChildren;
    docShellAsNode->GetChildCount(&numChildren);
    for (PRInt32 i = 0; i < numChildren; i++)
    {
      nsCOMPtr<nsIDocShellTreeItem> child;
      docShellAsNode->GetChildAt(i, getter_AddRefs(child));
      nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
      if (child)
        DumpContentRecurse(childAsShell, inDestFile);
    }
  }
}


void
nsRegressionTester::DumpFramesRecurse(nsIDocShell* aDocShell, FILE* inDestFile)
{
  if (aDocShell)
  {
    fprintf(inDestFile, "webshell=%p \n", aDocShell);
    
    nsCOMPtr<nsIPresShell> presShell;
    aDocShell->GetPresShell(getter_AddRefs(presShell));
    if (presShell)
    {
      nsIFrame* root;
      presShell->GetRootFrame(&root);
      if (root)
      {
        nsCOMPtr<nsIPresContext> presContext;
        presShell->GetPresContext(getter_AddRefs(presContext));

        nsIFrameDebug* fdbg;
        if (NS_SUCCEEDED(CallQueryInterface(root, &fdbg)))
          fdbg->List(presContext, inDestFile, 0);
      }
    }
    else
    {
      fputs("null pres shell\n", inDestFile);
    }

    // dump the frames of the sub documents
    nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(aDocShell));
    PRInt32 numChildren;
    docShellAsNode->GetChildCount(&numChildren);
    for (PRInt32 i = 0; i < numChildren; i++)
    {
      nsCOMPtr<nsIDocShellTreeItem> child;
      docShellAsNode->GetChildAt(i, getter_AddRefs(child));
      nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
      if (childAsShell)
        DumpFramesRecurse(childAsShell, inDestFile);
    }
  }
}



void
nsRegressionTester::DumpViewsRecurse(nsIDocShell* aDocShell, FILE* inDestFile)
{
  if (aDocShell)
  {
    fprintf(inDestFile, "webshell=%p \n", aDocShell);
    
    nsCOMPtr<nsIPresShell> presShell;
    aDocShell->GetPresShell(getter_AddRefs(presShell));
    if (presShell)
    {
      nsCOMPtr<nsIViewManager> vm;
      presShell->GetViewManager(getter_AddRefs(vm));
      if (vm)
      {
        nsIView* root;
        vm->GetRootView(root);
        if (root)
          root->List(inDestFile);
      }
    }
    else
    {
      fputs("null pres shell\n", inDestFile);
    }

    // dump the frames of the sub documents
    nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(aDocShell));
    PRInt32 numChildren;
    docShellAsNode->GetChildCount(&numChildren);
    for (PRInt32 i = 0; i < numChildren; i++)
    {
      nsCOMPtr<nsIDocShellTreeItem> child;
      docShellAsNode->GetChildAt(i, getter_AddRefs(child));
      nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
      if (childAsShell)
        DumpViewsRecurse(childAsShell, inDestFile);
    }
  }
}

