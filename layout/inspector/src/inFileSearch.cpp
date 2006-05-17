/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "inFileSearch.h"

#include "nsCOMPtr.h"
#include "nsString.h"

///////////////////////////////////////////////////////////////////////////////

inFileSearch::inFileSearch()
{
  NS_INIT_ISUPPORTS();

  mSearchLoop = 0;
}

inFileSearch::~inFileSearch()
{
  delete mSearchLoop;
  delete mTextCriteria;
}

NS_IMPL_ISUPPORTS2(inFileSearch, inISearchProcess, inIFileSearch);

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
    nsAutoString msg;
    msg.AssignWithConversion("No search path has been provided");
    mObserver->OnSearchError(this, msg.ToNewUnicode());
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
  nsIFile* nextDir;
  PRBool more = GetNextSubDirectory(&nextDir);

  if (more) {
    SearchDirectory(nextDir, PR_FALSE);
  } else {
    KillSearch(inISearchObserver::SUCCESS);
    *_retval = PR_TRUE;
  }

  return NS_OK;
}

NS_IMETHODIMP 
inFileSearch::GetStringResultAt(PRInt32 aIndex, PRUnichar **_retval)
{
  nsCOMPtr<nsIFile> file;

  if (mHoldResults) {
    nsCOMPtr<nsISupports> supports;
    mResults->GetElementAt(aIndex, getter_AddRefs(supports));
    file = do_QueryInterface(supports);
  } else if (aIndex == mResultCount-1 && mLastResult) {
    // get the path of the last result as an nsAutoString
    file = mLastResult;
  } 
  
  if (file) {
    char* temp;
    mLastResult->GetPath(&temp);
    nsAutoString path;
    path.AssignWithConversion(temp);
    if (mReturnRelativePaths)
      MakePathRelative(&path);
    *_retval = path.ToNewUnicode();
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
    *aBasePath = mBasePath->ToNewUnicode();
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
  *aTextCriteria = mTextCriteria->ToNewUnicode();
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
  if (mHoldResults && mResults) {
    nsCOMPtr<nsISupports> supports;
    mResults->GetElementAt(aIndex, getter_AddRefs(supports));
    nsCOMPtr<nsIFile> file = do_QueryInterface(supports);
    *_retval = file;
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
  if (mHoldResults) {
    mResults = do_CreateInstance("@mozilla.org/supports-array;1");
  } else {
    mResults = nsnull;
  }
  
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
    mResults->AppendElement(aFile);
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
  mDirStack = do_CreateInstance("@mozilla.org/supports-array;1");

  return NS_OK;
}

PRBool
inFileSearch::GetNextSubDirectory(nsIFile** aDir)
{
  // get the enumerator on top of the stack
  nsCOMPtr<nsISupports> supports;
  nsCOMPtr<nsISimpleEnumerator> nextDirs;
  PRUint32 count;

  while (PR_TRUE) {
    mDirStack->Count(&count);
    // the stack is empty, so our search must be complete
    if (count == 0) return PR_FALSE;

    // get the next directory enumerator on the stack
    mDirStack->GetElementAt(count-1, getter_AddRefs(supports));
    nextDirs = do_QueryInterface(supports);

    // get the next directory from the enumerator
    nsIFile* dir = GetNextDirectory(nextDirs);
  
    if (dir) {
      // this enumerator is ready to rock, so let's move on
      *aDir = dir;
      return PR_TRUE;
    } else {
      // enumerator is done, so pop it off the stack
      mDirStack->RemoveElement(supports);
    }
  } 

  
  return PR_TRUE;
}

nsresult 
inFileSearch::PushSubDirectoryOnStack(nsIFile* aDir)
{
  nsISimpleEnumerator* entries;
  aDir->GetDirectoryEntries(&entries);
  mDirStack->AppendElement(entries);
  return NS_OK;
}

nsIFile*
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
  char* fileName;
  aFile->GetLeafName(&fileName);
  
  nsAutoString temp;
  temp.AssignWithConversion(fileName);

  PRUnichar* fileNameUnicode = temp.ToNewUnicode();
  
  PRBool match;

  for (PRUint32 i = 0; i < mFilenameCriteriaCount; ++i) {
    match = MatchPattern(mFilenameCriteria[i], fileNameUnicode);
    if (match) return PR_TRUE;
  }

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
inFileSearch::MakePathRelative(nsAutoString* aPath)
{
  nsAutoString result;

  // get an nsAutoString version of the search path
  char* temp;
  mSearchPath->GetPath(&temp);
  nsAutoString searchPath;
  searchPath.AssignWithConversion(temp);

  PRInt32 found = aPath->Find(searchPath, PR_FALSE, 0, 1);
  if (found == 0) {
    PRUint32 len = searchPath.Length();
    aPath->Mid(result, len+1, aPath->Length()-len);
    result.ReplaceChar('\\', '/');
  }
  aPath->Assign(result);

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
