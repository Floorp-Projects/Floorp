/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "inSearchOrphanImages.h"

#include "dsinfo.h"
#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsILocalFile.h"
#include "nsIURI.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsICSSStyleSheet.h"
#include "nsICSSStyleRule.h"
#include "nsICSSDeclaration.h"
#include "nsCSSValue.h"
#include "nsITimer.h"
#include "nsIRDFService.h"
#include "nsIRDFResource.h"

///////////////////////////////////////////////////////////////////////////////

#define INS_RDF_SEARCH_ROOT    "inspector:searchResults"
#define INS_RDF_RESULTS        INS_NAMESPACE_URI "results"
#define INS_RDF_URL            INS_NAMESPACE_URI "url"

nsIRDFResource*    kINS_SearchRoot;
nsIRDFResource*    kINS_results;
nsIRDFResource*    kINS_url;

///////////////////////////////////////////////////////////////////////////////

inSearchOrphanImages::inSearchOrphanImages()
{
  NS_INIT_ISUPPORTS();

  nsresult rv;

  rv = nsServiceManager::GetService(kRDFServiceCID, NS_GET_IID(nsIRDFService), (nsISupports**) &mRDF);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to get RDF service");
  rv = nsServiceManager::GetService(kRDFContainerUtilsCID, NS_GET_IID(nsIRDFContainerUtils), (nsISupports**) &mRDFCU);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to get RDF container utils");

  mRDF->GetResource(INS_RDF_SEARCH_ROOT, &kINS_SearchRoot);
  mRDF->GetResource(INS_RDF_RESULTS, &kINS_results);
  mRDF->GetResource(INS_RDF_URL, &kINS_url);
}

inSearchOrphanImages::~inSearchOrphanImages()
{
}

NS_IMPL_ISUPPORTS2(inSearchOrphanImages, inISearchOrphanImages, inISearchProcess);


///////////////////////////////////////////////////////////////////////////////
// inISearchOrphanImages

NS_IMETHODIMP 
inSearchOrphanImages::GetSearchPath(PRUnichar** aSearchPath)
{
  *aSearchPath = mSearchPath.ToNewUnicode();
  return NS_OK;
}

NS_IMETHODIMP 
inSearchOrphanImages::SetSearchPath(const PRUnichar* aSearchPath)
{
  mSearchPath.Assign(aSearchPath);
  mSearchPath.Trim("\\/", PR_FALSE, PR_TRUE, PR_FALSE);
  mSearchPath.AppendWithConversion("\\");
  return NS_OK;
}

NS_IMETHODIMP 
inSearchOrphanImages::GetRemotePath(PRUnichar** aRemotePath)
{
  *aRemotePath = mRemotePath.ToNewUnicode();
  return NS_OK;
}

NS_IMETHODIMP 
inSearchOrphanImages::SetRemotePath(const PRUnichar* aRemotePath)
{
  mRemotePath.Assign(aRemotePath);
  return NS_OK;
}

NS_IMETHODIMP 
inSearchOrphanImages::GetIsSkin(PRBool* aIsSkin)
{
  *aIsSkin = mIsSkin;
  return NS_OK;
}

NS_IMETHODIMP 
inSearchOrphanImages::SetIsSkin(PRBool aIsSkin)
{
  mIsSkin = aIsSkin;
  return NS_OK;
}

NS_IMETHODIMP 
inSearchOrphanImages::GetDocument(nsIDOMDocument** aDocument)
{
  *aDocument = mDocument;
  NS_IF_ADDREF(*aDocument);
  return NS_OK;
}

