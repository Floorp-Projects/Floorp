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

#ifndef TREE_H
#define TREE_H

#include "Fundamentals.h"

// --- PRIVATE ----------------------------------------------------------------

struct TreeNodeImpl
{
  private:
	TreeNodeImpl *parent;						// This node's parent or nil if this is the root
	TreeNodeImpl *children[2];					// This node's left (0) and right (1) children
	bool red BOOL_8;							// This node is black if false, red if true
	bool right BOOL_8;							// This node is a left child if false, right if true
  #ifdef DEBUG
	bool linked BOOL_8;							// True if this node is linked into a tree
  #endif

  public:
  #ifdef DEBUG
	TreeNodeImpl(): linked(false) {}
	~TreeNodeImpl() {assert(!linked);}
  #endif

	void link() {assert(!linked); DEBUG_ONLY(linked = true);}
	void unlink() {assert(linked); DEBUG_ONLY(linked = false);}

	TreeNodeImpl *getParent() const {assert(linked); return parent;}
	TreeNodeImpl *getChild(bool right) const {assert(linked); return children[right];}
	bool isBlack() const {assert(linked); return !red;}
	bool isRed() const {assert(linked); return red;}
	bool isLeft() const {assert(linked); return !right;}
	bool isRight() const {assert(linked); return right;}

	void setParent(TreeNodeImpl *p) {assert(linked); parent = p;}
	void setChild(bool right, TreeNodeImpl *c) {assert(linked); children[right] = c;}
	void setBlack() {assert(linked); red = false;}
	void setRed(bool red = true) {assert(linked); TreeNodeImpl::red = red;}
	void setLeft() {assert(linked); right = false;}
	void setRight(bool right = true) {assert(linked); TreeNodeImpl::right = right;}
	void move(TreeNodeImpl &src);

	TreeNodeImpl &extremeNode(bool right);
	TreeNodeImpl *prev();
	TreeNodeImpl *next();
};


// Base class for Tree.  Do not use directly.
class TreeImpl
{
	TreeNodeImpl *root;							// The tree's root or nil if the tree is empty
	Uint32 nNodes;								// Current number of nodes linked into the tree

  public:
	TreeImpl(): root(0), nNodes(0) {}

	TreeNodeImpl *getRoot() const {return root;}
	TreeNodeImpl *firstNode() const {return root ? &root->extremeNode(false) : 0;}
	TreeNodeImpl *lastNode() const {return root ? &root->extremeNode(true) : 0;}
	TreeNodeImpl *next(TreeNodeImpl *n) const {return n ? n->next() : firstNode();}
	TreeNodeImpl *prev(TreeNodeImpl *n) const {return n ? n->prev() : lastNode();}
	Uint32 getNNodes() const {return nNodes;}

  private:
	void linkNode(TreeNodeImpl *node, TreeNodeImpl *parent, bool right);
	void rotate(TreeNodeImpl &node, bool right);
	void addNode(TreeNodeImpl &node, TreeNodeImpl *parent, bool right);
  public:

	void attach(TreeNodeImpl &node, TreeNodeImpl *where, bool right);
	void insertAfter(TreeNodeImpl &node, TreeNodeImpl *where);
	void insertBefore(TreeNodeImpl &node, TreeNodeImpl *where);
	void remove(TreeNodeImpl &node);
	void substitute(TreeNodeImpl &newNode, TreeNodeImpl &oldNode);

  #ifdef DEBUG
  private:
	static bool verifySubtree(TreeNodeImpl *node, TreeNodeImpl *parent, bool rightChild, Uint32 &nNodes, Uint32 &blackDepth);
  public:
	void verify() const;
  #endif
};


// --- PUBLIC -----------------------------------------------------------------


// Derive Tree nodes from this class.  Node is the node class, which should
// be a subclass of this class.
template<class Node>
class TreeNode: public TreeNodeImpl
{
  #ifdef DEBUG
	TreeNode(const TreeNode<Node> &);			// Copying forbidden
	void operator=(const TreeNode<Node> &);		// Copying forbidden
  public:
	TreeNode() {}
  #endif
  public:
	Node *getParent() const {return static_cast<Node *>(TreeNodeImpl::getParent());}
	Node *getChild(bool right) const {return static_cast<Node *>(TreeNodeImpl::getChild(right));}

