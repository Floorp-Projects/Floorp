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
//
// A test program for the Tree classes
//

#include <stdio.h>
#include "TreeTest.h"
#include "DebugUtils.h"


//
// Return a pseudo-random number between 0 and range-1, inclusive.
// This is not a good pseudo-random number generator (it has biases),
// but it will do fine for generating tests.
// range should be relatively small -- less than 1000 or so.
//
Uint32 random(Uint16 range)
{
	static Uint32 seed = 1;

	seed *= 0x10003;
	return (seed>>16) % range;
}


// ----------------------------------------------------------------------------
// TreeHarness


//
// Initialize the TreeHarness, which contains both a SortedTree and a
// rep value that is a bitmap that is supposed to match the contents of
// the tree.
//
TreeHarness::TreeHarness(): rep(0)
{
	for (Uint32 i = nNodes; i--;)
		nodes[i].key.n = i;
}


//
// Return a random node number of a node present in the tree or nNodes
// if the tree is empty.
//
Uint32 TreeHarness::randomPresent() const
{
	Uint32 limit = tree.getNNodes();
	if (limit) {
		Uint32 i = random(limit);
		for (Uint32 j = 0; j != nNodes; j++)
			if (present(j))
				if (i)
					i--;
				else
					return j;
		trespass("Error in randomPresent");
	}
	return nNodes;
}


//
// Remove all elements from the tree.
//
void TreeHarness::testClear()
{
	Uint32 n;
	while ((n = randomPresent()) != nNodes)
		testRemove(n);
	assert(rep == 0);
}


//
// Test the find method on element n, which may or may not be in the tree.
//
void TreeHarness::testFind(Uint32 n) const
{
	TestNode *node = tree.find(TestKey(n));
	if (present(n))
		assert(node == nodes + n);
	else
		assert(!node);
}


//
// Test the findAfter method on element n, which may or may not be in the tree.
// Return the number of the lowest element in the set that is greater than or
// equal to n, or nNodes if there is no such element.
//
Uint32 TreeHarness::testFindAfter(Uint32 n) const
{
	TestNode *node = tree.findAfter(TestKey(n));
	Uint32 i = n;
	while (i != nNodes && !present(i))
		i++;
	if (i != nNodes)
		assert(node == nodes + i);
	else
		assert(!node);
	return i;
}


//
// Test the findBefore method on element n, which may or may not be in the tree.
// Return the number of the greatest element in the set that is less than or
// equal to n, or nNodes if there is no such element.
//
Uint32 TreeHarness::testFindBefore(Uint32 n) const
{
	TestNode *node = tree.findBefore(TestKey(n));
	Uint32 i = n;
	while (i != 0 && !present(i))
		i--;
	if (present(i)) {
		assert(node == nodes + i);
		return i;
	} else {
		assert(!node);
		return nNodes;
	}
}


//
// Test the three-argument find method on element n, which may or may not be
// in the tree.  If element n is not in the tree, add it.
//
void TreeHarness::testFindAttach(Uint32 n)
{
	TestNode *where;
	bool right;
	TestNode *node = tree.find(TestKey(n), where, right);
	if (!node)
		tree.attach(nodes[n], where, right);

	if (present(n))
		assert(node == nodes + n);
	else {
		assert(!node);
		add(n);
	}
}


//
// Test the insert method on element n, which is not currently in the tree.
//
void TreeHarness::testInsert(Uint32 n)
{
	assert(!present(n));
	tree.insert(nodes[n]);
	add(n);
}


//
// Test the insertAfter method on element n, which is not currently in the tree,
// and location where, which is a valid argument to insertAfter for this n.
// If where==nNodes, then insertAfter's where is set to nil.
//
void TreeHarness::testInsertAfter(Uint32 n, Uint32 where)
{
	assert(!present(n) && (where == nNodes || present(where)));
	tree.insertAfter(nodes[n], where == nNodes ? 0 : nodes + where);
	add(n);
}


//
// Test the insertBefore method on element n, which is not currently in the tree,
// and location where, which is a valid argument to insertBefore for this n.
// If where==nNodes, then insertBefore's where is set to nil.
//
void TreeHarness::testInsertBefore(Uint32 n, Uint32 where)
{
	assert(!present(n) && (where == nNodes || present(where)));
	tree.insertBefore(nodes[n], where == nNodes ? 0 : nodes + where);
	add(n);
}


//
// Test the remove method on element n, which is currently in the tree.
//
void TreeHarness::testRemove(Uint32 n)
{
	assert(present(n));
	tree.remove(nodes[n]);
	remove(n);
}


