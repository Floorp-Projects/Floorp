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

#ifndef GRAPHUTILS_H
#define GRAPHUTILS_H

#include "Fundamentals.h"

template<class Successor>
struct SearchStackEntry
{
	Successor *next;							// Next child to be searched at this level of the graph
	Successor *limit;							// Last child+1 to be searched at this level of the graph
};


//
// Search and mark all reachable nodes of the directed graph with the given root.
// The graph must have no more than maxNNodes nodes reachable from the root.
// The stack argument must be a preallocated temporary array of maxNNodes entries of
// type SearchStackEntry<Successor>.
//
// The NodeRef class is a way to refer to nodes in the graph (NodeRefs may be
// pointers to nodes, node indices, etc.).
// The SearchParams is a helper class that graphSimpleSearch uses to access the
// contents of graph nodes.  SearchParams must support the following types and
// methods (static or dynamic):
//
//   typedef Successor;
//   typedef NodeRef;
//
//   Successor *getSuccessorsBegin(NodeRef n);
//   Successor *getSuccessorsEnd(NodeRef n);
//     Return the bounds of an array of Successors of node n.
//
//   NodeRef getNodeRef(Successor &s);
//     Return the node to which the Successor s refers.
//
//   bool isMarked(NodeRef n);
//   void setMarked(NodeRef n);
//     Each node in the graph is either marked or unmarked.
//     All nodes should be in the unmarked state when graphSimpleSearch is called;
//     graphSimpleSearch will not traverse any marked nodes it encounters.  The
//     setMarked method changes the state of a node to marked.  Every node
//     reachable from the root will be marked when graphSimpleSearch exits.
//     setMarked is only called on unmarked nodes.
//
template<class SearchParams, class Successor>
void graphSimpleSearch(SearchParams &searchParams, Successor root, Uint32 DEBUG_ONLY(maxNNodes), SearchStackEntry<Successor> *stack)
{
  #ifdef DEBUG
	SearchStackEntry<Successor> *stackEnd = stack + maxNNodes;
  #endif

	SearchStackEntry<Successor> *sp = stack;

	// Prepare to visit the root.
	Successor *n = &root;
	Successor *l = &root + 1;

	while (true) {
		if (n == l) {
			// We're done with all successors between n and l, so pop up one level.
			// Finish when we've marked the root.
			if (sp == stack)
				break;
			--sp;
			n = sp->next;
			l = sp->limit;

		} else {
			// We still have to visit more successors between n and l.  Mark the
			// next successor and advance n.
			typename SearchParams::NodeRef node = searchParams.getNodeRef(*n++);
			if (!searchParams.isMarked(node)) {
				// Mark the successor, saving the current place on the stack.
				searchParams.setMarked(node);
				assert(sp < stackEnd);
				sp->next = n;
				sp->limit = l;
				sp++;
				n = searchParams.getSuccessorsBegin(node);
				l = searchParams.getSuccessorsEnd(node);
			}
		}
	}
}