	Node *prev() {return static_cast<Node *>(TreeNodeImpl::prev());}
	Node *next() {return static_cast<Node *>(TreeNodeImpl::next());}
};


//
// A balanced binary tree with nodes of class Node, which must be a subclass of TreeNode<Node>.
//
// Only insert, remove, and substitute operations are provided.  It is up to the user
// to determine the proper place to insert a node.  The Tree also does not do storage
// management (allocation or deallocation) of its nodes; the user should do so explicitly
// if needed.
//
template<class Node>
class Tree: private TreeImpl
{
  public:
	Node *getRoot() const {return static_cast<Node *>(TreeImpl::getRoot());}
	Node *firstNode() const {return static_cast<Node *>(TreeImpl::firstNode());}
	Node *lastNode() const {return static_cast<Node *>(TreeImpl::lastNode());}
	Node *next(Node *n) const {return n ? n->next() : firstNode();}
	Node *prev(Node *n) const {return n ? n->prev() : lastNode();}
	TreeImpl::getNNodes;

	void attach(Node &node, Node *where, bool right) {TreeImpl::attach(node, where, right);}
	void insertAfter(Node &node, Node *where) {TreeImpl::insertAfter(node, where);}
	void insertBefore(Node &node, Node *where) {TreeImpl::insertBefore(node, where);}
	void remove(Node &node) {TreeImpl::remove(node);}
	void substitute(Node &newNode, Node &oldNode) {TreeImpl::substitute(newNode, oldNode);}

  #ifdef DEBUG
	TreeImpl::verify;
  #endif
};


//
// A sorted balanced binary tree with nodes of class Node, which must be a subclass of
// TreeNode<Node>.  Each Node has a key of class Key.
//
// Each Key must support the following methods:
//
//   Key(const Key &);
//   void operator=(const Key &);
//     Copy constructor and assignment.
//
//   bool operator<(const Key &key2) const;
//     Comparisons.  These comparisons must induce a full order on all Keys.  In particular:
//       For any key key1,  key1<key1  must be false;
//       At most one of  key1<key2  or  key2<key1  can be true;
//       If  key1<key2  and  key2<key3  then  key1<key3  must be true;
//       If  !(key1<key2)  and  !(key2<key3)  then  key1<key3  must be false.
//
// In addition to deriving from TreeNode<Node>, each Node must support the following methods:
//
//   Key getKey() const   or
//   const Key &getKey() const;
//     Returns the node's key.
//
// Find, insert, remove, and substitute operations are provided.  The SortedTree does not
// do storage management (allocation or deallocation) of its nodes; the user should do so
// explicitly if needed.
//
template<class Node, class Key>
class SortedTree: public Tree<Node>
{
  #ifdef DEBUG
	static bool isBetween(Node &node, Node *prev, Node *next);
	bool immediatelyAfter(Node &node, Node *prev) const;
	bool immediatelyBefore(Node &node, Node *next) const;
  #endif
  public:
	Node *find(Key key) const;
	Node *find(Key key, Node *&where, bool &right) const;
	Node *findAfter(Key key) const;
	Node *findBefore(Key key) const;

	void attach(Node &node, Node *where, bool right);
	void insert(Node &node);
	void insertAfter(Node &node, Node *where) {assert(immediatelyAfter(node, where)); Tree<Node>::insertAfter(node, where);}
	void insertBefore(Node &node, Node *where) {assert(immediatelyBefore(node, where)); Tree<Node>::insertBefore(node, where);}
	void substitute(Node &newNode, Node &oldNode);

  #ifdef DEBUG
	void verify() const;
  #endif
};


// --- TEMPLATES --------------------------------------------------------------

#ifdef DEBUG
//
// Return true if key(prev) <= key(node) <= key(next), where comparisons against a nil
// node's key always succeed.
//
template<class Node, class Key>
inline bool SortedTree<Node, Key>::isBetween(Node &node, Node *prev, Node *next)
{
	return !(prev && node.getKey() < prev->getKey() || next && next->getKey() < node.getKey());
}

//
// Return true if node can be inserted immediately after prev
// without violating the key ordering of the tree.
//
template<class Node, class Key>
bool SortedTree<Node, Key>::immediatelyAfter(Node &node, Node *prev) const
{
	return isBetween(node, prev, next(prev));
}


