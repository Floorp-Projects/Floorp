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
#include "nsDebugObject.h"

#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIFileSpec.h"
#include "nsIWindowWatcher.h"
#include "nsVoidArray.h"
#include "prmem.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMWindowInternal.h"
#include "nsIPresShell.h"
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
#include "nsIFile.h"

NS_IMPL_ISUPPORTS1(nsDebugObject, nsIDebugObject)

static NS_DEFINE_IID(kFrameUtilCID, NS_FRAME_UTIL_CID);
static NS_DEFINE_IID(kIFrameUtilIID, NS_IFRAME_UTIL_IID);

/** ---------------------------------------------------
 *  See documentation in nsDebugObject.h
 *	@update 5/16/02 dwc
 */
nsDebugObject::nsDebugObject() :
  mRuntimeTestIsOn(PR_FALSE),
  mPrintAsIs(PR_FALSE),
  mRuntimeTestId(nsIDebugObject::PRT_RUNTIME_NONE),
  mFileName(nsnull)
{
}

/** ---------------------------------------------------
 *  See documentation in nsDebugObject.h
 *	@update 5/16/02 dwc
 */
nsDebugObject::~nsDebugObject() 
{
  if (mFileName) {
    nsMemory::Free(mFileName);
  }
}

/** ---------------------------------------------------
 *  See documentation in nsDebugObject.h
 *	@update 5/16/02 dwc
 */
NS_IMETHODIMP
nsDebugObject::CreateDirectory( const PRUnichar *aFilePath, PRUint32 aFlags) 
{
  nsresult                rv,result = NS_ERROR_FAILURE;
  PRBool exists =         PR_TRUE;

  nsCString dirStr;
  dirStr.AssignWithConversion(aFilePath);
  nsCOMPtr<nsIFile> localFile;
  rv = NS_GetFileFromURLSpec(dirStr, getter_AddRefs(localFile));
  if ( NS_SUCCEEDED(rv) ) {
    rv = localFile->Exists(&exists);
    if (!exists){
      rv = localFile->Create(nsIFile::DIRECTORY_TYPE, 0600);
      if (NS_FAILED(rv)) {
        printf("Failed to create directory [%s]\n", NS_LossyConvertUCS2toASCII(nsAutoString(aFilePath)).get());
      }
    } else {
      printf("OK - Directory Exists [%s]\n", NS_LossyConvertUCS2toASCII(nsAutoString(aFilePath)).get());
    }
  } else {
    printf("Failed to init path for local file in CreateDirectory [%s]\n", NS_LossyConvertUCS2toASCII(nsAutoString(aFilePath)).get());
  }
    
  return result;
}

/** ---------------------------------------------------
 *  See documentation in nsDebugObject.h
 *	@update 5/16/02 dwc
 */
NS_IMETHODIMP
nsDebugObject::OutputTextToFile(PRBool aNewFile, const PRUnichar *aFilePath, const PRUnichar *aFileName, const PRUnichar *aOutputString) 
{
  nsresult      result = NS_ERROR_FAILURE;
  nsCAutoString outputPath;
  outputPath.AssignWithConversion(aFilePath);
  outputPath.AppendWithConversion(aFileName);
  char* filePath = ToNewCString(outputPath);
  FILE* fp;
  
  if ( aNewFile ) {
    fp = fopen(filePath, "wt");
  } else {
    fp = fopen(filePath, "at");
  }

  if ( fp ) {
    nsCAutoString outputString;
    outputString.AssignWithConversion(aOutputString);
    char* theOutput = ToNewCString(outputString);
    fprintf(fp,theOutput);
    fprintf(fp,"\n");
    fclose(fp);
    delete filePath;
  }
  return result;
}



/** ---------------------------------------------------
 *  See documentation in nsDebugObject.h
 *	@update 5/16/02 dwc
 */