//
// Search, mark, and count all reachable nodes of the directed graph with
// the given root (which may be null).  The graph must have no more than
// maxNNodes nodes reachable from the root.  The stack argument must be a
// preallocated temporary array of maxNNodes entries of type
// SearchStackEntry<Successor>.
//
// This function returns the number of nodes reached, including the root.
//
// The NodeRef class is a way to refer to nodes in the graph (NodeRefs may be
// pointers to nodes, node indices, etc.).
// The SearchParams is a helper class that graphSearch uses to access the
// contents of graph nodes.  SearchParams must support the following types and
// methods (static or dynamic):
//
//   typedef Successor;
//   typedef NodeRef;
//
//   Successor *getSuccessorsBegin(NodeRef n);
//   Successor *getSuccessorsEnd(NodeRef n);
//     Return the bounds of an array of Successors of node n.
//
//   bool isNull(Successor &s);
//     Returns true if n is null.
//
//   NodeRef getNodeRef(Successor &s);
//     Return the node to which the Successor s refers.
//
//   bool isMarked(NodeRef n);
//   void setMarked(NodeRef n);
//     Each node in the graph is either marked or unmarked.
//     All nodes should be in the unmarked state when graphSearch is called;
//     graphSearch will not traverse any marked nodes it encounters.  The
//     setMarked method changes the state of a node to marked.  Every node
//     reachable from the root will be marked when graphSearch exits.
//     setMarked is only called on unmarked nodes.
//
//   void notePredecessor(NodeRef n);
//     This method is called on node n once for each time n is a successor
//     of any node or the root.  The SearchParams class may use this method to
//     count predecessors of node n.
//
template<class SearchParams, class Successor>
Uint32 graphSearch(SearchParams &searchParams, Successor &root, Uint32 DEBUG_ONLY(maxNNodes), SearchStackEntry<Successor> *stack)
{
  #ifdef DEBUG
	SearchStackEntry<Successor> *stackEnd = stack + maxNNodes;
  #endif

	Uint32 nNodes = 0;
	if (!searchParams.isNull(root)) {
		SearchStackEntry<Successor> *sp = stack;

		// Prepare to visit the root.
		Successor *n = &root;
		Successor *l = &root + 1;

		while (true) {
			if (n == l) {
				// We're done with all successors between n and l, so pop up one level.
				// Finish when we've marked the root.
				if (sp == stack)
					break;
				--sp;
				n = sp->next;
				l = sp->limit;

			} else {
				// We still have to visit more successors between n and l.  Mark the
				// next successor and advance n.
				typename SearchParams::NodeRef node = searchParams.getNodeRef(*n++);
				searchParams.notePredecessor(node);
				if (!searchParams.isMarked(node)) {
					// Mark the successor, saving the current place on the stack.
					nNodes++;
					searchParams.setMarked(node);
					assert(sp < stackEnd);
					sp->next = n;
					sp->limit = l;
					sp++;
					n = searchParams.getSuccessorsBegin(node);
					l = searchParams.getSuccessorsEnd(node);
				}
			}
		}
	}
	assert(nNodes <= maxNNodes);
	return nNodes;
}


//
// A specialized version of graphSearch for graphs with a designated end node.
// The end node must have no successors.  There may be other nodes with no successors.
// Unlike all other nodes, which must be reachable from the root, the end node does
// not have to be reachable from the root; if it's not reachable, it will have no
// predecessors.
//
// The end node may be the root node, in which case the graph consists of exactly
// one node.
//
// graphSearchWithEnd guarantees that the end node will be marked and counted,
// even if it is not reachable from the root.
//
template<class SearchParams, class Successor>
Uint32 graphSearchWithEnd(SearchParams &searchParams, Successor &root, Successor &end,
		Uint32 maxNNodes, SearchStackEntry<Successor> *stack)
{
	assert(!searchParams.isNull(end));
	typename SearchParams::NodeRef endNode = searchParams.getNodeRef(end);
	assert(searchParams.getSuccessorsBegin(endNode) == searchParams.getSuccessorsEnd(endNode));
	searchParams.setMarked(endNode);
	return 1 + graphSearch(searchParams, root, maxNNodes, stack);
}


