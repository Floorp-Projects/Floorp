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

#ifndef TREETEST_H
#define TREETEST_H

#include "Tree.h"


const bool printProgress = true;				// Set to print the progress of each test

Uint32 random(Uint16 range);


// --- PRIVATE ----------------------------------------------------------------

class TreeHarness
{
	// We could have used Uint32 directly instead of struct TestKey, but
	// this way we can check that the Tree templates do not expect any methods
	// other than operator< to apply to keys.
	struct TestKey
	{
		Uint32 n;								// Number of the node that contains this key

		explicit TestKey(Uint32 n): n(n) {}

		bool operator<(const TestKey &key2) const {return n < key2.n;}
	};

	struct TestNode: TreeNode<TestNode>
	{
		TestKey key;

		TestNode(): key(0) {}

		const TestKey &getKey() const {return key;}
	};

  public:
	static const nNodes = 32;

  private:
	SortedTree<TestNode, TestKey> tree;
	Uint32 rep;									// Bitmap of what should be in the tree
	TestNode nodes[nNodes];

  public:
	TreeHarness();

	static inline Uint32 randomNode() {return random(nNodes);}
	inline bool present(Uint32 n) const {return rep>>n & 1;}
	inline void add(Uint32 n) {rep |= 1<<n;}
	inline void remove(Uint32 n) {rep &= ~(1<<n);}
	Uint32 randomPresent() const;

	void testClear();
	void testFind(Uint32 n) const;
	Uint32 testFindAfter(Uint32 n) const;
	Uint32 testFindBefore(Uint32 n) const;
	void testFindAttach(Uint32 n);
	void testInsert(Uint32 n);
	void testInsertAfter(Uint32 n, Uint32 where);
	void testInsertBefore(Uint32 n, Uint32 where);
	void testRemove(Uint32 n);
	void testSubstitute(Uint32 newN, Uint32 oldN);

	void verify() const;
  private:
	static void printSubtree(TestNode *n);
  public:
	void print() const;
};

// --- PUBLIC -----------------------------------------------------------------

void testTrees();

#endif
