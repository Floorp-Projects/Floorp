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

#ifndef DOUBLYLINKEDLIST_H
#define DOUBLYLINKEDLIST_H

#include "Fundamentals.h"

// --- PRIVATE ----------------------------------------------------------------

struct DoublyLinkedNode
{
	DoublyLinkedNode *next;						// Link to next node	 (in DEBUG versions nil if not explicitly set)
	DoublyLinkedNode *prev;						// Link to previous node (in DEBUG versions nil if not explicitly set)

	void remove();
	void substitute(DoublyLinkedNode &src);
	void insertAfter(DoublyLinkedNode &loc);
	void insertBefore(DoublyLinkedNode &loc);

  #ifdef DEBUG
	void init() {next = 0; prev = 0;}
	bool isLinked() const {return next && prev;}
	bool isUnlinked() const {return !next && !prev;}
  #else
	void init() {}
  #endif
};


class DoublyLinkedRoot
{
  protected:
	DoublyLinkedNode root;

  private:
#if 0
	DoublyLinkedRoot(const DoublyLinkedRoot &);			// Copying forbidden
	void operator=(const DoublyLinkedRoot &);			// Copying forbidden
#endif
  public:
	DoublyLinkedRoot(const DoublyLinkedRoot &){ PR_ASSERT(0); }	// Copying forbidden
	void operator=(const DoublyLinkedRoot &){ PR_ASSERT(0); }	// Copying forbidden
	DoublyLinkedRoot() {root.next = &root; root.prev = &root;}
	
	void init() {root.next = &root; root.prev = &root;}
	bool empty() const {return root.next == &root;}

  protected:
	void removeFirstNode();
};


// --- PUBLIC -----------------------------------------------------------------


// Derive doubly-linked list nodes from this class. N is the node class, which should
// be a subclass of this class.
template<class N>
class DoublyLinkedEntry: public DoublyLinkedNode
{
  #ifdef DEBUG
	DoublyLinkedEntry(const DoublyLinkedEntry<N> &);	// Copying forbidden
	void operator=(const DoublyLinkedEntry<N> &);		// Copying forbidden
  public:
	DoublyLinkedEntry() {init();}
  #endif
  public:

	// DoublyLinkedNode administration
	static N &linkOwner(DoublyLinkedNode &l) {return *static_cast<N *>(&l);}
	DoublyLinkedNode &getLinks() {return *this;}
};


// Use this class for doubly-linked list containers. N is the node class, which should
// inherit from DoublyLinkedEntry<N>.
template<class N>
class DoublyLinkedList: public DoublyLinkedRoot
{
  public:
	typedef DoublyLinkedNode *iterator;

	// To iterate forward through a DoublyLinkedList<T> dl, use:
	//	 for (DoublyLinkedList<T>::iterator i = dl.begin(); !dl.done(i); i = dl.advance(i))
	//		 ... dl.get(i) ...
	iterator begin() const {return root.next;}
	iterator end() const {return root.prev;}
	NONDEBUG_ONLY(static) iterator location(N &node) {assert(exists(node)); return &node.getLinks();}
	static N &get(iterator i) {return N::linkOwner(*i);}
	static iterator advance(iterator i) {return i->next;}
	static iterator retreat(iterator i) {return i->prev;}
	bool done(iterator i) const {return i == &root;}
  #ifdef DEBUG
	bool validIterator(iterator i) const;
  #endif

	Uint32 length() const;
	bool lengthIs(Uint32 n) const;
	N &first() const {assert(!empty()); return get(root.next);}
	N &last() const {assert(!empty()); return get(root.prev);}
	bool exists(N &node) const;
	Uint32 index(N &node) const;

	void addFirst(N &node);
	void addLast(N &node);
	NONDEBUG_ONLY(static) void insertBefore(N &node, iterator i);
	NONDEBUG_ONLY(static) void insertAfter(N &node, iterator i);
	void removeFirst();
	void removeLast();
	void clear();
	void move(DoublyLinkedList<N> &src);
};


template<class N>
class SortedDoublyLinkedList: public DoublyLinkedList<N>
{
  public:
	explicit SortedDoublyLinkedList(int (*compare)(const N *elem1, const N *elem2)): compareFunc(compare) {}
	void insert(N &node);
	bool isInList(N &node);

  private:
	int (*const compareFunc)(const N*, const N*);	// Comparison function returns 1 for >, -1 for <, and 0 for ==.
};


// --- INLINES ----------------------------------------------------------------


//
// Unlink this node from the list into which it is linked.
//
inline void DoublyLinkedNode::remove()
{
	assert(isLinked() && next != this);
	DoublyLinkedNode *n = next;
	DoublyLinkedNode *p = prev;
	n->prev = p;
	p->next = n;
	init();
}


//
// Insert this node after node loc in a doubly-linked list.
// Node loc must be already linked into a list, while
// this node should not be already linked into any list.
//
inline void DoublyLinkedNode::insertAfter(DoublyLinkedNode &loc)
{
	assert(isUnlinked() && loc.isLinked());
	next = loc.next;
	prev = &loc;
	loc.next->prev = this;
	loc.next = this;
}


//
// Insert this node before node loc in a doubly-linked list.
// Node loc must be already linked into a list, while
// this node should not be already linked into any list.
//
inline void DoublyLinkedNode::insertBefore(DoublyLinkedNode &loc)
{
	assert(isUnlinked() && loc.isLinked());
	next = &loc;
	prev = loc.prev;
	loc.prev->next = this;
	loc.prev = this;
}