NS_IMETHODIMP
nsDebugObject::DumpContent(nsISupports *aWindow, const PRUnichar *aFilePath, const PRUnichar *aFileName, PRUint32 aFlags) 
{
nsresult    result = NS_ERROR_NOT_AVAILABLE;
PRUint32    busyFlags;
PRBool      stillLoading;

  nsCOMPtr<nsIDOMWindowInternal> theInternWindow(do_QueryInterface(aWindow));
  if (theInternWindow) {
    nsCOMPtr<nsIPresShell> presShell;
    if (theInternWindow != nsnull) {
      nsIFrameDebug*  fdbg;
      nsIFrame*       root;
      nsIPresContext  *thePC;

      nsCOMPtr<nsIScriptGlobalObject> scriptObj(do_QueryInterface(theInternWindow));
      nsCOMPtr<nsIDocShell> docShell;
      scriptObj->GetDocShell(getter_AddRefs(docShell));

      // find out if the document is loaded
      docShell->GetBusyFlags(&busyFlags);
      stillLoading = busyFlags && (nsIDocShell::BUSY_FLAGS_BUSY | nsIDocShell::BUSY_FLAGS_PAGE_LOADING);

      if ( !stillLoading ) {
        docShell->GetPresShell(getter_AddRefs(presShell));
        presShell->GetRootFrame(&root);
        if (NS_SUCCEEDED(CallQueryInterface(root, &fdbg))) {
          // create the string for the output
          nsCAutoString outputPath;
          outputPath.AssignWithConversion(aFilePath);
          outputPath.AppendWithConversion(aFileName);
          char* filePath = ToNewCString(outputPath);
          PRBool  dumpStyle=PR_FALSE;

          if(aFlags){
            dumpStyle = PR_TRUE;
          }

          FILE* fp = fopen(filePath, "wt");

          if ( fp ) {
            presShell->GetPresContext(&thePC);
          
            fdbg->DumpRegressionData(thePC, fp, 0, dumpStyle);
            fclose(fp);
            delete filePath;
            result = NS_OK;    // the document is now loaded, and the frames are dumped.
          } else {

            result = NS_ERROR_FILE_INVALID_PATH;
          }
        }
      }
    }
  }

  return result;
}

/** ---------------------------------------------------
 *  See documentation in nsDebugObject.h
 *	@update 5/16/02 dwc
 */
NS_IMETHODIMP
nsDebugObject::CompareFrameModels(const PRUnichar *aBasePath, const PRUnichar *aVerifyPath,
            const PRUnichar *aBaseLineFileName, const PRUnichar *aVerifyFileName, PRUint32 aFlags) 
{
nsresult        result = NS_ERROR_FAILURE;
nsCAutoString   tempString;
nsCAutoString   verifyFile;
char*           baselineFilePath; 
char*           verifyFilePath;
FILE            *bp,*vp;


  tempString.AssignWithConversion(aBasePath);
  tempString.AppendWithConversion(aBaseLineFileName);
  baselineFilePath = ToNewCString(tempString);

  tempString.AssignWithConversion(aVerifyPath);
  tempString.AppendWithConversion(aVerifyFileName);
  verifyFilePath = ToNewCString(tempString);

  bp = fopen(baselineFilePath, "rt");
  if (bp) {
    vp = fopen(verifyFilePath, "rt");
    if (vp) {
      nsIFrameUtil* fu;
      nsresult rv = nsComponentManager::CreateInstance(kFrameUtilCID, nsnull,
                                          kIFrameUtilIID, (void **)&fu);
      if (NS_SUCCEEDED(rv)) {
        result = fu->CompareRegressionData(bp,vp,1);
      }
      fclose(vp);          
    }
    fclose(bp);
  }
  delete baselineFilePath;
  delete verifyFilePath;

  return result;
}

/* attribute boolean doRuntimeTests; */
NS_IMETHODIMP 
nsDebugObject::GetDoRuntimeTests(PRBool *aDoRuntimeTests)
{
  NS_ENSURE_ARG_POINTER(aDoRuntimeTests);
  *aDoRuntimeTests = mRuntimeTestIsOn;
  return NS_OK;
}
NS_IMETHODIMP 
nsDebugObject::SetDoRuntimeTests(PRBool aDoRuntimeTests)
{
  mRuntimeTestIsOn = aDoRuntimeTests;
  return NS_OK;
}

/* attribute short testId; */
NS_IMETHODIMP 
nsDebugObject::GetTestId(PRInt16 *aTestId)
{
  NS_ENSURE_ARG_POINTER(aTestId);
  *aTestId = mRuntimeTestId;
  return NS_OK;
}
NS_IMETHODIMP 
nsDebugObject::SetTestId(PRInt16 aTestId)
{
  mRuntimeTestId = aTestId;
  return NS_OK;
}

/* attribute boolean printAsIs; */
NS_IMETHODIMP 
nsDebugObject::GetPrintAsIs(PRBool *aPrintAsIs)
{
  NS_ENSURE_ARG_POINTER(aPrintAsIs);
  *aPrintAsIs = mPrintAsIs;
  return NS_OK;
}
NS_IMETHODIMP 
nsDebugObject::SetPrintAsIs(PRBool aPrintAsIs)
{
  mPrintAsIs = aPrintAsIs;
  return NS_OK;
}

/* attribute wstring printFileName; */
NS_IMETHODIMP nsDebugObject::GetPrintFileName(PRUnichar * *aPrintFileName)
{
  *aPrintFileName = nsCRT::strdup(mFileName);
  return NS_OK;
}
NS_IMETHODIMP nsDebugObject::SetPrintFileName(const PRUnichar * aPrintFileName)
{
  if (mFileName) {
    nsMemory::Free(mFileName);
  }
  mFileName = nsCRT::strdup(aPrintFileName);
  return NS_OK;
}


