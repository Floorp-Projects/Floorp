/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// ==============================
// ! Scriptability related code !
// ==============================

/////////////////////////////////////////////////////
//
// This file implements the nsScriptablePeer object
// The native methods of this class are supposed to
// be callable from JavaScript
//
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"

#include "plugin.h" 


static NS_DEFINE_IID(kIDebugPluginIID, NS_IDEBUGPLUGIN_IID);
static NS_DEFINE_IID(kIClassInfoIID, NS_ICLASSINFO_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsScriptablePeer::nsScriptablePeer(nsPluginInstance* aPlugin)
{
  NS_INIT_ISUPPORTS();
  mPlugin = aPlugin;

  mDebugObj = do_GetService("@mozilla.org/debug/debugobject;1");

}

/** ---------------------------------------------------
 *  See documentation in nsScriptablePeer.h
 *	@update 9/25/02 dwc
 */
nsScriptablePeer::~nsScriptablePeer()
{
}

NS_IMPL_ADDREF(nsScriptablePeer)
NS_IMPL_RELEASE(nsScriptablePeer)

/** ---------------------------------------------------
 *  See documentation in nsScriptablePeer.h
 *	@update 9/25/02 dwc
 *  here nsScriptablePeer should return three interfaces it can be asked for by their iid's
 *  static casts are necessary to ensure that correct pointer is returned
 */
NS_IMETHODIMP nsScriptablePeer::QueryInterface(const nsIID& aIID, void** aInstancePtr) 
{ 
  if(!aInstancePtr) 
    return NS_ERROR_NULL_POINTER; 

  if(aIID.Equals(kIDebugPluginIID)) {
    *aInstancePtr = static_cast<nsIDebugPlugin*>(this); 
    AddRef();
    return NS_OK;
  }

  if(aIID.Equals(kIClassInfoIID)) {
    *aInstancePtr = static_cast<nsIClassInfo*>(this); 
    AddRef();
    return NS_OK;
  }

  if(aIID.Equals(kISupportsIID)) {
    *aInstancePtr = static_cast<nsISupports*>(static_cast<nsIDebugPlugin*>(this)); 
    AddRef();
    return NS_OK;
  }

  return NS_NOINTERFACE; 
}

/** ---------------------------------------------------
 *  See documentation in nsScriptablePeer.h
 *	@update 9/25/02 dwc
 *  the following method will be callable from JavaScript
 */
NS_IMETHODIMP nsScriptablePeer::GetVersion(char * *aVersion)
{

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsScriptablePeer.h
 *	@update 9/25/02 dwc
 */
NS_IMETHODIMP 
nsScriptablePeer::CreateDirectory(const PRUnichar *aFilePath,PRUint32 aFlags, PRInt32 *aResult)
{
nsresult  rv = NS_OK;
PRBool    retVal;

  retVal = NS_ERROR_FAILURE;

  if(mDebugObj){
    retVal = mDebugObj->CreateDirectory(aFilePath, aFlags);
    printf("Tested CreateDirectory\n");
  }

  return NS_OK;

}

/** ---------------------------------------------------
 *  See documentation in nsScriptablePeer.h
 *	@update 9/25/02 dwc
 */
NS_IMETHODIMP 
nsScriptablePeer::DumpLayout(nsISupports *aWindow, const PRUnichar *aFilePath, const PRUnichar *aFileName, 
                                           PRUint32 aFlags, PRInt32 *aResult)
{
nsresult  rv = NS_OK;
PRBool    retVal;


  if(mDebugObj){
    retVal = mDebugObj->DumpContent(aWindow,aFilePath,aFileName,aFlags);
    if (retVal == NS_OK) {
      *aResult= 0;
    } else if ( retVal == NS_ERROR_FILE_INVALID_PATH ) {
      *aResult = 2;     // fatal error.. stop exection
    } else {
      *aResult = 1;     // did not load.. keep going
    }
    printf("Tested DumpLayout\n");
  }
  
  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsScriptablePeer.h
 *	@update 9/25/02 dwc
 */
NS_IMETHODIMP 
nsScriptablePeer::OutputTextToFile(PRBool aNewFile,const PRUnichar *aFilePath, const PRUnichar *aFileName, 
                                           const PRUnichar *aOutputString,PRInt32 *aResult)
{
nsresult  rv = NS_OK;
PRBool    retVal;


  if(mDebugObj){
    retVal = mDebugObj->OutputTextToFile(aNewFile,aFilePath,aFileName,aOutputString);
    if (retVal == NS_OK) {
      *aResult= 0;
    } else  {
      *aResult = 1;     // not really fatal...
    } 
  }
  
  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsScriptablePeer.h
 *	@update 9/25/02 dwc
 */
NS_IMETHODIMP 
nsScriptablePeer::CompareLayoutFiles(const PRUnichar *aBasePath, const PRUnichar *aVerPath,
                  const PRUnichar *aBaseFile, const PRUnichar *aVerFile, PRUint32 aFlags, PRInt32 *aResult)
{
nsresult  rv = NS_OK;
PRBool    retVal = NS_ERROR_FAILURE;

  if(mDebugObj){
    retVal = mDebugObj->CompareFrameModels(aBasePath,aVerPath,aBaseFile,aVerFile,aFlags);
     if (retVal == NS_OK) {
      *aResult= 0;
    } else {
      *aResult = 1;
    }
    printf("Tested CompareLayoutFiles\n");
  }

  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsScriptablePeer.h
 *	@update 9/25/02 dwc
 *  the following method will be callable from JavaScript
 */
NS_IMETHODIMP 
nsScriptablePeer::StartDirectorySearch(const char *aFilePath)
{

nsXPIDLCString  dirPath;
nsresult        rv;

  nsCString dirStr(aFilePath);
  nsCOMPtr<nsIFile> theDir;
  rv = NS_GetFileFromURLSpec(dirStr, getter_AddRefs(theDir));
  if (NS_FAILED(rv)) {
    printf("nsPluginInstance::StartDirectorySearch failed on creation of nsIFile [%s]\n", aFilePath);
    mIter = 0;
    return NS_OK;
  }

  rv = theDir->GetDirectoryEntries(getter_AddRefs(mIter));
  if (NS_FAILED(rv)){
    printf("nsPluginInstance::StartDirectorySearch failed on GetDirectoryEntries of nsIFile [%s]\n", aFilePath);
    mIter = 0;
    return NS_OK;
  }

  printf("Tested StartDirectory\n");

  return NS_OK;

}


/** ---------------------------------------------------
 *  See documentation in nsScriptablePeer.h
 *	@update 9/25/02 dwc
 */
NS_IMETHODIMP 
nsScriptablePeer::GetNextFileInDirectory(char **aFilePath)
{
PRBool                hasMore;
nsresult              rv;
nsCOMPtr<nsISupports> supports;
char*                 URLName;

  *aFilePath = 0;
  if ( 0 ==mIter ){
    return NS_OK;
  }

  while ( NS_SUCCEEDED(mIter->HasMoreElements(&hasMore)) ){
    rv = mIter->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv))
      break;
    nsCOMPtr<nsIFile> dirEntry(do_QueryInterface(supports, &rv));
    if (NS_FAILED(rv))
      break;
    nsXPIDLCString filePath;
    char* afilepath;
    nsAutoString path;


    rv = dirEntry->GetPath(path);
    if (NS_FAILED(rv))
      continue;

    afilepath =  ToNewCString(path);
 
    if( strstr(afilepath,".html") != 0 ) {
      *aFilePath = afilepath;
      nsCAutoString urlname;
      NS_GetURLSpecFromFile(dirEntry,urlname);
      URLName = ToNewCString(urlname);
      *aFilePath = URLName;
      break;
    } else {
      nsMemory::Free(afilepath);
    }
  }

  printf("Tested NextDirectory\n");

  return NS_OK;

}

/** ---------------------------------------------------
 *  See documentation in nsScriptablePeer.h
 *	@update 9/25/02 dwc
 */
NS_IMETHODIMP nsScriptablePeer::SetBoolPref(const PRUnichar *aPrefName, PRBool aVal)
{

  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefBranch) {
    nsCString prefName;
    prefName.AssignWithConversion(aPrefName);
    prefBranch->SetBoolPref(prefName.get(), aVal);
    return NS_OK;
  } else {
    return NS_ERROR_FAILURE;
  }
    
}

/** ---------------------------------------------------
 *  See documentation in nsScriptablePeer.h
 *	@update 9/25/02 dwc
 *  attribute boolean doRuntimeTests;
 */
NS_IMETHODIMP 
nsScriptablePeer::GetDoRuntimeTests(PRBool *aDoRuntimeTests)
{
  if (mDebugObj)
    return mDebugObj->GetDoRuntimeTests(aDoRuntimeTests);
  return NS_ERROR_FAILURE;
}


/** ---------------------------------------------------
 *  See documentation in nsScriptablePeer.h
 *	@update 9/25/02 dwc
 */
NS_IMETHODIMP 
nsScriptablePeer::SetDoRuntimeTests(PRBool aDoRuntimeTests)
{
  if (mDebugObj)
    return mDebugObj->SetDoRuntimeTests(aDoRuntimeTests);
  return NS_ERROR_FAILURE;
}

/** ---------------------------------------------------
 *  See documentation in nsScriptablePeer.h
 *	@update 9/25/02 dwc
 *  ttribute short testId;
 */
NS_IMETHODIMP 
nsScriptablePeer::GetTestId(PRInt16 *aTestId)
{
  if (mDebugObj)
    return mDebugObj->GetTestId(aTestId);
  return NS_ERROR_FAILURE;
}

/** ---------------------------------------------------
 *  See documentation in nsScriptablePeer.h
 *	@update 9/25/02 dwc
 */
NS_IMETHODIMP 
nsScriptablePeer::SetTestId(PRInt16 aTestId)
{
  if (mDebugObj)
    return mDebugObj->SetTestId(aTestId);
  return NS_ERROR_FAILURE;
}

/** ---------------------------------------------------
 *  See documentation in nsScriptablePeer.h
 *	@update 9/25/02 dwc
 *  attribute boolean printAsIs;
 */
NS_IMETHODIMP 
nsScriptablePeer::GetPrintAsIs(PRBool *aPrintAsIs)
{
  if (mDebugObj)
    return mDebugObj->GetPrintAsIs(aPrintAsIs);
  return NS_ERROR_FAILURE;
}

/** ---------------------------------------------------
 *  See documentation in nsScriptablePeer.h
 *	@update 9/25/02 dwc
 */
NS_IMETHODIMP 
nsScriptablePeer::SetPrintAsIs(PRBool aPrintAsIs)
{
  if (mDebugObj)
    return mDebugObj->SetPrintAsIs(aPrintAsIs);
  return NS_ERROR_FAILURE;
}


/** ---------------------------------------------------
 *  See documentation in nsScriptablePeer.h
 *	@update 9/25/02 dwc
 */
/* attribute wstring printFileName; */
NS_IMETHODIMP 
nsScriptablePeer::GetPrintFileName(PRUnichar * *aPrintFileName)
{
  if (mDebugObj)
    return mDebugObj->GetPrintFileName(aPrintFileName);
  return NS_ERROR_FAILURE;
}

/** ---------------------------------------------------
 *  See documentation in nsScriptablePeer.h
 *	@update 9/25/02 dwc
 */
NS_IMETHODIMP 
nsScriptablePeer::SetPrintFileName(const PRUnichar * aPrintFileName)
{
  if (mDebugObj)
    return mDebugObj->SetPrintFileName(aPrintFileName);
  return NS_ERROR_FAILURE;
}

/** ---------------------------------------------------
 *  See documentation in nsScriptablePeer.h
 *	@update 9/25/02 dwc
 */
NS_IMETHODIMP 
nsScriptablePeer::PluginShutdown()
{
  mPlugin = nsnull;
  return NS_OK;
}


