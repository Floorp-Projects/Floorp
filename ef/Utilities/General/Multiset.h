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

#ifndef MULTISET_H
#define MULTISET_H

#include "Fundamentals.h"

//
// A multiset (i.e. set that can contain the same element more than once)
// with elements of class Elt.  Class Elt must provide the following operations:
//
//   Copy constructor
//   operator== and operator!=
//
// Elt equality must be reflexive (every Elt is equal to itself), symmetric
// (if a==b then b==a), and transitive.
// 
//
template<class Elt>
class SimpleMultiset
{
	struct Node
	{
		Node *next;
		Uint32 count;
		Elt element;

		Node(Elt element, Uint32 count): count(count), element(element) {}
	};

	Pool &pool;
	Node *nodes;
	Uint32 nNodes;

  public:
	explicit SimpleMultiset(Pool &pool): pool(pool), nodes(0), nNodes(0) {}
	SimpleMultiset(const SimpleMultiset &src): pool(src.pool) {copy(src);}
	void operator=(const SimpleMultiset &src) {copy(src);}
	void move(SimpleMultiset &src);
	void clear() {nodes = 0; nNodes = 0;}

  private:
	void copy(const SimpleMultiset &src);
  public:

	bool empty() const {return !nodes;}
	bool operator==(const SimpleMultiset &s) const;
	bool operator!=(const SimpleMultiset &s) const {return !operator==(s);}

	Uint32 find(Elt element) const;
	Uint32 add(Elt element);
	Uint32 remove(Elt element);

  #ifdef DEBUG
	void verify() const;
  #endif
};


//
// Destructively move the src SimpleMultiset to this SimpleMultiset, leaving the
// src SimpleMultiset empty.
//
template<class Elt>
inline void SimpleMultiset<Elt>::move(SimpleMultiset &src)
{
	assert(&pool == &src.pool);
	nodes = src.nodes;
	nNodes = src.nNodes;
	src.clear();
  #ifdef DEBUG
	verify();
  #endif
}


//
// Assign a copy of the src SimpleMultiset to this SimpleMultiset, whose previous
// contents are destroyed.
//
template<class Elt>
inline void SimpleMultiset<Elt>::copy(const SimpleMultiset &src)
{
	if (&src != this) {
		nNodes = src.nNodes;
		Node **link = &nodes;
		for (const Node *srcNode = src.nodes; srcNode; srcNode = srcNode->next) {
			Node *dstNode = new Node(srcNode.element, srcNode.count);
			*link = dstNode;
			link = &dstNode->next;
		}
		*link = 0;
	}
  #ifdef DEBUG
	verify();
  #endif
}


//
// Return true if this multiset contains the same elements with the same multiplicities
// as multiset s.
//
template<class Elt>
bool SimpleMultiset<Elt>::operator==(const SimpleMultiset &s) const
{
	if (nNodes != s.nNodes)
		return false;
	for (const Node *n = nodes; n; n = n->next)
		if (s.find(n->element) != n->count)
			return false;
	return true;
}


//
// Return the number of times the element is present in the multiset.  This number
// is zero if the element is not present.
//
template<class Elt>
Uint32 SimpleMultiset<Elt>::find(Elt element) const
{
	for (const Node *n = nodes; n; n = n->next)
		if (n->element == element)
			return n->count;
	return 0;
}


//
// Add the element to the multiset.  An element can be present multiple times in the
// same multiset.  Return the number of times the element was present in the multiset
// before this latest addition.
//
template<class Elt>
Uint32 SimpleMultiset<Elt>::add(Elt element)
{
	const Node *n;
	for (n = nodes; n; n = n->next)
		if (n->element == element)
			return n->count++;
	n = new(pool) Node(element, 1);
	n->next = nodes;
	nodes = n;
	nNodes++;
  #ifdef DEBUG
	verify();
  #endif
	return 0;
}


//
// Remove one instance of the element to the multiset.  An element can be present
// multiple times in the same multiset.  Return the number of times the element was
// present in the multiset before this latest removal.  If the element was not
// present at all in the multiset before this removal, do nothing and return 0.
//
template<class Elt>
Uint32 SimpleMultiset<Elt>::remove(Elt element)
{
	const Node **prev;
	const Node *n;
	for (prev = &nodes; (n = *prev) != 0; prev = &n->next)
		if (n->element == element) {
			Uint32 count = n->count--;
			if (count == 1) {
				*prev = n->next;
				nNodes--;
			}
		  #ifdef DEBUG
			verify();
		  #endif
			return count;
		}
	return 0;
}


#ifdef DEBUG
//
// Verify that the multiset is internally consistent.  Assert if anything wrong is found.
//
template<class Elt>
void SimpleMultiset<Elt>::verify() const
{
	// Check nNodes.
	Uint32 i = 0;
	const Node *n;
	for (n = nodes; n; n = n->next)
		i++;
	assert(i == nNodes);

	// Check that all elements are unique and none has a count of zero.
	for (n = nodes; n; n = n->next) {
		assert(n->count != 0);
		for (const Node *m = n->next; m; m = m->next)
			assert(m->element != n->element);
	}
}
#endif
#endif
