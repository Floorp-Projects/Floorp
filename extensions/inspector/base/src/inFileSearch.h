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

#ifndef __inFileSearch_h__
#define __inFileSearch_h__

#include "inIFileSearch.h"

#include "nsString.h"
#include "nsVoidArray.h"
#include "nsISupportsArray.h"
#include "inISearchObserver.h"
#include "nsIFile.h"
#include "inSearchLoop.h"

class inFileSearch : public inIFileSearch
{
public:
  inFileSearch();
  ~inFileSearch();

protected:
  // inISearchProcess related
  PRBool mIsActive;
  PRInt32 mResultCount;
  nsIFile* mLastResult;
  nsCOMPtr<nsISupportsArray> mResults;
  PRBool mHoldResults;
  nsAutoString* mBasePath;
  PRBool mReturnRelativePaths;
  nsCOMPtr<inISearchObserver> mObserver;

  // inIFileSearch related
  nsCOMPtr<nsIFile> mSearchPath;
  nsAutoString* mTextCriteria;
  PRUnichar** mFilenameCriteria;
  PRUint32 mFilenameCriteriaCount;
  PRBool mSearchRecursive;
  PRUint32 mDirsSearched;

  // asynchronous search related
  nsCOMPtr<nsISupportsArray> mDirStack;
  inSearchLoop* mSearchLoop;

  // life cycle of search
  nsresult InitSearch();
  nsresult KillSearch(PRInt16 aResult);
  nsresult SearchDirectory(nsIFile* aDir, PRBool aIsSync);
  nsresult PrepareResult(nsIFile* aFile, PRBool aIsSync);

  // asynchronous search helpers
  nsresult InitSearchLoop();
  nsresult InitSubDirectoryStack();
  PRBool GetNextSubDirectory(nsIFile** aDir);
  nsresult PushSubDirectoryOnStack(nsIFile* aDir);
  nsIFile* GetNextDirectory(nsISimpleEnumerator* aEnum);

  // pattern matching
  PRBool MatchFile(nsIFile* aFile);
  static PRBool MatchPattern(PRUnichar* aPattern, PRUnichar* aString);
  static PRBool AdvanceWildcard(PRUnichar** aString, PRUnichar* aNextChar);
  
  // misc
  nsresult MakePathRelative(nsAutoString* aPath);
  nsresult CountDirectoryDepth(nsIFile* aDir, PRUint32* aDepth);

public:
  NS_DECL_ISUPPORTS

  NS_DECL_INISEARCHPROCESS

  NS_DECL_INIFILESEARCH
};

//////////////////////////////////////////////////////////////

// {D5636476-9F94-47f2-9CE9-69CDD9D7BBCD}
#define IN_FILESEARCH_CID \
{ 0xd5636476, 0x9f94, 0x47f2, { 0x9c, 0xe9, 0x69, 0xcd, 0xd9, 0xd7, 0xbb, 0xcd } }

#define IN_FILESEARCH_CONTRACTID \
"@mozilla.org/inspector/search;1?type=file"

#endif