//
// Perform a depth-first search of the directed graph with the given root
// (which may be null).  This search will assign a unique integer between 0
// and nNodes-1 to each node in the graph according to the graph's depth-
// first ordering (see [ASU86], page 661).  The graph must have exactly nNodes
// nodes reachable from the root.  The stack argument must be a preallocated
// temporary array of nNodes entries of type SearchStackEntry<Successor>.
//
// The NodeRef class is a way to refer to nodes in the graph (NodeRefs may be
// pointers to nodes, node indices, etc.).
// The DFSParams is a helper class that depthFirstSearch uses to access the
// contents of graph nodes.  DFSParams must support the following types and
// methods (static or dynamic):
//
//   typedef Successor;
//   typedef NodeRef;
//
//   Successor *getSuccessorsBegin(NodeRef n);
//   Successor *getSuccessorsEnd(NodeRef n);
//     Return the bounds of an array of Successors of node n.
//
//   bool isNull(Successor &s);
//     Returns true if n is null.
//
//   NodeRef getNodeRef(Successor &s);
//     Return the node to which the Successor s refers.
//
//   bool isUnvisited(NodeRef n);
//   bool isNumbered(NodeRef n);
//   void setVisited(NodeRef n);
//   void setNumbered(NodeRef n, Int32 i);
//     Each node in the graph is in one of three states:
//       unvisited, visited, or numbered.
//     All nodes should be in the unvisited state when depthFirstSearch is
//     called.  The setVisited and setNumbered methods change the state of
//     a node to visited or numbered; i is the node's depth-first ordering
//     index.  Every node will be numbered when depthFirstSearch exits.
//     setVisited is only called on an unvisited node.  setNumbered is only
//     called on an unvisited or visited node.
//     isUnvisited and isNumbered query the current state of a node.
//
//   void noteIncomingBackwardEdge(NodeRef n);
//     This method is called on node n once for each time n is the target
//     of any backward edge.  The DFSParams class may use this method to
//     determine if the graph contains cycles and which nodes are cycle
//     headers.
//
template<class DFSParams, class Successor>
void depthFirstSearch(DFSParams &dfsParams, Successor &root, Uint32 nNodes, SearchStackEntry<Successor> *stack)
{
  #ifdef DEBUG
	SearchStackEntry<Successor> *stackEnd = stack + nNodes;
  #endif

	if (!dfsParams.isNull(root)) {
		SearchStackEntry<Successor> *sp = stack;

		// Prepare to visit the root.
		Successor *n = &root;
		Successor *l = &root + 1;

		while (true) {
			if (n == l) {
				// We're done with all successors between n and l, so number the
				// source node (which is on the stack) and pop up one level.
				// Finish when we've marked the root.
				if (sp == stack)
					break;
				--sp;
				n = sp->next;
				l = sp->limit;
				dfsParams.setNumbered(dfsParams.getNodeRef(n[-1]), --nNodes);

			} else {
				// We still have to visit more successors between n and l.  Visit the
				// next successor and advance n.
				typename DFSParams::NodeRef node = dfsParams.getNodeRef(*n++);
				if (dfsParams.isUnvisited(node)) {
					// Visit the successor, saving the current place on the stack.
					dfsParams.setVisited(node);
					assert(sp < stackEnd);
					sp->next = n;
					sp->limit = l;
					sp++;
					n = dfsParams.getSuccessorsBegin(node);
					l = dfsParams.getSuccessorsEnd(node);
				} else
					// We have a cycle if we ever encounter a successor that has been visited
					// but not yet numbered.
					if (!dfsParams.isNumbered(node))
						dfsParams.noteIncomingBackwardEdge(node);
			}
		}
	}
	assert(nNodes == 0);
}


//
// A specialized version of depthFirstSearch for graphs with a designated end node.
// The end node must have no successors.  There may be other nodes with no successors.
// Unlike all other nodes, which must be reachable from the root, the end node does
// not have to be reachable from the root; if it's not reachable, it will have no
// predecessors.
//
// The end node may be the root node, in which case the graph consists of exactly
// one node.
//
// depthFirstSearchWithEnd guarantees that the end node will be assigned the number nNodes-1.
//
template<class DFSParams, class Successor>
void depthFirstSearchWithEnd(DFSParams &dfsParams, Successor &root, Successor &end,
		Uint32 nNodes, SearchStackEntry<Successor> *stack)
{
	assert(!dfsParams.isNull(end));
	typename DFSParams::NodeRef endNode = dfsParams.getNodeRef(end);
	assert(dfsParams.getSuccessorsBegin(endNode) == dfsParams.getSuccessorsEnd(endNode));
	dfsParams.setNumbered(endNode, --nNodes);
	depthFirstSearch(dfsParams, root, nNodes, stack);
}

#endif
