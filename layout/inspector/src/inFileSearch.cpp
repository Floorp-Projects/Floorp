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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
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

#include "inFileSearch.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsReadableUtils.h"

///////////////////////////////////////////////////////////////////////////////

inFileSearch::inFileSearch()
  : mSearchLoop(nsnull),
    mBasePath(nsnull),
    mTextCriteria(nsnull),
    mFilenameCriteria(nsnull),
    mDirsSearched(0),
    mFilenameCriteriaCount(0),
    mResultCount(0),
    mIsActive(PR_FALSE),
    mHoldResults(PR_FALSE),
    mReturnRelativePaths(PR_FALSE),
    mSearchRecursive(PR_FALSE)
{
}

inFileSearch::~inFileSearch()
{
  delete mSearchLoop;
  delete mTextCriteria;
}

NS_IMPL_ISUPPORTS2(inFileSearch, inISearchProcess, inIFileSearch)

///////////////////////////////////////////////////////////////////////////////
// inISearchProcess

NS_IMETHODIMP 
inFileSearch::GetIsActive(PRBool *aIsActive)
{
  *aIsActive = mIsActive;
  return NS_OK;
}

NS_IMETHODIMP 
inFileSearch::GetResultCount(PRInt32 *aResultCount)
{
  *aResultCount = mResultCount;
  return NS_OK;
}

NS_IMETHODIMP 
inFileSearch::GetHoldResults(PRBool *aHoldResults)
{
  *aHoldResults = mHoldResults;
  return NS_OK;
}

NS_IMETHODIMP 
inFileSearch::SetHoldResults(PRBool aHoldResults)
{
  mHoldResults = aHoldResults;
  return NS_OK;
}