//
// Test the remove substitute on element newN, which is not currently in the tree
// (unless newN==oldN) and element oldN, which is currently in the tree.  The caller
// guarantees that the combination of newN and oldN is legal.
//
void TreeHarness::testSubstitute(Uint32 newN, Uint32 oldN)
{
	assert(newN == oldN || !present(newN) && present(oldN));
	tree.substitute(nodes[newN], nodes[oldN]);
	remove(oldN);
	add(newN);
}


//
// Make sure that the contents of the tree and rep match.
//
void TreeHarness::verify() const
{
	tree.verify();

	// Make sure that there is a one-to-one correspondence between the tree nodes
	// and the bits set in rep.
	Uint32 r = rep;
	for (TestNode *p = tree.firstNode(); p; p = p->next()) {
		Uint32 n = p->key.n;
		assert(r & 1<<n);
		assert(p == nodes + n);
		r &= ~(1<<n);
	}
	assert(r == 0);
}


//
// Print the subtree with the given root.
//
void TreeHarness::printSubtree(TestNode *node)
{
	if (node) {
		printf(node->isRed() ? "(" : "[");
		printSubtree(node->getChild(false));
		printf(" %d ", node->key.n);
		printSubtree(node->getChild(true));
		printf(node->isRed() ? ")" : "]");
	} else
		printf("-");
}

//
// Print the current state of the TreeHarness.
//
void TreeHarness::print() const
{
	printSubtree(tree.getRoot());
	printf("\n");
}


// ----------------------------------------------------------------------------


//
// Test the implementation of SortedTree.  Assert if problems are found.
//
void testTrees()
{
	printf("Testing SortedTree...\n");
	TreeHarness h;

	for (Uint32 i = 0; i != 10000; i++) {
		Uint32 n = h.randomNode();
		Uint32 m;
		Uint32 c = 0;

		switch (random(21)) {
			case 0:	// Clear the tree (but do it infrequently)
				if (random(5) != 2)
					continue;
				if (printProgress)
					c = printf("Clear");
				h.testClear();
				break;

			case 1:	// Test finding an element
			case 2:
				if (printProgress)
					c = printf("Find %d", n);
				h.testFind(n);
				break;

			case 3:	// Test findAfter
			case 4:
				if (printProgress)
					c = printf("FindAfter %d", n);
				h.testFindAfter(n);
				break;

			case 5:	// Test findBefore
			case 6:
				if (printProgress)
					c = printf("FindBefore %d", n);
				h.testFindBefore(n);
				break;

			case 7:	// Test findAttach
			case 8:
				if (printProgress)
					c = printf("Find+Attach %d", n);
				h.testFindAttach(n);
				break;

			case 9:	// Test insert
			case 10:
				if (h.present(n))
					continue;
				if (printProgress)
					c = printf("Insert %d", n);
				h.testInsert(n);
				break;

			case 11: // Test insertAfter
			case 12:
				if (h.present(n))
					continue;
				m = h.testFindBefore(n);
				if (printProgress)
					if (m == h.nNodes)
						c = printf("Insert %d after nil", n);
					else
						c = printf("Insert %d after %d", n, m);
				h.testInsertAfter(n, m);
				break;

			case 13: // Test insertBefore
			case 14:
				if (h.present(n))
					continue;
				m = h.testFindAfter(n);
				if (printProgress)
					if (m == h.nNodes)
						c = printf("Insert %d before nil", n);
					else
						c = printf("Insert %d before %d", n, m);
				h.testInsertBefore(n, m);
				break;

			case 15: // Test remove
			case 16:
			case 17:
				n = h.randomPresent();
				if (n == h.nNodes)
					continue;
				if (printProgress)
					c = printf("Remove %d", n);
				h.testRemove(n);
				break;

			case 18: // Test substitute
			case 19:
				n = h.randomPresent();
				m = n^1;
				if (n == h.nNodes || m >= h.nNodes || h.present(m))
					continue;
				if (printProgress)
					c = printf("Substitute %d for %d", m, n);
				h.testSubstitute(m, n);
				break;

			case 20: // Test self-substitute
				n = h.randomPresent();
				if (n == h.nNodes)
					continue;
				if (printProgress)
					c = printf("Substitute %d for %d", n, n);
				h.testSubstitute(n, n);
				break;

			default:
				trespass("Bad case");
		}

		h.verify();
		if (printProgress) {
			printSpaces(stdout, 32 - c);
			h.print();
		}
	}
	h.testClear();
	h.verify();
	printf("Done testing SortedTree.\n");
}
