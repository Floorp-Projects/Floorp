/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 * Conrad Carlen <conrad@ingress.com>
 * Based on nsFindComponent.cpp by Pierre Phaneuf <pp@ludusdesign.com>
 * 
 */

#include "CFindComponent.h"

#include "nsITextServicesDocument.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIPresShell.h"
#include "nsTextServicesCID.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIDocShell.h"
#include "nsIWordBreaker.h"

static NS_DEFINE_CID(kCTextServicesDocumentCID, NS_TEXTSERVICESDOCUMENT_CID);

// ===========================================================================
//	CFindComponent
// ===========================================================================

CFindComponent::CFindComponent() :
  mLastCaseSensitive(FALSE), mLastSearchBackwards(FALSE),
  mLastWrapSearch(TRUE), mLastEntireWord(FALSE),
  mDocShell(NULL)
{
}


CFindComponent::~CFindComponent()
{
   SetContext(nsnull);
}


  // Call when the webshell content changes  
NS_IMETHODIMP
CFindComponent::SetContext(nsIDocShell* aDocShell)
{
  if (aDocShell != mDocShell)
  {
    mDocShell = aDocShell;
    mTextDoc = nsnull;
    mWordBreaker = nsnull;
  }
  return NS_OK;
}
  

  // Initiates a find - sets up the context    
NS_IMETHODIMP
CFindComponent::Find(const nsString& searchString,
                     PRBool caseSensitive,
                     PRBool searchBackwards,
                     PRBool wrapSearch,
                     PRBool entireWord,
                     PRBool& didFind)
{
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);

	nsAutoString		matchString(searchString);
	if (!caseSensitive)
		matchString.ToLowerCase();

  didFind = FALSE;
  nsresult	rv = NS_OK;

  
  if (!mTextDoc)
  {
    rv = CreateTSDocument(mDocShell, getter_AddRefs(mTextDoc));
    if (NS_FAILED(rv) || !mTextDoc)
      return rv;
  }
  
  // Set up the TSDoc. We are going to start searching thus:
  // 
  // Searching forwards:
  //        Look forward from the end of the selection
  // Searching backwards:
  //        Look backwards from the start of the selection
  //
  PRInt32  selOffset = 0;
  rv = SetupDocForSearch(mTextDoc, searchBackwards, &selOffset);
  if (NS_FAILED(rv))
    return rv;  
  
  // find out where we started
  PRInt32  blockIndex;
  rv = GetCurrentBlockIndex(mTextDoc, &blockIndex);
  if (NS_FAILED(rv))
    return rv;

  // remember where we started
  PRInt32  startingBlockIndex = blockIndex;
  
  // and set the starting position again (hopefully, in future we won't have to do this)
  rv = SetupDocForSearch(mTextDoc, searchBackwards, &selOffset);
  if (NS_FAILED(rv))
    return rv;  
  
  nsIWordBreaker *wordBreaker = entireWord ? mWordBreaker.get() : nsnull;  
  
  PRBool wrappedOnce = PR_FALSE;	// Remember whether we've already wrapped
	PRBool done = PR_FALSE;
	
  // Loop till we find a match or fail.
  while ( !done )
  {
      PRBool atExtremum = PR_FALSE;		// are we at the end (or start)

      while ( NS_SUCCEEDED(mTextDoc->IsDone(&atExtremum)) && !atExtremum )
      {
          nsString str;
          rv = mTextDoc->GetCurrentTextBlock(&str);
  
          if (NS_FAILED(rv))
              return rv;
  
          if (!caseSensitive)
              str.ToLowerCase();
          
          PRInt32 foundOffset = FindInString(str, matchString, selOffset, searchBackwards, wordBreaker);
          selOffset = -1;			// reset for next block
  
          if (foundOffset != -1)
          {
              // Match found.  Select it, remember where it was, and quit.
              mTextDoc->SetSelection(foundOffset, searchString.Length());
              mTextDoc->ScrollSelectionIntoView();
              done = PR_TRUE;
             	didFind = PR_TRUE;
              break;
          }
          else
          {
              // have we already been around once?
              if (wrappedOnce && (blockIndex == startingBlockIndex))
              {
                done = PR_TRUE;
                break;
              }

              // No match found in this block, try the next (or previous) one.
              if (searchBackwards) {
                  mTextDoc->PrevBlock();
                  blockIndex--;
              } else {
                  mTextDoc->NextBlock();
                  blockIndex++;
              }
          }
          
      } // while !atExtremum
      
      // At end (or matched).  Decide which it was...
      if (!done)
      {
          // Hit end without a match.  If we haven't passed this way already,
          // then reset to the first/last block (depending on search direction).
          if (!wrappedOnce)
          {
              // Reset now.
              wrappedOnce = PR_TRUE;
              // If not wrapping, give up.
              if ( !wrapSearch ) {
                  done = PR_TRUE;
              }
              else
              {
                  if ( searchBackwards ) {
                      // Reset to last block.
                      rv = mTextDoc->LastBlock();
                      // ugh
                      rv = GetCurrentBlockIndex(mTextDoc, &blockIndex);
                      rv = mTextDoc->LastBlock();
                  } else {
                      // Reset to first block.
                      rv = mTextDoc->FirstBlock();
                      blockIndex = 0;
                  }
              }
          } else
          {
              // already wrapped.  This means no matches were found.
              done = PR_TRUE;
          }
      }
  }
  
  // Save the last params
  mLastSearchString = searchString;
  mLastCaseSensitive = caseSensitive;
  mLastSearchBackwards = searchBackwards;
  mLastWrapSearch = wrapSearch;
  mLastEntireWord = entireWord;

	return NS_OK;
}