NS_IMETHODIMP 
inSearchOrphanImages::SetDocument(nsIDOMDocument* aDocument)
{
  mDocument = aDocument;

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// inISearchProcess

nsresult
inSearchOrphanImages::GetUid(PRUnichar** aUid)
{
  nsAutoString uid;
  uid.AssignWithConversion("findOrphanImages");
  *aUid = uid.ToNewUnicode();
  return NS_OK;
}

NS_IMETHODIMP 
inSearchOrphanImages::GetDataSource(nsIRDFDataSource** aDataSource)
{
  *aDataSource = mDataSource;
  NS_IF_ADDREF(*aDataSource);
  return NS_OK;
}

NS_IMETHODIMP 
inSearchOrphanImages::GetProgressPercent(PRInt16* aProgressPercent)
{
  PRUint32 count;
  mDirectories->Count(&count);
  *aProgressPercent = (100 * (mCurrentStep+1)) / count;
  return NS_OK;
}

NS_IMETHODIMP 
inSearchOrphanImages::GetResultCount(PRUint32* aResultCount)
{
  *aResultCount = mResultCount;
  return NS_OK;
}

NS_IMETHODIMP 
inSearchOrphanImages::GetProgressText(PRUnichar** aProgressText)
{
  // XX return current directory being searched
  nsAutoString text;
  text.AssignWithConversion("searching");
  *aProgressText = text.ToNewUnicode();
  return NS_OK;
}

nsresult
inSearchOrphanImages::Start(inISearchObserver* aObserver)
{
  nsresult rv;

  mObserver = aObserver;
  mCurrentStep = 0;
  mResultCount = 0;
  
  rv = InitDataSource();
  if (NS_FAILED(rv)) {
    ReportError("Unable to create results datasource.");
    return NS_OK;
  }

  mObserver->OnSearchStart(this);

  rv = CacheAllDirectories();
  if (!NS_FAILED(rv))
    StartSearchTimer();
  
  PRUint32 count;
  mDirectories->Count(&count);

  return rv;
}

NS_IMETHODIMP 
inSearchOrphanImages::Stop()
{
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// inSearchOrphanImages

nsresult
inSearchOrphanImages::SearchStep(PRBool* aDone)
{
  PRUint32 count;
  mDirectories->Count(&count);

  if (mCurrentStep == 0) {
    BuildRemoteURLHash();
  } else if (mCurrentStep == count) {
    *aDone = PR_TRUE;
      mObserver->OnSearchEnd(this);
  } else {
    nsCOMPtr<nsIFile> file;
    mDirectories->GetElementAt(mCurrentStep-1, getter_AddRefs(file));
    SearchDirectory(file);
  }

  mCurrentStep++;

  return NS_OK;
}


nsresult 
inSearchOrphanImages::CacheAllDirectories()
{
  if (mSearchPath.Length() < 1) return NS_ERROR_FAILURE;

  mDirectories = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID);
  
  nsCOMPtr<nsILocalFile> file;
  NS_NewLocalFile(mSearchPath.ToNewCString(), PR_FALSE, getter_AddRefs(file));
  
  CacheDirectory(file);
  
  return NS_OK;
}

nsresult
inSearchOrphanImages::CacheDirectory(nsIFile* aDir)
{
  // store this direcotyr
  mDirectories->AppendElement(aDir);

  // recurse through subdirectories
  nsISimpleEnumerator* entries;
  aDir->GetDirectoryEntries(&entries);
  
  PRBool* hasMoreElements = new PRBool();
  PRBool* isDirectory = new PRBool();
  nsCOMPtr<nsIFile> entry;

  entries->HasMoreElements(hasMoreElements);
  while (*hasMoreElements) {
    entries->GetNext(getter_AddRefs(entry));
    entries->HasMoreElements(hasMoreElements);

    entry->IsDirectory(isDirectory);
    if (*isDirectory) {
      CacheDirectory(entry);
    }
  }

  return NS_OK;
}

nsresult
inSearchOrphanImages::SearchDirectory(nsIFile* aDir)
{
  nsISimpleEnumerator* entries;
  aDir->GetDirectoryEntries(&entries);
  
  PRBool* hasMoreElements = new PRBool();
  PRBool* isFile = new PRBool();
  nsCOMPtr<nsIFile> entry;

  entries->HasMoreElements(hasMoreElements);
  while (*hasMoreElements) {
    entries->GetNext(getter_AddRefs(entry));
    entries->HasMoreElements(hasMoreElements);

    entry->IsFile(isFile);
    if (*isFile) {
      nsAutoString ext = GetFileExtension(entry);
      if (ext.EqualsIgnoreCase("gif", -1) ||
          ext.EqualsIgnoreCase("png", -1) ||
          ext.EqualsIgnoreCase("jpg", -1)) {
        
        char* path;
        entry->GetPath(&path);

        nsAutoString localPath;
        localPath.AssignWithConversion(path);
        nsAutoString equalized = EqualizeLocalURL(&localPath);

        nsStringKey key(equalized);
        PRUint32 result = (PRUint32) mFileHash->Get(&key);
        if (!result) {
          //printf("ORPHAN: '%s'\n", url); 

          nsCOMPtr<nsIRDFResource> res;
          CreateResourceFromFile(entry, getter_AddRefs(res));

          mResultCount++;

          mObserver->OnSearchResult(this);
        }
      }
    }
    

  }

  return NS_OK;
}

nsAutoString
inSearchOrphanImages::GetFileExtension(nsIFile* aFile)
{
  nsAutoString result;

  char* fileName;
  aFile->GetLeafName(&fileName);
  nsAutoString name;
  name.AssignWithConversion(fileName);

  name.Right(result, 3);
  result.ToLowerCase();

  return result;
}

nsresult
inSearchOrphanImages::BuildRemoteURLHash()
{
  // XXX dunno if I have to addref here and release the old one.. Scripting habits be damned!
  mFileHash = new nsHashtable(3000, PR_TRUE);

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
  if (doc) {
    PRInt32 count = doc->GetNumberOfStyleSheets();
    for (PRInt32 i = 0; i < count; i++) {
      nsIStyleSheet* sheet = doc->GetStyleSheetAt(i);
      HashStyleSheet(sheet);
    }
  }

  return NS_OK;
}

nsresult
inSearchOrphanImages::HashStyleSheet(nsIStyleSheet* aStyleSheet)
{
  NS_IF_ADDREF(aStyleSheet);
  
  nsCOMPtr<nsICSSStyleSheet> cssSheet = do_QueryInterface(aStyleSheet);
  if (cssSheet) {
    // recurse downward through the stylesheet tree
    PRInt32 count;
    cssSheet->StyleSheetCount(count);
    for (PRInt32 i = 0; i < count; i++) {
      nsICSSStyleSheet* child;
      cssSheet->GetStyleSheetAt(i, child);
      HashStyleSheet(child);
    }

    cssSheet->StyleRuleCount(count);
    for (i = 0; i < count; i++) {
      nsICSSRule* rule;
      cssSheet->GetStyleRuleAt(i, rule);
      HashStyleRule(rule);
    }
  }

  NS_IF_RELEASE(aStyleSheet);
  return NS_OK;
}

nsresult
inSearchOrphanImages::HashStyleRule(nsIStyleRule* aStyleRule)
{
  NS_IF_ADDREF(aStyleRule);

  nsCOMPtr<nsICSSStyleRule> cssRule = do_QueryInterface(aStyleRule);
  if (cssRule) {
    nsCOMPtr<nsICSSDeclaration> aDec = cssRule->GetDeclaration();
    HashStyleValue(aDec, eCSSProperty_background_image);
    HashStyleValue(aDec, eCSSProperty_list_style_image);
  }

  
  NS_IF_RELEASE(aStyleRule);
  return NS_OK;
}

nsresult
inSearchOrphanImages::HashStyleValue(nsICSSDeclaration* aDec, nsCSSProperty aProp)
{
  nsCSSValue value;
  aDec->GetValue(aProp, value);

  if (value.GetUnit() == eCSSUnit_URL) {
    nsAutoString result;
    result = value.GetStringValue(result);
    EqualizeRemoteURL(&result);
    nsStringKey key (result);
    mFileHash->Put(&key, (void*)1);
  }

  return NS_OK;
}

nsresult
inSearchOrphanImages::EqualizeRemoteURL(nsAutoString* aURL)
{
  if (mIsSkin) {
    if (aURL->Find("chrome://", PR_FALSE, 0, 1) >= 0) {
      PRUint32 len = aURL->Length();
      char* result = new char[len-8];
      char* buffer = aURL->ToNewCString();
      PRUint32 i = 9;
      PRUint32 milestone = 0;
      PRUint32 s = 0;
      while (i < len) {
        if (buffer[i] == '/') {
          milestone += 1;
        } 
        if (milestone == 0 || milestone > 1) {
          result[i-9-s] = (buffer[i] == '/') ? '/' : buffer[i];
        } else {
          s++;
        }
        i++;
      }
      result[i-9-s] = 0;

      aURL->AssignWithConversion(result);
      //printf("hash: '%s'\n", result);
    }
  } else {
  }

  return NS_OK;
}

nsAutoString
inSearchOrphanImages::EqualizeLocalURL(nsAutoString* aURL)
{
  nsAutoString result;
  PRInt32 found = aURL->Find(mSearchPath, PR_FALSE, 0, 1);
  if (found == 0) {
    PRUint32 len = mSearchPath.Length();
    aURL->Mid(result, len, aURL->Length()-len);
    result.ReplaceChar('\\', '/');
    //printf("file: '%s'\n", result.ToNewCString());
  }

  return result;
}

nsresult
inSearchOrphanImages::ReportError(char* aMsg)
{
  nsAutoString msg;
  msg.AssignWithConversion(aMsg);
  mObserver->OnSearchError(this, msg.ToNewUnicode());

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// timer stuff

nsresult
inSearchOrphanImages::StartSearchTimer()
{
  nsresult rv;
  mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  mTimer->Init(inSearchOrphanImages::SearchTimerCallback, (void*)this, 0, NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_SLACK);

  return NS_OK;
}

nsresult
inSearchOrphanImages::StopSearchTimer()
{
  mTimer->Cancel();
  mTimer = nsnull;

  return NS_OK;
}

void 
inSearchOrphanImages::SearchTimerCallback(nsITimer *aTimer, void *aClosure)
{
  inSearchOrphanImages* search = (inSearchOrphanImages*) aClosure;

  PRBool done = PR_FALSE;
  search->SearchStep(&done);

  if (done) {
    search->StopSearchTimer();
  }
}

///////////////////////////////////////////////////////////////////////////////
// RDF stuff

nsresult
inSearchOrphanImages::InitDataSource()
{
  nsresult rv;
  mDataSource = do_CreateInstance("@mozilla.org/rdf/datasource;1?name=in-memory-datasource", &rv);
  
  nsCOMPtr<nsIRDFResource> res;
  mRDF->GetAnonymousResource(getter_AddRefs(res));

  mDataSource->Assert(kINS_SearchRoot, kINS_results, res, PR_TRUE);
  
  mRDFCU->MakeSeq(mDataSource, res, getter_AddRefs(mResultSeq));

  return rv;
}

nsresult 
inSearchOrphanImages::CreateResourceFromFile(nsIFile* aFile, nsIRDFResource** aRes)
{
  nsCOMPtr<nsIRDFResource> res;
  mRDF->GetAnonymousResource(getter_AddRefs(res));

  char* url;
  aFile->GetURL(&url);
  nsAutoString theURL;
  theURL.AssignWithConversion(url);
  
  nsCOMPtr<nsIRDFLiteral> literal;
  mRDF->GetLiteral(theURL.ToNewUnicode(), getter_AddRefs(literal));

  mDataSource->Assert(res, kINS_url, literal, PR_TRUE);

  mResultSeq->AppendElement(res);

  return NS_OK;
}


