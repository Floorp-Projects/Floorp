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

#ifndef _NewsSet_H_
#define _NewsSet_H_

// msg_NewsArtSet represents a set of articles.  Typically, it is the set of
// read articles from a .newsrc file, but it can be used for other purposes
// too.

// If a MSG_NewsHost* is supplied to the creation routine, then that
// MSG_NewsHost will be notified whwnever a change is made to set.

class MSG_NewsHost;

class msg_NewsArtSet {
public:
	// Creates an empty set.
	static msg_NewsArtSet* Create(MSG_NewsHost* host = NULL);

	// Creates a set from the list of numbers, as might be found in a
	// newsrc file.
	static msg_NewsArtSet* Create(const char* str, MSG_NewsHost* host = NULL);
	~msg_NewsArtSet();

	// FirstNonMember() returns the lowest non-member of the set that is
	// greater than 0.
	int32 FirstNonMember();

	// Output() converts to a string representation suitable for writing to a
	// .newsrc file.  (The result must be freed by the caller using delete[].)
	char* Output();		

	// IsMember() returns whether the given article is a member of this set.
	XP_Bool IsMember(int32 art);

	// Add() adds the given article to the set.  (Returns 1 if a change was
	// made, 0 if it was already there, and negative on error.)
	int Add(int32 art);

	// Remove() removes the given article from the set. 
	int Remove(int32 art);

	// AddRange() adds the (inclusive) given range of articles to the set.
	int AddRange(int32 first, int32 last);

	// CountMissingInRange() takes an inclusive range of articles and returns
	// the number of articles in that range which are not in the set.
 	int32 CountMissingInRange(int32 start, int32 end);

	// FirstMissingRange() takes an inclusive range and finds the first range
	// of articles that are not in the set.  If none, return zeros. 
	int FirstMissingRange(int32 min, int32 max, int32* first, int32* last);


	// LastMissingRange() takes an inclusive range and finds the last range
	// of articles that are not in the set.  If none, return zeros. 
	int LastMissingRange(int32 min, int32 max, int32* first, int32* last);

	int32 GetLastMember();
	int32 GetFirstMember();
	void  SetLastMember(int32 highWaterMark);
	// For debugging only...
	int32 getLength() {return m_length;}

#ifdef DEBUG
	static void RunTests();
#endif

protected:
	msg_NewsArtSet(MSG_NewsHost* host);
	msg_NewsArtSet(const char*, MSG_NewsHost* host);
	XP_Bool Grow();
	XP_Bool Optimize();

#ifdef DEBUG
	static void test_decoder(const char*);
	static void test_adder();
	static void test_ranges();
	static void test_member(XP_Bool with_cache);
#endif

	int32 *m_data;					/* the numbers composing the `chunks' */
	int32 m_data_size;				/* size of that malloc'ed block */
	int32 m_length;				/* active area */

	int32 m_cached_value;			/* a potential set member, or -1 if unset*/
	int32 m_cached_value_index;		/* the index into `data' at which a search
									   to determine whether `cached_value' was
									   a member of the set ended. */
	MSG_NewsHost* m_host;
};


#endif /* _NewsSet_H_ */
