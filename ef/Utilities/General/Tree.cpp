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

#include "Tree.h"


// ----------------------------------------------------------------------------
// TreeNodeImpl


//
// Copy the link data from the src node to this node.  On entry this node must not
// be linked to a tree, while src must be linked to a tree; on exit the opposite
// will be true.
//
// CAUTION: Do not call this directly; call TreeImpl::substitute or one of its
// derivatives instead.
//
inline void TreeNodeImpl::move(TreeNodeImpl &src)
{
	assert(!linked && src.linked);
	*this = src;
	src.unlink();
}


//
// Let s be the subtree with this node as the root.  Return the first (if right
// is false) or last (if right is true) node of this subtree.
//
TreeNodeImpl &TreeNodeImpl::extremeNode(bool right)
{
	TreeNodeImpl *p;
	TreeNodeImpl *subtree = this;

	while ((p = subtree->getChild(right)) != 0)
		subtree = p;
	return *subtree;
}


//
// Return the node in the tree immediately before this node or nil if this
// node is the first in the tree.
//
TreeNodeImpl *TreeNodeImpl::prev()
{
	TreeNodeImpl *child = getChild(false);
	if (child)
		return &child->extremeNode(true);
	TreeNodeImpl *node = this;
	while (node && node->isLeft())
		node = node->getParent();
	if (node)
		node = node->getParent();
	return node;
}


//
// Return the node in the tree immediately after this node or nil if this
// node is the last in the tree.
//
TreeNodeImpl *TreeNodeImpl::next()
{
	TreeNodeImpl *child = getChild(true);
	if (child)
		return &child->extremeNode(false);
	TreeNodeImpl *node = this;
	while (node && node->isRight())
		node = node->getParent();
	if (node)
		node = node->getParent();
	return node;
}


// ----------------------------------------------------------------------------
// TreeImpl


//
// Link a node to its new parent on the right side (if right is true) or left side
// if (right is false), replacing the parent's previous child link, if any, on that
// side.  A nil node indicates that the parent's previous child link should be cleared.
// A nil parent represents the root, in which case right is ignored.
//
void TreeImpl::linkNode(TreeNodeImpl *node, TreeNodeImpl *parent, bool right)
{
	if (node) {
		node->setParent(parent);
		if (parent)
			node->setRight(right);
		else
			node->setLeft();
	}
	if (parent)
		parent->setChild(right, node);
	else
		root = node;
}


//
// Rotate a node pair to the right (if right is true) or left (if right is false)
// without changing their colors.  The given node starts at the parent of its
// partner and ends as the child of its partner.
//
void TreeImpl::rotate(TreeNodeImpl &node, bool right)
{
	TreeNodeImpl *partner = node.getChild(!right);
	assert(partner);
	linkNode(partner->getChild(right), &node, !right);
	linkNode(partner, node.getParent(), node.isRight());
	linkNode(&node, partner, right);
}


//
// Attach a new red node and link it with the parent on the right side (if right is
// true) or left side if (right is false).  The given node must not be already linked
// into a tree.
// A nil parent represents the root.
//
void TreeImpl::addNode(TreeNodeImpl &node, TreeNodeImpl *parent, bool right)
{
	assert(!(parent ? parent->getChild(right) : root));
	node.link();
	node.setRed();
	node.setChild(false, 0);
	node.setChild(true, 0);
	linkNode(&node, parent, right);
	nNodes++;
}


//
// Make node be the right (if right is true) or left (if right is false) child of
// parent, which must be in the tree.  That child must be currently nil.
// If where is nil, the tree must be currently empty, and in this case make node
// be the new root, ignoring the value of right.
// Rebalance the tree as needed.  node should not be part of any tree on entry.
//
void TreeImpl::attach(TreeNodeImpl &node, TreeNodeImpl *parent, bool right)
{
	addNode(node, parent, right);

	// Rebalance the tree to make sure that we don't have two red nodes in a row.
	TreeNodeImpl *p = &node;
	while (true) {
		// If we are the root, we make this node black and we're done.
		if (!parent) {
			p->setBlack();
			break;
		}

		// If we're black, we're done.
		if (parent->isBlack())
			break;

		// Otherwise we need a more complex rebalance.
		right = p->isRight();
		TreeNodeImpl *grandparent = parent->getParent();
		assert(grandparent);
		bool parentRight = parent->isRight();
		TreeNodeImpl *uncle = grandparent->getChild(!parentRight);
		if (uncle && uncle->isRed()) {
			parent->setBlack();
			uncle->setBlack();
			grandparent->setRed();
			p = grandparent;
		} else {
			if (right != parentRight) {
				rotate(*parent, parentRight);
				p = parent;
				parent = p->getParent();
			}
			parent->setBlack();
			grandparent->setRed();
			rotate(*grandparent, !parentRight);
			break;
		}
		parent = p->getParent();
	}
}


//
// Insert node into the tree after node where.  If where is nil, make node be the first
// node in the tree.  Rebalance the tree as needed.
// node should not be part of any tree on entry.
//
void TreeImpl::insertAfter(TreeNodeImpl &node, TreeNodeImpl *where)
{
	TreeNodeImpl *child = where ? where->getChild(true) : root;
	if (child)
		attach(node, &child->extremeNode(false), false);
	else
		attach(node, where, true);
}