//
// Unlink src from the list into which it is linked and link this node in src's
// place.  src must not be the list's root (unless called from the move method below).
//
inline void DoublyLinkedNode::substitute(DoublyLinkedNode &src)
{
	assert(isUnlinked() && src.isLinked());
	DoublyLinkedNode *n = src.next;
	DoublyLinkedNode *p = src.prev;
	assert(n != &src && p != &src && n != this && p != this && n->prev == &src && p->next == &src);
	next = n;
	prev = p;
	n->prev = this;
	p->next = this;
	src.init();
}


//
// Return the number of nodes (not including the root) in the doubly-linked list.
//
template<class N>
Uint32 DoublyLinkedList<N>::length() const
{
	Uint32 n = 0;
	for (iterator i = begin(); !done(i); i = advance(i))
		n++;
	return n;
}


//
// Return true if the number of nodes (not including the root) in the doubly-linked list
// is equal to n.  This is often faster than "length() == n" because if n is small it can
// stop iterating through the list as soon as it exceeds n.
//
template<class N>
bool DoublyLinkedList<N>::lengthIs(Uint32 n) const
{
	for (iterator i = begin(); !done(i); i = advance(i))
		if (n-- == 0)
			return false;
	return n == 0;
}


#ifdef DEBUG
//
// Return true if the iterator points somewhere within this doubly-linked list
// (including its root).
//
template<class N>
bool DoublyLinkedList<N>::validIterator(DoublyLinkedNode* i) const
{
	iterator j;
	for (j = begin(); !done(j); j = advance(j))
		if (i == j)
			return true;
	return i == j;
}
#endif


//
// Return true if node is in the list and false otherwise.
//
template<class N>
bool DoublyLinkedList<N>::exists(N &node) const
{
	for (iterator current = this->begin(); !done(current); current = advance(current))
		if (&node == &this->get(current))
			return true;
	return false;
}


//
// Return the zero-based index of node in the list or this->length() if
// the node is not in the list.
//
template<class N>
Uint32 DoublyLinkedList<N>::index(N &node) const
{
	Uint32 index = 0;
	for (iterator current = this->begin(); !done(current) && &node != &this->get(current); current = advance(current))
		index++;
	return index;
}


//
// Insert node into the doubly-linked list at its beginning.
// The node should not be already linked into any list.
//
template<class N>
inline void DoublyLinkedList<N>::addFirst(N &node)
{
	node.getLinks().insertAfter(root);
}


//
// Insert node into the doubly-linked list at its end.
// The node should not be already linked into any list.
//
template<class N>
inline void DoublyLinkedList<N>::addLast(N &node)
{
	node.getLinks().insertBefore(root);
}


//
// Insert node into the doubly-linked list before iterator i.
// The node should not be already linked into any list.
//
template<class N>
inline void DoublyLinkedList<N>::insertBefore(N &node, DoublyLinkedNode* i)
{
	assert(validIterator(i));
	node.getLinks().insertBefore(*i);
}


//
// Insert node into the doubly-linked list after iterator i.
// The node should not be already linked into any list.
//
template<class N>
inline void DoublyLinkedList<N>::insertAfter(N &node, DoublyLinkedNode* i)
{
	assert(validIterator(i));
	node.getLinks().insertAfter(*i);
}


//
// Remove the first node, if any, from this doubly-linked list.
//
template<class N>
inline void DoublyLinkedList<N>::removeFirst()
{
	if (!empty())
		root.next->remove();
}


//
// Remove the last node, if any, from this doubly-linked list.
//
template<class N>
inline void DoublyLinkedList<N>::removeLast()
{
	if (!empty())
		root.prev->remove();
}


//
// Remove all nodes from this doubly-linked list.
//
template<class N>
inline void DoublyLinkedList<N>::clear()
{
	while (!empty())
		root.next->remove();
}


//
// Destructively move the src DoublyLinkedList to this DoublyLinkedList.
// The src DoublyLinkedList will subsequently be empty.  This DoublyLinkedList
// must be empty prior to this call.
//
template<class N>
void DoublyLinkedList<N>::move(DoublyLinkedList<N> &src)
{
	assert(empty() && &src != this);

	if (!src.empty()) {
		root.init();	// Avoid assert inside substitute.
		root.substitute(src.root);
		src.init();
	}
}


//
// Uses the specified compare function to search for node.  
// Returns true if node is in the list and false otherwise.
//
template<class N>
bool SortedDoublyLinkedList<N>::isInList(N &node)
{ 
  iterator current = this->begin();
  for (; !done(current) && (compareFunc(&node, &(this->get(current))) == 1);
	  current = advance(current))
	{}  // do nothing in body

  if (done(current))
  	return false;
  else
  	return (compareFunc(&node, &(this->get(current))) == 0);
}


//
// Inserts node into the sorted list unless it already exists in the list.  
//
template<class N>
void SortedDoublyLinkedList<N>::insert(N &node)
{
  // loop will terminate with current pointing to the spot where we wish to insert node.
  // last will be the node before the desired spot.  If the element is already in the list then
  // current will point to it.
  iterator current = this->begin();
  iterator last = current;
  for(;  !done(current) && (compareFunc(&node, &(this->get(current))) == 1); 
	  current = advance(current))
	last = current; 
   
  if(this->done(current)) // special case add to end of list
    addLast(node);
  else if(compareFunc(&node, &(this->get(current))) != 0)  // Do not insert if element is in the list already
    {                                                      // might want this to be users responsibilities
      if (current == this->begin())
        addFirst(node);
      else
        {
          // add to list between last and current
          DoublyLinkedNode& beforeNode = this->get(last).getLinks();
          DoublyLinkedNode& afterNode = this->get(current).getLinks();
          beforeNode.next = &(node.getLinks());
          node.getLinks().prev = &beforeNode;
          afterNode.prev = &(node.getLinks());
          node.getLinks().next = &afterNode;
        }
    }
}
	
#endif
