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