NS_IMETHODIMP 
inFileSearch::SearchSync()
{
/*  if (mSearchPath)
    SearchDirectory(mSearchPath, PR_TRUE);
  else {
    return NS_ERROR_FAILURE;
  }
*/
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
inFileSearch::SearchAsync(inISearchObserver *aObserver)
{
  mObserver = aObserver;
  mObserver->OnSearchStart(this);
  
  InitSearch();
  InitSubDirectoryStack();
  InitSearchLoop();
  
  if (mSearchPath) {
    // start off by searching the first directory
    SearchDirectory(mSearchPath, PR_FALSE);
    
    if (mSearchRecursive) {
      // start the loop to continue searching
      mIsActive = PR_TRUE;
      mSearchLoop->Start();
    } else {
      KillSearch(inISearchObserver::SUCCESS);
    }
  } else {
    mObserver->OnSearchError(this, NS_LITERAL_STRING("No search path has been provided"));
    KillSearch(inISearchObserver::ERROR);
  }

  return NS_OK;
}

NS_IMETHODIMP
inFileSearch::SearchStop()
{
  KillSearch(inISearchObserver::INTERRUPTED);
  return NS_OK;
}

NS_IMETHODIMP
inFileSearch::SearchStep(PRBool* _retval)
{
  nsCOMPtr<nsIFile> nextDir;
  PRBool more = GetNextSubDirectory(getter_AddRefs(nextDir));

  if (more) {
    SearchDirectory(nextDir, PR_FALSE);
  } else {
    KillSearch(inISearchObserver::SUCCESS);
    *_retval = PR_TRUE;
  }

  return NS_OK;
}

NS_IMETHODIMP 
inFileSearch::GetStringResultAt(PRInt32 aIndex, nsAString& _retval)
{
  nsCOMPtr<nsIFile> file;

  _retval.Truncate();

  if (mHoldResults) {
    if (aIndex < mResults.Count()) {
      file = mResults[aIndex];
    }
  } else if (aIndex == mResultCount-1 && mLastResult) {
    // get the path of the last result as an nsAutoString
    file = mLastResult;
  } 
  
  if (file) {
    mLastResult->GetPath(_retval);
    if (mReturnRelativePaths)
      MakePathRelative(_retval);
  } else {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP 
inFileSearch::GetIntResultAt(PRInt32 aIndex, PRInt32 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
inFileSearch::GetUIntResultAt(PRInt32 aIndex, PRUint32 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

///////////////////////////////////////////////////////////////////////////////
// inIFileSearch

NS_IMETHODIMP 
inFileSearch::GetBasePath(PRUnichar** aBasePath)
{
  if (mBasePath) {
    *aBasePath = ToNewUnicode(*mBasePath);
  } else {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP 
inFileSearch::SetBasePath(const PRUnichar* aBasePath)
{
  mBasePath = new nsAutoString();
  mBasePath->Assign(aBasePath);
  return NS_OK;
}

NS_IMETHODIMP 
inFileSearch::GetReturnRelativePaths(PRBool* aReturnRelativePaths)
{
  *aReturnRelativePaths = mReturnRelativePaths;
  return NS_OK;
}

NS_IMETHODIMP 
inFileSearch::SetReturnRelativePaths(PRBool aReturnRelativePaths)
{
  mReturnRelativePaths = aReturnRelativePaths;
  return NS_OK;
}

NS_IMETHODIMP 
inFileSearch::GetFilenameCriteria(PRUnichar** aFilenameCriteria)
{
  // TODO: reconstruct parsed filename criteria into string
  return NS_OK;
}

NS_IMETHODIMP 
inFileSearch::SetFilenameCriteria(const PRUnichar* aFilenameCriteria)
{
  // first pass: scan for commas so we know how long to make array
  PRUint32 idx = 0;
  PRUint32 commas = 0;
  const PRUnichar* c = aFilenameCriteria;
  while (*c) {
    if (*c == ',')
        ++commas;
    ++c;
  }
  
  mFilenameCriteria = new PRUnichar*[commas+1];
  mFilenameCriteriaCount = 0;

  // second pass: split up at commas and insert into array
  idx = 0;
  PRInt32 lastComma = -1;
  PRUnichar* buf = new PRUnichar[257];
  c = aFilenameCriteria;
  PRBool going = PR_TRUE;
  while (going) {
    if (*c == ',' || !*c) {
      buf[idx-lastComma-1] = 0;
      lastComma = idx;
      mFilenameCriteria[mFilenameCriteriaCount] = buf;
      ++mFilenameCriteriaCount;
      buf = new PRUnichar[257];
      if (!*c) going = PR_FALSE;
    } else {
      buf[idx-lastComma-1] = *c;
    }
    ++c;
    ++idx;
  }

  return NS_OK;
}

NS_IMETHODIMP 
inFileSearch::GetTextCriteria(PRUnichar** aTextCriteria)
{
  *aTextCriteria = ToNewUnicode(*mTextCriteria);
  return NS_OK;
}

NS_IMETHODIMP 
inFileSearch::SetTextCriteria(const PRUnichar* aTextCriteria)
{
  mTextCriteria = new nsAutoString();
  mTextCriteria->Assign(aTextCriteria);
  return NS_OK;
}

NS_IMETHODIMP 
inFileSearch::GetSearchPath(nsIFile** aSearchPath)
{
  *aSearchPath = mSearchPath;
  NS_IF_ADDREF(*aSearchPath);
  return NS_OK;
}

NS_IMETHODIMP 
inFileSearch::SetSearchPath(nsIFile* aSearchPath)
{
  mSearchPath = aSearchPath;
  return NS_OK;
}

NS_IMETHODIMP 
inFileSearch::GetSearchRecursive(PRBool* aSearchRecursive)
{
  *aSearchRecursive = mSearchRecursive;
  return NS_OK;
}

NS_IMETHODIMP 
inFileSearch::SetSearchRecursive(PRBool aSearchRecursive)
{
  mSearchRecursive = aSearchRecursive;
  return NS_OK;
}

NS_IMETHODIMP 
inFileSearch::GetDirectoriesSearched(PRUint32* aDirectoriesSearched)
{
  *aDirectoriesSearched = mDirsSearched;
  return NS_OK;
}

NS_IMETHODIMP 
inFileSearch::GetCurrentDirectory(nsIFile** aCurrentDirectory)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
inFileSearch::GetFileResultAt(PRInt32 aIndex, nsIFile** _retval)
{
  if (mHoldResults) {
    if (aIndex < mResults.Count()) {
      NS_IF_ADDREF(*_retval = mResults[aIndex]);
    }
  } else if (aIndex == mResultCount-1 && mLastResult) {
    *_retval = mLastResult;
    NS_IF_ADDREF(*_retval);
  } else {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP 
inFileSearch::GetDirectoryDepth(nsIFile* aDir, PRUint32* _retval)
{
  *_retval = 0;
  return CountDirectoryDepth(aDir, _retval);
}

NS_IMETHODIMP 
inFileSearch::GetSubDirectories(nsIFile* aDir, nsISupportsArray** _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


///////////////////////////////////////////////////////////////////////////////
// inFileSearch

nsresult
inFileSearch::InitSearch()
{
  mResults.Clear();
  
  mLastResult = nsnull;
  mResultCount = 0;
  mDirsSearched = 0;
  return NS_OK;
}

nsresult
inFileSearch::KillSearch(PRInt16 aResult)
{
  mIsActive = PR_TRUE;
  mObserver->OnSearchEnd(this, aResult);

  return NS_OK;
}

nsresult 
inFileSearch::SearchDirectory(nsIFile* aDir, PRBool aIsSync)
{
  ++mDirsSearched;

  // recurse through subdirectories
  nsISimpleEnumerator* entries;
  aDir->GetDirectoryEntries(&entries);

  if (!aIsSync) {
    // store this directory for next step in async search
    PushSubDirectoryOnStack(aDir);
  }
  
  PRBool hasMoreElements;
  PRBool isDirectory;
  nsCOMPtr<nsIFile> entry;

  entries->HasMoreElements(&hasMoreElements);
  while (hasMoreElements) {
    entries->GetNext(getter_AddRefs(entry));
    entries->HasMoreElements(&hasMoreElements);

    entry->IsDirectory(&isDirectory);
    if (isDirectory && aIsSync) {
      // this is a directory, so search it now (only if synchronous)
      if (aIsSync) 
        SearchDirectory(entry, aIsSync);
    } else {
      // this is a file, so see if it matches
      if (MatchFile(entry)) {
        PrepareResult(entry, aIsSync);
      }
    }
  }

  return NS_OK;
}

nsresult
inFileSearch::PrepareResult(nsIFile* aFile, PRBool aIsSync)
{
  if (aIsSync || mHoldResults) {
    mResults.AppendObject(aFile);
  }

  if (!aIsSync) {
    ++mResultCount;
    mLastResult = aFile;
    mObserver->OnSearchResult(this);
  } 

  return NS_OK;
}

nsresult
inFileSearch::InitSearchLoop()
{
  if (!mSearchLoop) {
    nsCOMPtr<inISearchProcess> process = do_QueryInterface(this);
    mSearchLoop = new inSearchLoop(process);
  }
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Subdirectory stack (for asynchronous searches)

nsresult 
inFileSearch::InitSubDirectoryStack()
{
  mDirStack.Clear();

  return NS_OK;
}

PRBool
inFileSearch::GetNextSubDirectory(nsIFile** aDir)
{
  // get the enumerator on top of the stack
  nsCOMPtr<nsISimpleEnumerator> nextDirs;
  while (PR_TRUE) {
    PRInt32 count = mDirStack.Count();
    // the stack is empty, so our search must be complete
    if (count == 0) return PR_FALSE;

    // get the next directory enumerator on the stack
    nextDirs = mDirStack[count-1];

    // get the next directory from the enumerator
    *aDir = GetNextDirectory(nextDirs).get();
  
    if (*aDir)  {
      // this enumerator is ready to rock, so let's move on
      return PR_TRUE;
    }

    // enumerator is done, so pop it off the stack
    mDirStack.RemoveObjectAt(count-1);
  } 

  
  return PR_TRUE;
}

nsresult 
inFileSearch::PushSubDirectoryOnStack(nsIFile* aDir)
{
  nsCOMPtr<nsISimpleEnumerator> entries;
  aDir->GetDirectoryEntries(getter_AddRefs(entries));
  mDirStack.AppendObject(entries);
  return NS_OK;
}

already_AddRefed<nsIFile>
inFileSearch::GetNextDirectory(nsISimpleEnumerator* aEnum)
{
  nsCOMPtr<nsIFile> file;
  nsCOMPtr<nsISupports> supports;
  PRBool isDir;
  PRBool hasMoreElements;

  while (PR_TRUE) {
    aEnum->HasMoreElements(&hasMoreElements);
    if (!hasMoreElements) 
      break;
    aEnum->GetNext(getter_AddRefs(supports));
    file = do_QueryInterface(supports);
    file->IsDirectory(&isDir);
    if (isDir)
      break;
  } 

  nsIFile* f = file.get();
  NS_IF_ADDREF(f);

  return isDir ? f : nsnull;
}

///////////////////////////////////////////////////////////////////////////////
// Pattern Matching

PRBool
inFileSearch::MatchFile(nsIFile* aFile)
{
  nsAutoString fileName;
  aFile->GetLeafName(fileName);

  PRUnichar* fileNameUnicode = ToNewUnicode(fileName);
  
  PRBool match;

  for (PRUint32 i = 0; i < mFilenameCriteriaCount; ++i) {
    match = MatchPattern(mFilenameCriteria[i], fileNameUnicode);
    if (match) return PR_TRUE;
  }

  // XXX are we leaking fileNameUnicode?
  return PR_FALSE;
}

PRBool
inFileSearch::MatchPattern(PRUnichar* aPattern, PRUnichar* aString)
{
  PRInt32 index = 0;
  PRBool matching = PR_TRUE;
  char wildcard = '*';
  
  PRUnichar* patternPtr = aPattern;
  PRUnichar* stringPtr = aString;

  while (matching && *patternPtr && *stringPtr) {
    if (*patternPtr == wildcard) {
      matching = AdvanceWildcard(&stringPtr, patternPtr+1);
    } else {
      matching = *patternPtr == *stringPtr;
      ++stringPtr;
    }
    if (!matching) return PR_FALSE;
    ++patternPtr;
    ++index;
  }

  return matching;
}

PRBool
inFileSearch::AdvanceWildcard(PRUnichar** aString, PRUnichar* aNextChar)
{
  PRUnichar* stringPtr = *aString;

  while (1) {
    if (*stringPtr == *aNextChar) {
      // we have found the next char after the wildcard, so return with success
      *aString = stringPtr;
      return PR_TRUE;
    } else if (*stringPtr == 0)
      return PR_FALSE;
    ++stringPtr;
  }
}

///////////////////////////////////////////////////////////////////////////////
// URL fixing

nsresult
inFileSearch::MakePathRelative(nsAString& aPath)
{

  // get an nsAutoString version of the search path
  nsAutoString searchPath;
  mSearchPath->GetPath(searchPath);

  nsAutoString result;
  PRUint32 len = searchPath.Length();
  if (Substring(aPath, 0, len) == searchPath) {
    result = Substring(aPath, len+1, aPath.Length() - len - 1);
    result.ReplaceChar('\\', '/');
  }
  aPath = result;

  return NS_OK;
}

nsresult
inFileSearch::CountDirectoryDepth(nsIFile* aDir, PRUint32* aDepth)
{
  ++(*aDepth);

  nsISimpleEnumerator* entries;
  aDir->GetDirectoryEntries(&entries);

  PRBool hasMoreElements;
  PRBool isDirectory;
  nsCOMPtr<nsIFile> entry;

  entries->HasMoreElements(&hasMoreElements);
  while (hasMoreElements) {
    entries->GetNext(getter_AddRefs(entry));
    entries->HasMoreElements(&hasMoreElements);

    entry->IsDirectory(&isDirectory);
    if (isDirectory) {
      CountDirectoryDepth(entry, aDepth);
    }
  }

  return NS_OK;
}