NS_IMETHODIMP
CFindComponent::CanFindNext(PRBool& canDo)
{
  canDo = (mTextDoc != nsnull);
  return NS_OK;
}


NS_IMETHODIMP
CFindComponent::GetLastSearchString(nsString& searchString)
{
  searchString = mLastSearchString;
  return NS_OK;
}

NS_IMETHODIMP
CFindComponent::GetLastCaseSensitive(PRBool& caseSensitive)
{
  caseSensitive = mLastCaseSensitive;
  return NS_OK;
}


NS_IMETHODIMP
CFindComponent::GetLastSearchBackwards(PRBool& searchBackwards)
{
  searchBackwards = mLastSearchBackwards;
  return NS_OK;
}

NS_IMETHODIMP
CFindComponent::GetLastWrapSearch(PRBool& wrapSearch)
{
  wrapSearch = mLastWrapSearch;
  return NS_OK;
}


NS_IMETHODIMP
CFindComponent::GetLastEntireWord(PRBool& entireWord)
{
  entireWord = mLastEntireWord;
  return NS_OK;
}


  // Finds the next using the current context and params  
NS_IMETHODIMP
CFindComponent::FindNext(PRBool& didFind)
{
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NULL_POINTER);

  Find(mLastSearchString, mLastCaseSensitive, mLastSearchBackwards, mLastWrapSearch, mLastEntireWord, didFind);
  
  return NS_OK;
}

  // Finds all occurrances from the top to bottom  
NS_IMETHODIMP
CFindComponent::FindAll(const nsString& searchString,
                        PRBool caseSensitive,
                        PRInt32& numFound)
{
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NULL_POINTER);

  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
CFindComponent::SetFindStyle(CFHighlightStyle style)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
CFindComponent::CreateTSDocument(nsIDocShell* aDocShell,
                                 nsITextServicesDocument** aDoc)
{
  if (!aDocShell)
    return NS_ERROR_INVALID_ARG;
    
  if (!aDoc)
    return NS_ERROR_NULL_POINTER;

  *aDoc = NULL;
  
  // Create the text services document.
  nsCOMPtr<nsITextServicesDocument>  tempDoc;
  nsresult rv = nsComponentManager::CreateInstance(kCTextServicesDocumentCID,
                                                   nsnull,
                                                   NS_GET_IID(nsITextServicesDocument),
                                                   getter_AddRefs(tempDoc));
  if (NS_FAILED(rv) || !tempDoc)
    return rv;

  // Get content viewer from the web shell.
  nsCOMPtr<nsIContentViewer> contentViewer;
  rv = aDocShell->GetContentViewer(getter_AddRefs(contentViewer));
  if (NS_FAILED(rv) || !contentViewer)
    return rv;

  // Up-cast to a document viewer.
  nsCOMPtr<nsIDocumentViewer> docViewer = do_QueryInterface(contentViewer, &rv);
  if (NS_FAILED(rv) || !docViewer)
    return rv;
    
  // Get the document and pres shell from the doc viewer.
  nsCOMPtr<nsIDocument>  document;
  nsCOMPtr<nsIPresShell> presShell;
  rv = docViewer->GetDocument(*getter_AddRefs(document));
  if (document)
      rv = docViewer->GetPresShell(*getter_AddRefs(presShell));

  if (NS_FAILED(rv) || !document || !presShell)
    return rv;
      
  // Get the word breaker used by the document
  rv = document->GetWordBreaker(getter_AddRefs(mWordBreaker));
  if (NS_FAILED(rv) || !mWordBreaker)
    return rv;

  // Upcast document to a DOM document.
  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(document, &rv);
  if (NS_FAILED(rv) || !domDoc)
    return rv;

  // Initialize the text services document.
  rv = tempDoc->InitWithDocument(domDoc, presShell);
  if (NS_FAILED(rv))
    return rv;

  // Return the resulting text services document.
  *aDoc = tempDoc;
  NS_IF_ADDREF(*aDoc);
  
  return rv;
}


