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
#include "plugin.h"

static NS_DEFINE_IID(kIDebugPluginIID, NS_IDEBUGPLUGIN_IID);
static NS_DEFINE_IID(kIClassInfoIID, NS_ICLASSINFO_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsScriptablePeer::nsScriptablePeer(nsPluginInstance* aPlugin)
{
  NS_INIT_ISUPPORTS();
  mPlugin = aPlugin;
}

nsScriptablePeer::~nsScriptablePeer()
{
}

NS_IMPL_ADDREF(nsScriptablePeer)
NS_IMPL_RELEASE(nsScriptablePeer)

// here nsScriptablePeer should return three interfaces it can be asked for by their iid's
// static casts are necessary to ensure that correct pointer is returned
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

//
// the following method will be callable from JavaScript
//
NS_IMETHODIMP nsScriptablePeer::GetVersion(char * *aVersion)
{
  if (mPlugin)
    mPlugin->GetVersion(aVersion);
  return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::CreateDirectory(const PRUnichar *aFilePath,PRUint32 aFlags, PRInt32 *aResult)
{
nsresult  rv = NS_OK;
PRBool    retVal;

  if ( mPlugin ) {
    return mPlugin->CreateDirectory(aFilePath,aFlags,&retVal);
  }
  return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::DumpLayout(nsISupports *aWindow, const PRUnichar *aFilePath, const PRUnichar *aFileName, 
                                           PRUint32 aFlags, PRInt32 *aResult)
{
nsresult  rv = NS_OK;
PRBool    retVal;

  if (mPlugin){
    mPlugin->OutPutLayoutFrames(aWindow,aFilePath,aFileName,aFlags,&retVal);
    if (retVal == NS_OK) {
      *aResult= 0;
    } else if ( retVal == NS_ERROR_FILE_INVALID_PATH ) {
      *aResult = 2;     // fatal error.. stop exection
    } else {
      *aResult = 1;     // did not load.. keep going
    }
  }
  return rv;
}

NS_IMETHODIMP nsScriptablePeer::CompareLayoutFiles(const PRUnichar *aBasePath, const PRUnichar *aVerPath,
                  const PRUnichar *aBaseFile, const PRUnichar *aVerFile, PRUint32 aFlags, PRInt32 *aResult)
{
nsresult  rv = NS_OK;
PRBool    retVal;

  if (mPlugin){
    mPlugin->CompareLayoutFrames(aBasePath,aVerPath,aBaseFile,aVerFile,aFlags, &retVal);
    if (retVal == NS_OK) {
      *aResult= 0;
    } else {
      *aResult = 1;
    }
  }
  return rv;
}

//
// the following method will be callable from JavaScript
//
NS_IMETHODIMP nsScriptablePeer::StartDirectorySearch(const char *aFilePath)
{

  if (mPlugin)
    return mPlugin->StartDirectorySearch(aFilePath);

  return NS_OK;
}


NS_IMETHODIMP nsScriptablePeer::GetNextFileInDirectory(char **aFilePath)
{

  if (mPlugin)
    return mPlugin->GetNextFileInDirectory(aFilePath);

  return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::SetBoolPref(const PRUnichar *aPrefName, PRBool aVal)
{
  if (mPlugin)
    return mPlugin->SetBoolPref(aPrefName, aVal);
  return NS_ERROR_FAILURE;
}

/* attribute boolean doRuntimeTests; */
NS_IMETHODIMP 
nsScriptablePeer::GetDoRuntimeTests(PRBool *aDoRuntimeTests)
{
  if (mPlugin)
    return mPlugin->GetDoRuntimeTests(aDoRuntimeTests);
  return NS_ERROR_FAILURE;
}
NS_IMETHODIMP 
nsScriptablePeer::SetDoRuntimeTests(PRBool aDoRuntimeTests)
{
  if (mPlugin)
    return mPlugin->SetDoRuntimeTests(aDoRuntimeTests);
  return NS_ERROR_FAILURE;
}

/* attribute short testId; */
NS_IMETHODIMP 
nsScriptablePeer::GetTestId(PRInt16 *aTestId)
{
  if (mPlugin)
    return mPlugin->GetTestId(aTestId);
  return NS_ERROR_FAILURE;
}
NS_IMETHODIMP 
nsScriptablePeer::SetTestId(PRInt16 aTestId)
{
  if (mPlugin)
    return mPlugin->SetTestId(aTestId);
  return NS_ERROR_FAILURE;
}

/* attribute boolean printAsIs; */
NS_IMETHODIMP 
nsScriptablePeer::GetPrintAsIs(PRBool *aPrintAsIs)
{
  if (mPlugin)
    return mPlugin->GetPrintAsIs(aPrintAsIs);
  return NS_ERROR_FAILURE;
}
NS_IMETHODIMP 
nsScriptablePeer::SetPrintAsIs(PRBool aPrintAsIs)
{
  if (mPlugin)
    return mPlugin->SetPrintAsIs(aPrintAsIs);
  return NS_ERROR_FAILURE;
}

/* attribute wstring printFileName; */
NS_IMETHODIMP nsScriptablePeer::GetPrintFileName(PRUnichar * *aPrintFileName)
{
  if (mPlugin)
    return mPlugin->GetPrintFileName(aPrintFileName);
  return NS_ERROR_FAILURE;
}
NS_IMETHODIMP nsScriptablePeer::SetPrintFileName(const PRUnichar * aPrintFileName)
{
  if (mPlugin)
    return mPlugin->SetPrintFileName(aPrintFileName);
  return NS_ERROR_FAILURE;
}

/* void PluginShutdown (); */
NS_IMETHODIMP nsScriptablePeer::PluginShutdown()
{
  mPlugin = nsnull;
  return NS_OK;
}


