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
//  LinearInstructionScheduler.cpp
//
//  Peter DeSantis   28 April 1997

#include "Scheduler.h"
#include "Vector.h"
#include "ControlNodes.h"
#include "Primitives.h"
#include "DoublyLinkedList.h"
#include "Instruction.h"

void  swapRootToEnd(Vector<RootPair>& inRoots, RootPair* inSwapToEnd);


//  schedule [no real purpose currently] FIX-ME
//  Calls the appropriate internal routine(s).
void LinearInstructionScheduler:: 
schedule(Vector<RootPair>& roots, ControlNode& controlNode)
{	
    linearSchedule(roots, controlNode);
}


// The primary goal of linear scheduling is to schedule all the instructions
//  which are derived from one line of src code one after another.  There are numerous 
//  ways to meet this objective.  The simpliest method is to do a simple prioritived 
//  topological sort of the control node.  The priority is that from any node the 
//  algorith follows the edges in increasing linenumber order.  (Each instruction has
//  a line number associated with it which corresponds to src code line number.)  As the 
//  topological sort backtracks it inserts the instruction which it is leaving into 
//  the list of instructions in the control node.  The instructions have to be inserted 
//  in an ordered fashion such that each new instruction is added at the end of the 
//  section with the same associated line numbers.  Currently this is done in the 
//  simpliest n^2 fashion.  If scheduling is determined to take significant time this 
//  running time can be improved by using a binary search insertion routine.  



/*
  priorityTopoEmit
  does a priority topological sort of the instructions rooted at root and fills the 
  controlnode's list ordered list of instructions.
*/
void LinearInstructionScheduler:: 
priorityTopoEmit(Instruction* inInstruction, ControlNode& controlNode)
{
  if (controlNode.haveScheduledInstruction(*inInstruction))
      return;

  InstructionUse* conditionCodeUse = NULL;
  InstructionUse* curUse;
  Instruction* curInstruction;

  for (curUse = inInstruction->getInstructionUseBegin();
       curUse < inInstruction->getInstructionUseEnd();
       curUse++)
    {
      if(curUse->kind == udCond)
        {
          assert(conditionCodeUse == NULL);   // Instructions only allowed to use one condition code
          conditionCodeUse = curUse;
        } else {
          curInstruction = CodeGenerator::instructionUseToInstruction(*curUse);

		  // Only need to schedule if it hasn't been scheduled and it is in coltrolNode
          if (curInstruction != NULL 
              && curInstruction->getPrimitive()->getContainer() == inInstruction->getPrimitive()->getContainer())
            {
              priorityTopoEmit(curInstruction,  controlNode);
            } 	      
        }
    }
  
  if(conditionCodeUse != NULL)
    {
      curInstruction = CodeGenerator::instructionUseToInstruction(*conditionCodeUse);
      
      if (curInstruction != NULL
          && curInstruction->getPrimitive()->getContainer() == inInstruction->getPrimitive()->getContainer())
        {
          priorityTopoEmit(curInstruction,  controlNode);
        } 	      
    }
 
  // Now insert the instruction into the controlnodes list of instructions
  controlNode.addScheduledInstruction(*inInstruction);

}


// swapRootToEnd
//
// Swaps the passed in root to the end of the root vector
void 
swapRootToEnd(Vector<RootPair>& inRoots, RootPair* inSwapToEnd)
{
	RootPair	oldRoot;
	RootPair*	end = inRoots.end() - 1;

	oldRoot = *end;
	*end = *inSwapToEnd;
	*inSwapToEnd = oldRoot;
}