// utility method to discover which block we're in. The TSDoc interface doesn't give
// us this, because it can't assume a read-only document.
NS_IMETHODIMP
CFindComponent::GetCurrentBlockIndex(nsITextServicesDocument *aDoc, PRInt32 *outBlockIndex)
{
  PRInt32  blockIndex = 0;
  PRBool   isDone = PR_FALSE;
  
  while (NS_SUCCEEDED(aDoc->IsDone(&isDone)) && !isDone)
  {
    aDoc->PrevBlock();
    blockIndex ++;
  }
  
  *outBlockIndex = blockIndex;
  return NS_OK;
}
    
NS_IMETHODIMP
CFindComponent::SetupDocForSearch(nsITextServicesDocument *aDoc, PRBool searchBackwards, PRInt32 *outBlockOffset)
{
  nsresult  rv;
  
  nsITextServicesDocument::TSDBlockSelectionStatus blockStatus;
  PRInt32 selOffset;
  PRInt32 selLength;
  
  if (!searchBackwards)	// searching forwards
  {
    rv = aDoc->LastSelectedBlock(&blockStatus, &selOffset, &selLength);
    if (NS_SUCCEEDED(rv) && (blockStatus != nsITextServicesDocument::eBlockNotFound))
    {
      switch (blockStatus)
      {
        case nsITextServicesDocument::eBlockOutside:		// No TB in S, but found one before/after S.
        case nsITextServicesDocument::eBlockPartial:		// S begins or ends in TB but extends outside of TB.
          // the TS doc points to the block we want.
          *outBlockOffset = selOffset + selLength;
          break;
                    
        case nsITextServicesDocument::eBlockInside:			// S extends beyond the start and end of TB.
          // we want the block after this one.
          rv = aDoc->NextBlock();
          *outBlockOffset = 0;
          break;
                
        case nsITextServicesDocument::eBlockContains:		// TB contains entire S.
          *outBlockOffset = selOffset + selLength;
          break;
        
        case nsITextServicesDocument::eBlockNotFound:		// There is no text block (TB) in or before the selection (S).
        default:
            NS_NOTREACHED("Shouldn't ever get this status");
      }
    
    }
    else		//failed to get last sel block. Just start at beginning
    {
      rv = aDoc->FirstBlock();
    }
  
  }
  else			// searching backwards
  {
    rv = aDoc->FirstSelectedBlock(&blockStatus, &selOffset, &selLength);
		if (NS_SUCCEEDED(rv) && (blockStatus != nsITextServicesDocument::eBlockNotFound))
		{
		  switch (blockStatus)
		  {
		    case nsITextServicesDocument::eBlockOutside:		// No TB in S, but found one before/after S.
		    case nsITextServicesDocument::eBlockPartial:		// S begins or ends in TB but extends outside of TB.
		      // the TS doc points to the block we want.
		      *outBlockOffset = selOffset;
		      break;
		                
		    case nsITextServicesDocument::eBlockInside:			// S extends beyond the start and end of TB.
		      // we want the block before this one.
		      rv = aDoc->PrevBlock();
		      *outBlockOffset = -1;
		      break;
		            
		    case nsITextServicesDocument::eBlockContains:		// TB contains entire S.
		      *outBlockOffset = selOffset;
		      break;
		    
		    case nsITextServicesDocument::eBlockNotFound:		// There is no text block (TB) in or before the selection (S).
		    default:
		        NS_NOTREACHED("Shouldn't ever get this status");
		  }
		}
		else
		{
		  rv = aDoc->LastBlock();
		}
  }

  return rv;
}


// ----------------------------------------------------------------
//	CharsMatch
//
//	Compare chars. Match if both are whitespace, or both are
//	non whitespace and same char.
// ----------------------------------------------------------------

inline static PRBool CharsMatch(PRUnichar c1, PRUnichar c2)
{
	return (nsString::IsSpace(c1) && nsString::IsSpace(c2)) ||
						(c1 == c2);
	
}

