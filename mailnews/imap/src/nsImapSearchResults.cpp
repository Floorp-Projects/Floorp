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
 * Portions created by the Initial Developer are Copyright (C) 1999
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
		returnObject = nsnull;
	}
	
	return returnObject;
}

void nsImapSearchResultSequence::Clear(void)
{
    if (mImpl) {
        PRInt32 i = mImpl->mCount;
        while (0 <= --i) {
            char* string = (char*)mImpl->mArray[i];
            delete string;
        }
        nsVoidArray::Clear();
    }
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
				fPositionInCurrentLine = nsnull;
		}
		else	// eat the space
			fPositionInCurrentLine++;
	}
	
	return returnValue;
}


