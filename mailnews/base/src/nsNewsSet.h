/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _nsNewsSet_H_
#define _nsNewsSet_H_

// msg_NewsArtSet represents a set of articles.  Typically, it is the set of
// read articles from a .newsrc file, but it can be used for other purposes
// too.

// If a MSG_NewsHost* is supplied to the creation routine, then that
// MSG_NewsHost will be notified whwnever a change is made to set.

class MSG_NewsHost;

class nsNewsSet {
public:
	// Creates an empty set.
	static nsNewsSet* Create(MSG_NewsHost* host = NULL);

	// Creates a set from the list of numbers, as might be found in a
	// newsrc file.
	static nsNewsSet* Create(const char* str, MSG_NewsHost* host = NULL);
	~nsNewsSet();

	// FirstNonMember() returns the lowest non-member of the set that is
	// greater than 0.
	PRInt32 FirstNonMember();

	// Output() converts to a string representation suitable for writing to a
	// .newsrc file.  (The result must be freed by the caller using delete[].)
	char* Output();		

	// IsMember() returns whether the given article is a member of this set.
	PRBool IsMember(PRInt32 art);

	// Add() adds the given article to the set.  (Returns 1 if a change was
	// made, 0 if it was already there, and negative on error.)
	int Add(PRInt32 art);

	// Remove() removes the given article from the set. 
	int Remove(PRInt32 art);

	// AddRange() adds the (inclusive) given range of articles to the set.
	int AddRange(PRInt32 first, PRInt32 last);

	// CountMissingInRange() takes an inclusive range of articles and returns
	// the number of articles in that range which are not in the set.
 	PRInt32 CountMissingInRange(PRInt32 start, PRInt32 end);

	// FirstMissingRange() takes an inclusive range and finds the first range
	// of articles that are not in the set.  If none, return zeros. 
	int FirstMissingRange(PRInt32 min, PRInt32 max, PRInt32* first, PRInt32* last);


	// LastMissingRange() takes an inclusive range and finds the last range
	// of articles that are not in the set.  If none, return zeros. 
	int LastMissingRange(PRInt32 min, PRInt32 max, PRInt32* first, PRInt32* last);

	PRInt32 GetLastMember();
	PRInt32 GetFirstMember();
	void  SetLastMember(PRInt32 highWaterMark);
	// For debugging only...
	PRInt32 getLength() {return m_length;}

#ifdef DEBUG
	static void RunTests();
#endif

protected:
	nsNewsSet(MSG_NewsHost* host);
	nsNewsSet(const char*, MSG_NewsHost* host);
	PRBool Grow();
	PRBool Optimize();

#ifdef DEBUG
	static void test_decoder(const char*);
	static void test_adder();
	static void test_ranges();
	static void test_member(PRBool with_cache);
#endif

	PRInt32 *m_data;					/* the numbers composing the `chunks' */
	PRInt32 m_data_size;				/* size of that malloc'ed block */
	PRInt32 m_length;				/* active area */

	PRInt32 m_cached_value;			/* a potential set member, or -1 if unset*/
	PRInt32 m_cached_value_index;		/* the index into `data' at which a search
									   to determine whether `cached_value' was
									   a member of the set ended. */
#ifdef NEWSRC_DOES_HOST_STUFF
	MSG_NewsHost* m_host;
#endif
};


#endif /* _nsNewsSet_H_ */