// ----------------------------------------------------------------
//	FindInString
//
//	Routine to search in an nsString which is smart about extra
//  whitespace, can search backwards, and do case insensitive search.
//
//	This uses a brute-force algorithm, which should be sufficient
//	for our purposes (text searching)
// 
//	searchStr contains the text from a content node, which can contain
//	extra white space between words, which we have to deal with.
//	The offsets passed in and back are offsets into searchStr,
//	and thus include extra white space.
//
//	If we are ignoring case, the strings have already been lowercased
// 	at this point.
//
//  If we are searching for entire words only, wordBreaker is non-NULL
//  and is used to check whether the found string is an entire word.
//  If wordBreaker is NULL, we are not searching for entire words only.
//
//  startOffset is the offset in the search string to start seraching
//  at. If -1, it means search from the start (forwards) or end (backwards).
//
//	Returns -1 if the string is not found, or if the pattern is an
//	empty string, or if startOffset is off the end of the string.
// ----------------------------------------------------------------

PRInt32
CFindComponent::FindInString(const nsString &searchStr, const nsString &patternStr,
						                 PRInt32 startOffset, PRBool searchBackwards, nsIWordBreaker* wordBreaker)
{
	PRInt32		foundOffset = -1;
	PRInt32		patternLen = patternStr.Length();
	PRInt32		searchStrLen = searchStr.Length();
	PRBool    goodMatch;
	PRUint32  wordBegin, wordEnd;
		
	if (patternLen == 0)									// pattern is empty
		return -1;
	
	if (startOffset < 0)
		startOffset = (searchBackwards) ? searchStrLen : 0;
	
	if (startOffset > searchStrLen)			// bad start offset
		return -1;
	
	if (patternLen > searchStrLen)				// pattern is longer than string to search
		return -1;
		
	if (!wordBreaker)
	  goodMatch = PR_TRUE;
	
	const PRUnichar	*searchBuf = searchStr.GetUnicode();
	const PRUnichar	*patternBuf = patternStr.GetUnicode();

	const PRUnichar	*searchEnd = searchBuf + searchStrLen;
	const PRUnichar	*patEnd = patternBuf + patternLen;
	
	if (searchBackwards)
	{
		// searching backwards
		const PRUnichar	*s = searchBuf + startOffset - patternLen - 1;
	
		while (s >= searchBuf)
		{
			if (CharsMatch(*patternBuf, *s))			// start potential match
			{
				const PRUnichar	*t = s;
				const PRUnichar	*p = patternBuf;
				PRInt32		curMatchOffset = t - searchBuf;
				PRBool		inWhitespace = nsString::IsSpace(*p);
				
				while (p < patEnd && CharsMatch(*p, *t))
				{
					if (inWhitespace && !nsString::IsSpace(*p))
					{
						// leaving p whitespace. Eat up addition whitespace in s
						while (t < searchEnd - 1 && nsString::IsSpace(*(t + 1)))
							t ++;
							
						inWhitespace = PR_FALSE;
					}
					else
						inWhitespace = nsString::IsSpace(*p);

					t ++;
					p ++;
				}
				
				if (p == patEnd)
				{
				  if (wordBreaker)
				  {
				    wordBreaker->FindWord(searchBuf, searchStrLen, curMatchOffset, &wordBegin, &wordEnd);
				    goodMatch = ((wordBegin == curMatchOffset) && (wordEnd - wordBegin == patternLen));
				  }
				               
          if (goodMatch)  // always TRUE if wordBreaker == NULL
          {
					  foundOffset = curMatchOffset;
					  goto done;
					}
				}
				
				// could be smart about decrementing s here
			}
		
			s --;
		}
	
	}
	else
	{
		// searching forwards
		
		const PRUnichar	*s = &searchBuf[startOffset];
	
		while (s < searchEnd)
		{
			if (CharsMatch(*patternBuf, *s))			// start potential match
			{
				const PRUnichar	*t = s;
				const PRUnichar	*p = patternBuf;
				PRInt32		curMatchOffset = t - searchBuf;
				PRBool		inWhitespace = nsString::IsSpace(*p);
				
				while (p < patEnd && CharsMatch(*p, *t))
				{
					if (inWhitespace && !nsString::IsSpace(*p))
					{
						// leaving p whitespace. Eat up addition whitespace in s
						while (t < searchEnd - 1 && nsString::IsSpace(*(t + 1)))
							t ++;
							
						inWhitespace = PR_FALSE;
					}
					else
						inWhitespace = nsString::IsSpace(*p);

					t ++;
					p ++;
				}
				
				if (p == patEnd)
				{
				  if (wordBreaker)
				  {
				    wordBreaker->FindWord(searchBuf, searchStrLen, curMatchOffset, &wordBegin, &wordEnd);
				    goodMatch = ((wordBegin == curMatchOffset) && (wordEnd - wordBegin == patternLen));
				  }
				               
          if (goodMatch) // always TRUE if wordBreaker == NULL
				  {
  					foundOffset = curMatchOffset;
  					goto done;
					}
				}
				
				// could be smart about incrementing s here
			}
			
			s ++;
		}
	
	}

done:
	return foundOffset;
}