/* 
   linearSchedule 
   Does a prioritized linear search which fills in controlNode->instructions from the
   back to the front.
*/
void LinearInstructionScheduler::
linearSchedule(Vector<RootPair>& roots, ControlNode& controlNode)
{
	// FIX-ME move this stuff to the function that builds up roots
	if (controlNode.hasControlKind(ckReturn))
	{
		// Enforce the register allocator requirement that code emitted for 
		// Result nodes be placed at the end of the block
		// The register allocator has no "global scope" uses, so basically it
		// would be sufficient to place the ExternalUse instruction emitted for
		// each Result node at the end, however we just go ahead and make sure
		// all of the code for a Result is last thing scheduled in a node.
		// Hopefully the register allocator will have globally scoped uses
		// in the future.  FIX-ME.
		RootPair* curRoot;
		RootPair* resultRoot = NULL;

		// find poResult
		for (curRoot = roots.begin(); curRoot < roots.end(); curRoot++)
			if (curRoot->root->hasCategory(pcResult))
			{
				assert(!resultRoot);	// verify there is only one Result node (is this true?)
				resultRoot = curRoot;
#ifndef DEBUG
				break;
#endif
			}

		// it is not necessary that there be a pcResult
		if (resultRoot)
			swapRootToEnd(roots, resultRoot);
	}
	else if (controlNode.hasControlKind(ckExc))
	{
		// Satisfy constraint that an exception producing primitive
		// must be the last excecuted in the ControlNode
		RootPair* curRoot;
		RootPair* exceptionRoot = NULL;
 
		// find poResult
		for (curRoot = roots.begin(); curRoot < roots.end(); curRoot++)
			if (curRoot->root->canRaiseException())
			{
				assert(!exceptionRoot);	// verify there is only one exception generating root
				exceptionRoot = curRoot;
#ifndef DEBUG
				break;
#endif
			}

		assert(exceptionRoot);
		swapRootToEnd(roots, exceptionRoot);	  
	}

	renumberCN(roots, controlNode);

	RootPair*	curRoot;
	Primitive*	anchoredPrimitive;

	// take care of any anchored primitives last, so find out the anchored primitive
	if (controlNode.hasControlKind(ckIf) || controlNode.hasControlKind(ckSwitch))
		anchoredPrimitive = &controlNode.getControlPrimExtra(); 
	else
		anchoredPrimitive = NULL;

	  //For each primary root, rum topologicalEmit
	for(curRoot = roots.begin(); curRoot < roots.end(); curRoot++)
	{
		if(curRoot->isPrimary && (curRoot->root != anchoredPrimitive))
		{
			if (curRoot->root->getInstructionRoot() != NULL)
				priorityTopoEmit(curRoot->root->getInstructionRoot(), controlNode);

			// FIX-ME no longer need curOutput
			DataNode*	curOutput = curRoot->root;
		  
			// although exception edges are not explicit
			// we need to consider them as real outgoing edges
			if ((curOutput->hasConsumers() || curOutput->canRaiseException()) && !curOutput->hasKind(vkVoid))
			{
				// now walk through all vr's assigned to this producer
				VirtualRegister* 	curVR;
				Instruction*		nextInsn;

				switch (curOutput->getKind())
				{
					case vkCond:
					case vkMemory:
						nextInsn = curOutput->getInstructionAnnotation();
						if (nextInsn)
							priorityTopoEmit(nextInsn, controlNode);
						break;
					case vkLong:
						curVR = curOutput->getHighVirtualRegisterAnnotation();
						assert(curVR);
						nextInsn = curVR->getDefiningInstruction();
						if (nextInsn)
							priorityTopoEmit(nextInsn, controlNode);
						// FALL THROUGH for low vr
					case vkAddr:	
					case vkInt:
					case vkFloat:
					case vkDouble:
						curVR = curOutput->getLowVirtualRegisterAnnotation();
						assert(curVR);
						nextInsn = curVR->getDefiningInstruction();
						if (nextInsn)
							priorityTopoEmit(nextInsn, controlNode);
						break;
					case vkTuple:
					case vkVoid:
						break;
					default:
						trespass("unknown or unhandled output");
				} 				
			}
		}
	}

	if (anchoredPrimitive)
		priorityTopoEmit(anchoredPrimitive->getInstructionRoot(), controlNode);
}


