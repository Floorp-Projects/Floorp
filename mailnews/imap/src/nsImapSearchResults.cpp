/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "msgCore.h"  // for pre-compiled headers

#include "nsImapCore.h"
#include "nsImapSearchResults.h"


nsImapSearchResultSequence::nsImapSearchResultSequence()
{
}

nsImapSearchResultSequence *nsImapSearchResultSequence::CreateSearchResultSequence()
{
	nsImapSearchResultSequence *returnObject = new nsImapSearchResultSequence;
	if (!returnObject)
	{
		delete returnObject;
		returnObject = nil;
	}
	
	return returnObject;
}

void nsImapSearchResultSequence::Clear(void)
{
  PRInt32 index = mCount;
  while (0 <= --index) {
    char* string = (char*)mArray[index];
    delete string;
  }
  nsVoidArray::Clear();
}

nsImapSearchResultSequence::~nsImapSearchResultSequence()
{
	Clear();
}


void nsImapSearchResultSequence::ResetSequence()
{
	Clear();
}

void nsImapSearchResultSequence::AddSearchResultLine(const char *searchLine)
{
	// The first add becomes node 2.  Fix this.
	char *copiedSequence = PL_strdup(searchLine + 9); // 9 == "* SEARCH "

	if (copiedSequence)	// if we can't allocate this then the search won't hit
		AppendElement(copiedSequence);
}


nsImapSearchResultIterator::nsImapSearchResultIterator(nsImapSearchResultSequence &sequence) :
	fSequence(sequence)
{
	ResetIterator();
}

nsImapSearchResultIterator::~nsImapSearchResultIterator()
{
}

void  nsImapSearchResultIterator::ResetIterator()
{
	fSequenceIndex = 0;
	fCurrentLine = (fSequence.Count() > 0) ? (char *) fSequence.ElementAt(fSequenceIndex) : nsnull;
	if (fCurrentLine)
		fPositionInCurrentLine = fCurrentLine;
	else
		fPositionInCurrentLine = nsnull;
}

PRInt32 nsImapSearchResultIterator::GetNextMessageNumber()
{
	int32 returnValue = 0;
	if (fPositionInCurrentLine)
	{	
		returnValue = atoi(fPositionInCurrentLine);
		
		// eat the current number
		while (isdigit(*++fPositionInCurrentLine))
			;
		
		if (*fPositionInCurrentLine == 0xD)	// found CR, no more digits on line
		{
			fCurrentLine = (fSequence.Count() > ++fSequenceIndex) ? (char *) fSequence.ElementAt(fSequenceIndex) : nsnull;
			if (fCurrentLine)
				fPositionInCurrentLine = fCurrentLine;
			else
				fPositionInCurrentLine = nil;
		}
		else	// eat the space
			fPositionInCurrentLine++;
	}
	
	return returnValue;
}