//
// Insert node into the tree before node where.  If where is nil, make node be the last
// node in the tree.  Rebalance the tree as needed.
// node should not be part of any tree on entry.
//
void TreeImpl::insertBefore(TreeNodeImpl &node, TreeNodeImpl *where)
{
	TreeNodeImpl *child = where ? where->getChild(false) : root;
	if (child)
		attach(node, &child->extremeNode(true), true);
	else
		attach(node, where, false);
}


//
// Remove the node from the tree, rebalancing the tree as needed.
//
void TreeImpl::remove(TreeNodeImpl &node)
{
	// If the node has both children, exchange node with node's predecessor
	// in the tree ordering, which is guaranteed not to have a right child.
	if (node.getChild(false) && node.getChild(true)) {
		TreeNodeImpl &prev = node.getChild(false)->extremeNode(true);
		// Exchange the link fields of prev and node.
		TreeNodeImpl nodeCopy = node;
		TreeNodeImpl prevCopy = prev;

		node.setRed(prevCopy.isRed());
		prev.setRed(nodeCopy.isRed());
		if (prevCopy.getParent() == &node)
			linkNode(&node, &prev, false);
		else {
			linkNode(&node, prevCopy.getParent(), prevCopy.isRight());
			linkNode(nodeCopy.getChild(false), &prev, false);
		}
		linkNode(&prev, nodeCopy.getParent(), nodeCopy.isRight());
		linkNode(nodeCopy.getChild(true), &prev, true);
		linkNode(prevCopy.getChild(false), &node, false);
		node.setChild(true, 0);
		// Quiet the assert about a linked node being deleted.
		nodeCopy.unlink();
		prevCopy.unlink();
	}

	// At this point node has no more than one child.  Let p be that child
	// or nil if node has no children.
	assert(!(node.getChild(false) && node.getChild(true)));
	TreeNodeImpl *p = node.getChild(false);
	if (!p)
		p = node.getChild(true);

	// Link node's child directly to node's parent, bypassing node.
	bool red = node.isRed();
	bool right = node.isRight();
	TreeNodeImpl *parent = node.getParent();
	linkNode(p, parent, right);
	nNodes--;
	node.unlink();

	// If we just deleted a black node, we need to rebalance the tree.
	if (!red) {
		while (!p || p->isBlack()) {
			if (!parent)
				break;

			TreeNodeImpl *brother = parent->getChild(!right);
			assert(brother);
			if (brother->isRed()) {
				brother->setRed(parent->isRed());
				parent->setRed();
				rotate(*parent, right);
				brother = parent->getChild(!right);
			}

			TreeNodeImpl *cousin = brother->getChild(!right);
			if (cousin && cousin->isRed()) {
				cousin->setBlack();
				brother->setRed(parent->isRed());
				parent->setBlack();
				rotate(*parent, right);
				break;
			}

			cousin = brother->getChild(right);
			if (cousin && cousin->isRed()) {
				brother->setRed();
				cousin->setBlack();
				rotate(*brother, !right);
			} else {
				brother->setRed();
				p = parent;
				right = p->isRight();
				parent = p->getParent();
			}
		}
		if (p)
			p->setBlack();
	}
}


//
// Splice newNode into oldNode's position in the tree.  oldNode is removed from
// the tree.
//
void TreeImpl::substitute(TreeNodeImpl &newNode, TreeNodeImpl &oldNode)
{
	if (&newNode != &oldNode) {
		newNode.move(oldNode);

		// Change the parent's pointer from oldNode to newNode
		TreeNodeImpl *parent = newNode.getParent();
		if (parent)
			parent->setChild(newNode.isRight(), &newNode);
		else
			root = &newNode;

		// Change the children's parent pointers from oldNode to newNode
		TreeNodeImpl *child = newNode.getChild(false);
		if (child)
			child->setParent(&newNode);
		child = newNode.getChild(true);
		if (child)
			child->setParent(&newNode);
	}
}


#ifdef DEBUG
//
// Verify the integrity of a subtree rooted at the given node.  Assert if
// anything wrong is found.
// Increment nNodes by the number of nodes encountered in the subtree.
// Return the number of non-nil black nodes encountered on any path from node
// to a leaf (this number must be constant regardless of the path and leaf
// chosen) in blackDepth.  Return true if this node is red.
//
bool TreeImpl::verifySubtree(TreeNodeImpl *node, TreeNodeImpl *parent, bool rightChild, Uint32 &nNodes, Uint32 &blackDepth)
{
	if (node) {
		nNodes++;
		assert(node->getParent() == parent && node->isRight() == rightChild);
		Uint32 leftChildDepth;
		Uint32 rightChildDepth;
		bool red = node->isRed();
		bool hasRedChild = verifySubtree(node->getChild(false), node, false, nNodes, leftChildDepth);
		hasRedChild |= verifySubtree(node->getChild(true), node, true, nNodes, rightChildDepth);
		assert(leftChildDepth == rightChildDepth);
		if (red)
			assert(!hasRedChild);
		else
			leftChildDepth++;
		blackDepth = leftChildDepth;
		return red;
	} else {
		blackDepth = 0;
		return false;
	}
}


//
// Verify that the tree is internally consistent.  Assert if anything wrong is found.
//
void TreeImpl::verify() const
{
	Uint32 n = 0;
	Uint32 blackDepth;
	bool rootIsRed = verifySubtree(root, 0, false, n, blackDepth);
	assert(!rootIsRed && n == nNodes);
}
#endif
