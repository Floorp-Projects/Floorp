/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _nsMsgKeySet_H_
#define _nsMsgKeySet_H_

#include "msgCore.h"

// nsMsgKeySet represents a set of articles.  Typically, it is the set of
// read articles from a .newsrc file, but it can be used for other purposes
// too.

#if 0
// If a MSG_NewsHost* is supplied to the creation routine, then that
// MSG_NewsHost will be notified whenever a change is made to set.
class MSG_NewsHost;
#endif

class NS_MSG_BASE nsMsgKeySet {
public:
  // Creates an empty set.
  static nsMsgKeySet* Create(/* MSG_NewsHost* host = NULL*/);

  // Creates a set from the list of numbers, as might be found in a
  // newsrc file.
  static nsMsgKeySet* Create(const char* str/* , MSG_NewsHost* host = NULL*/);
  ~nsMsgKeySet();
  
  // FirstNonMember() returns the lowest non-member of the set that is
  // greater than 0.
  PRInt32 FirstNonMember();

  // Output() converts to a string representation suitable for writing to a
  // .newsrc file.  
  nsresult Output(char **outputStr);		

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
  nsMsgKeySet(/* MSG_NewsHost* host */);
  nsMsgKeySet(const char* /* , MSG_NewsHost* host */);
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


#endif /* _nsMsgKeySet_H_ */