/*
  Compute the debugLineNumbers for each Primitive in the CN.
  The debugLineNumber assures that the stable sort will not invalidate the scheduled instruction
  ordering.  Debug line numbers are the minimum integer values which  maintain the following 
  properties: lineNumber(P) <= debugLineNumber(P) and for every Primitive P' which 
  defines an edge consumed by P, debugLineNumber(P) >= debugLineNumber(P').  
*/

void LinearInstructionScheduler::
renumberCN(Vector<RootPair>& roots, ControlNode& /*controlNode*/)
{
  
  RootPair* curRoot;

  int rootsToRenumber = roots.size();
  int rootsRenumbered = 0;
  
  const DoublyLinkedList<DataConsumer>* consumers;
  DoublyLinkedList<DataConsumer> :: iterator curConsumer;

  // Initialize all root's renumberData
  for(curRoot = roots.begin(); curRoot < roots.end(); curRoot++)
    {
      if(curRoot->isPrimary)
		  curRoot->data.renumbered = false;
      else {
          curRoot->data.renumbered = false;
          curRoot->data.timesVisited = 0;
          curRoot->data.neededVisits = 0; 
          
          if (curRoot->root->getOutgoingEdgesEnd() > curRoot->root->getOutgoingEdgesBegin())
            {
              DataNode*	curOutput;
              for (curOutput = curRoot->root->getOutgoingEdgesBegin(); 
                   curOutput < curRoot->root->getOutgoingEdgesEnd();
                   curOutput++)
                {
                  consumers = &curOutput->getConsumers();
                  for(curConsumer = consumers->begin();  !consumers->done(curConsumer); 
                      curConsumer = consumers->advance(curConsumer))
                    curRoot->data.neededVisits++; 
                }
            }
        }
    }
  
  // Renumber Primary Roots
  for(curRoot = roots.begin(); curRoot < roots.end(); curRoot++)
    {
      if(curRoot->isPrimary)
        {
          curRoot->root->setDebugLineNumber(curRoot->root->getLineNumber());
          renumberPrimitive(roots, *(curRoot->root));
          rootsRenumbered++;
          curRoot->data.renumbered = true;
        }
    }
  
  curRoot = roots.begin();
  while ( rootsRenumbered != rootsToRenumber )
    {
      while(curRoot->data.renumbered)
        {
          curRoot++;
          if(curRoot == roots.end())
              curRoot = roots.begin();
        }
      
      if(curRoot->data.timesVisited != curRoot->data.neededVisits)
        {
          renumberPrimitive(roots, *(curRoot->root));
          rootsRenumbered++;
          curRoot->data.renumbered = true;
        }
      
      curRoot++;
      if(curRoot == roots.end())
        curRoot = roots.begin();
    }
}


void LinearInstructionScheduler::
renumberPrimitive(Vector<RootPair>& roots, Primitive& p)
{
	DataConsumer* curConsumer;
  
	for (curConsumer = p.getInputsBegin(); 
	   curConsumer < p.getInputsEnd();
	   curConsumer++)
	{
		if(curConsumer->isVariable())
		{
			DataNode&	possibleChild = curConsumer->getNode();
			
			if (!possibleChild.hasCategory(pcPhi))
			{
				Primitive &child = Primitive::cast(possibleChild);
				if(child.getDebugLineNumber() == 0)
					child.setDebugLineNumber(child.getLineNumber());
		  
				if(child.getDebugLineNumber() < p.getDebugLineNumber())
					child.setDebugLineNumber(p.getDebugLineNumber());
		  
				RootPair* curRoot;
				bool isRoot = false;
				for(curRoot = roots.begin(); curRoot < roots.end(); curRoot++)
				{
					if(curRoot->root == &child)
					{
						isRoot = true;
						curRoot->data.timesVisited++;
						break;
					}
				}
		  
				if(!isRoot)
					renumberPrimitive(roots, child);
			}
		}       
	 }
}


