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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsImapSearchResults_h___
#define nsImapSearchResults_h___


class nsImapSearchResultSequence : public nsVoidArray
{
public:
    virtual ~nsImapSearchResultSequence();
    static nsImapSearchResultSequence *CreateSearchResultSequence();
    
    virtual void AddSearchResultLine(const char *searchLine);
    virtual void ResetSequence();
	void		Clear();
    
    friend class nsImapSearchResultIterator;
private:
    nsImapSearchResultSequence();
};

class nsImapSearchResultIterator {
public:
    nsImapSearchResultIterator(nsImapSearchResultSequence &sequence);
    virtual ~nsImapSearchResultIterator();
    
    void  ResetIterator();
    PRInt32 GetNextMessageNumber();   // returns 0 at end of list
private:
    nsImapSearchResultSequence &fSequence;
	PRInt32 fSequenceIndex;
	char	*fCurrentLine;
    char    *fPositionInCurrentLine;
};



#endif