//
// Return true if node can be inserted immediately before next
// without violating the key ordering of the tree.
//
template<class Node, class Key>
bool SortedTree<Node, Key>::immediatelyBefore(Node &node, Node *next) const
{
	return isBetween(node, prev(next), next);
}
#endif

//
// Find and return a node that contains the given key.  If there is no such node,
// return nil.  If there is more than one node with the given key in the tree,
// pick one arbitrarily.
//
template<class Node, class Key>
Node *SortedTree<Node, Key>::find(Key key) const
{
	Node *p = getRoot();
	while (p)
		if (key < p->getKey())
			p = p->getChild(false);
		else if (p->getKey() < key)
			p = p->getChild(true);
		else
			break;
	return p;
}


//
// Find and return a node that contains the given key.  If there is no such node,
// return nil and store a node in where and a flag in right that can be passed to
// attach to add a node with the given key to the tree.  If there is more than one
// node with the given key in the tree, pick one arbitrarily.
// where and right are undefined if the node has been found.
//
template<class Node, class Key>
Node *SortedTree<Node, Key>::find(Key key, Node *&where, bool &right) const
{
	Node *p = getRoot();
	Node *parent = 0;
	bool rightChild = false;
	while (p) {
		if (key < p->getKey())
			rightChild = false;
		else if (p->getKey() < key)
			rightChild = true;
		else
			return p;
		parent = p;
		p = p->getChild(rightChild);
	}
	where = parent;
	right = rightChild;
	return 0;
}


//
// Find and return a node that contains the given key.  If there is no such node,
// return the tree node with the lowest key greater than the given key; if there
// still is no such node (i.e. the given key is greater than the keys of all nodes
// in the tree), return nil.  If there is more than one node with the given key in
// the tree, pick one arbitrarily.
//
template<class Node, class Key>
Node *SortedTree<Node, Key>::findAfter(Key key) const
{
	Node *where;
	bool right;
	Node *p = find(key, where, right);
	if (!p && where) {
		assert(!where->getChild(right));
		if (right)
			p = where->next();
		else
			p = where;
	}
	return p;
}


//
// Find and return a node that contains the given key.  If there is no such node,
// return the tree node with the greatest key less than the given key; if there
// still is no such node (i.e. the given key is lower than the keys of all nodes
// in the tree), return nil.  If there is more than one node with the given key in
// the tree, pick one arbitrarily.
//
template<class Node, class Key>
Node *SortedTree<Node, Key>::findBefore(Key key) const
{
	Node *where;
	bool right;
	Node *p = find(key, where, right);
	if (!p && where) {
		assert(!where->getChild(right));
		if (right)
			p = where;
		else
			p = where->prev();
	}
	return p;
}


//
// Attach the node at the position (where, right) returned by find.
//
template<class Node, class Key>
inline void SortedTree<Node, Key>::attach(Node &node, Node *where, bool right)
{
	assert(right ? immediatelyAfter(node, where) : immediatelyBefore(node, where));
	Tree<Node>::attach(node, where, right);
}


//
// Insert node into the proper place in the tree.  Rebalance the tree as needed.
// If there already are nodes with an equal key in the tree, the new node will be
// inserted arbitrarily either before, after, or between such nodes.
// node should not be part of any tree on entry.
//
template<class Node, class Key>
void SortedTree<Node, Key>::insert(Node &node)
{
	Node *where;
	bool right;
	Node *p = find(node.getKey(), where, right);
	if (p)
		insertAfter(node, p);
	else
		attach(node, where, right);
}


//
// Splice newNode into oldNode's position in the tree.  oldNode is removed from
// the tree.
//
template<class Node, class Key>
inline void SortedTree<Node, Key>::substitute(Node &newNode, Node &oldNode)
{
	assert(isBetween(newNode, oldNode.prev(), oldNode.next()));
	Tree<Node>::substitute(newNode, oldNode);
}


#ifdef DEBUG
//
// Verify that the tree is internally consistent and that the entries in it are
// still sorted.  Assert if anything wrong is found.
//
template<class Node, class Key>
void SortedTree<Node, Key>::verify() const
{
	Tree<Node>::verify();
	Node *p = firstNode();
	if (p) {
		Node *q;
		while ((q = p->next()) != 0) {
			assert(!(q->getKey() < p->getKey()));
			p = q;
		}
	}
}
#endif
#endif
