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

#include "Fundamentals.h"
#include "CGScheduler.h"
#include "ControlGraph.h"
#include "InstructionEmitter.h"
#include "GraphUtils.h"


inline static bool 
isPredecessor(ControlNode& pred, ControlNode& node)
{
  ControlEdge* limit = pred.getSuccessorsEnd();

  for (ControlEdge* ptr = pred.getSuccessorsBegin(); ptr < limit; ptr++)
	if (&ptr->getTarget() == &node)
	  return true;
  return false;
}

ControlNode** ControlGraphScheduler::
scheduleNodes()
{
  ControlNode** orderedNodes = orderNodes();
  addAbsoluteBranches(orderedNodes);

  return orderedNodes;
}

// addAbsoluteBranches
// Given an ordering of ControlNodes in the graph add necessary
// implicit branches demanded by the ordering (ie to satisfy the ControlEdges)
void ControlGraphScheduler::
addAbsoluteBranches(ControlNode** orderedNodes)
{
  Uint32 nNodes = cg.nNodes;

  // for each ControlNode in orderedNodes, determine for each sequential pairing
  // if a falls through to b then don't add an absolute branch from a->b, otherwise
  // if the edge a->b does not exist, then add an absolute branch to where a should
  // connect.
  for (Uint32 n = 0; n < nNodes; n++)
	{
	  ControlNode* thisNode = orderedNodes[n];
	  ControlNode* nextNode = (n < (nNodes - 1)) ? orderedNodes[n+1] : (ControlNode *) 0;
	  ControlEdge* branchEdge;

	  switch(thisNode->getControlKind())
		{
		case ckIf:
		case ckBlock:
		case ckExc:
		case ckAExc:
		case ckCatch:
		case ckBegin:
		  branchEdge = &thisNode->nthSuccessor(0);
		  break;

		// these three ControlNodes never have any implicit branches
		case ckSwitch:
		case ckReturn:
		case ckEnd:
		case ckThrow:
		  branchEdge = NULL;
		  break;

		default:
		  assert(false);
		}

	  if (branchEdge != NULL && nextNode != NULL && &branchEdge->getTarget() != nextNode)
		emitter.pushAbsoluteBranch(branchEdge->getSource(), branchEdge->getTarget());
	}
}

ControlNode** ControlGraphScheduler::
orderNodes()
{
  Uint32 nNodes = cg.nNodes - 1;
  Uint32 myGeneration = ControlNode::getNextGeneration();
  ControlNode** nodes = new ControlNode*[nNodes];
  ControlNode** n_ptr = nodes;

  SearchStackEntry<ControlEdge> *ceStack = new SearchStackEntry<ControlEdge>[nNodes];
  SearchStackEntry<ControlEdge> *ceSp = ceStack;
  
  ControlEdge beginEdge;
  beginEdge.setTarget(cg.getBeginNode());
  cg.getEndNode().generation = myGeneration;

  ControlEdge* n = &beginEdge;
  ControlEdge* l = &beginEdge + 1;

  while(true)
	{
	  if (n == l)
		{
		  if (ceSp == ceStack)
			break;
		  --ceSp;
		  n = ceSp->next;
		  l = ceSp->limit;
		}
	  else
		{
		  ControlNode& node = n++->getTarget();
		  if (node.generation != myGeneration)
			{
			  node.generation = myGeneration;
			  node.schedulePos = Uint32(n_ptr - nodes);
			  *n_ptr++ = &node;
			  ceSp->next = n;
			  ceSp->limit = l;
			  ceSp++;
			  n = node.getSuccessorsBegin();
			  l = node.getSuccessorsEnd();
			}
		}
	}
#ifndef WIN32 // ***** Visual C++ has a bug in the code for delete[].
  delete ceStack;
#endif

  Uint32* cnStack = new Uint32[nNodes];
  Uint32* cnSp = cnStack;

  ControlNode** orderedNodes = new(cg.pool) ControlNode*[nNodes + 1];
  n_ptr = orderedNodes;
  Uint32 next;

#if DEBUG
  fill_n(orderedNodes, nNodes, (ControlNode *) 0);
#endif

  myGeneration = ControlNode::getNextGeneration();
  orderedNodes[nNodes] = &cg.getEndNode();
  orderedNodes[nNodes]->generation = myGeneration;
  *n_ptr++ = nodes[0];
  *cnSp++ = 0;
  next = 1;

  while (true)
	{
	  if (next == 0)
		{
		  if (cnSp == cnStack)
			break;
		  next = *--cnSp;
		}
	  else
		{
		  ControlNode& node = *nodes[next];

		  if (node.generation == myGeneration)
			{
			  next = 0;
			}
		  else
			{
			  // if (node.hasControlKind(ckIf))
			  const DoublyLinkedList<ControlEdge>& edges = node.getPredecessors();
			  if (!edges.done(edges.advance(edges.begin()))) // more than one predecessor
				{
				  for (DoublyLinkedList<ControlEdge>::iterator i = edges.begin(); !edges.done(i); i = edges.advance(i))
					{
					  ControlNode& source = edges.get(i).getSource();
					  if ((source.dfsNum > node.dfsNum) && (source.generation != myGeneration))
						{
						  *cnSp++ = next;
						  next = source.schedulePos;
						  while (isPredecessor(*nodes[next - 1], *nodes[next]) && 
								 (nodes[next - 1]->generation != myGeneration) && (next != node.schedulePos))
							next--;
						  break;
						}
					}
				  if (next == node.schedulePos)
					{
					  node.generation = myGeneration;
					  *n_ptr++ = &node;
					  if (++next >= nNodes)
						next = 0;
					}					  
				}
			  else
				{
				  node.generation = myGeneration;
				  *n_ptr++ = &node;
				  if (++next >= nNodes)
					next = 0;
				}
			}

		}
	}
  //delete cnStack;
  //delete [] nodes;

#if DEBUG
  for (Uint32 i = 0; i < nNodes + 1; i++)
	assert(orderedNodes[i]);
#endif

  return orderedNodes;
}

