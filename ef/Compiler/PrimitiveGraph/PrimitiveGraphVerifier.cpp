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
#include "ControlGraph.h"
#include "GraphUtils.h"

#ifdef DEBUG

UT_DEFINE_LOG_MODULE(GraphVerifier);

//#define CARRY_ON_VERIFYING

#ifdef CARRY_ON_VERIFYING
#define VERIFY_ASSERT(cond) ( (!(cond)) ? UT_LOG(GraphVerifier, PR_LOG_ALWAYS, ("Verify failure - '%s' @ %s %d \n", #cond, __FILE__, __LINE__)) : 0 )
#else
#define VERIFY_ASSERT(cond) assert((cond))
#endif



void ControlGraph::validate()
{
    UT_LOG(GraphVerifier, PR_LOG_ALWAYS, ("\n*** Verifying control graph 0x%p\n", this));


    VERIFY_ASSERT(!controlNodes.empty());
    VERIFY_ASSERT(nNodes >= 2);

    dfsSearch();            // we use the dfs numbers to enforce the backward edge constraint, and as indices
                            // into handy-dandy tables of marks and reachingSets, thus...

    Uint32 *connectedSearchMark = new Uint32[nNodes];
    FastBitSet **reachingSet = new FastBitSet *[nNodes]; 
    
    
    // prove that there's only one begin and end node, and optionally one return node,
    // and that they're on the graph
    bool foundBeginNode = false, foundEndNode = false, foundReturnNode = false;
   	DoublyLinkedList<ControlNode>::iterator cni;
    Uint32 controlNodeCount = 0;
	for (cni = controlNodes.begin(); !controlNodes.done(cni); cni = controlNodes.advance(cni)) 
	{
		ControlNode &cn = controlNodes.get(cni);
        controlNodeCount++;
        connectedSearchMark[cn.dfsNum] = ControlNode::unmarked;          // prepare for the connected search below
        if (cn.hasControlKind(ckBegin)) {
            VERIFY_ASSERT(!foundBeginNode);         /* CONSTRAINT 'BEGIN1' */
            VERIFY_ASSERT(&cn == &beginNode);
            foundBeginNode = true;
        }
        if (cn.hasControlKind(ckEnd)) {
            VERIFY_ASSERT(!foundEndNode);           /* CONSTRAINT 'END1' */
            VERIFY_ASSERT(&cn == &endNode);
            foundEndNode = true;
        }
        if (cn.hasControlKind(ckReturn)) {
            VERIFY_ASSERT(!foundReturnNode);        /* CONSTRAINT 'RETURN1' */
            VERIFY_ASSERT(&cn == returnNode);
            foundReturnNode = true;
        }
    }
    VERIFY_ASSERT(foundBeginNode);          /* CONSTRAINT 'BEGIN1' */
    VERIFY_ASSERT(foundEndNode);            /* CONSTRAINT 'END1' */

    
    // following only the forward edges, mark every reached node, starting with the begin node
    ControlNode **stack = new ControlNode *[controlNodeCount];
    ControlNode **sp = stack;
    *sp++ = &beginNode;
    Uint32 markedCount = 0;
    while (sp != stack) {
        ControlNode *n = *--sp;
        connectedSearchMark[n->dfsNum] = 1;  // 'marked'
        markedCount++;
        for (ControlEdge *s = n->getSuccessorsBegin(); s != n->getSuccessorsEnd(); s++) {
            if (n->dfsNum < s->getTarget().dfsNum)  {   // a forward edge
                if (connectedSearchMark[s->getTarget().dfsNum] == ControlNode::unmarked) {
                    connectedSearchMark[s->getTarget().dfsNum] = ControlNode::unvisited;    
                                                        // so that this doesn't get pushed twice
                    *sp++ = &s->getTarget();
                    assert(sp < &stack[controlNodeCount]);
                }
            }
        }
    }
    // now assert that every node on the graph was marked
    if (markedCount != controlNodeCount)
        VERIFY_ASSERT(false);
	for (cni = controlNodes.begin(); !controlNodes.done(cni); cni = controlNodes.advance(cni)) 
	{
        ControlNode &cn = controlNodes.get(cni);
        VERIFY_ASSERT(connectedSearchMark[cn.dfsNum] == 1);            /* CONSTRAINT 'CONNECTED1' */
        connectedSearchMark[cn.dfsNum] = ControlNode::unmarked;   // prepare for the next search, below
    }

    // likewise, but starting with the end node and reach back through all the predecessor links
    markedCount = 0;
    sp = stack;
    *sp++ = &endNode;
    while (sp != stack) {
        ControlNode *n = *--sp;
        connectedSearchMark[n->dfsNum] = 1;  // 'marked'
        markedCount++;
        const DoublyLinkedList<ControlEdge> &nPredecessors = n->getPredecessors();
        for (DoublyLinkedList<ControlEdge>::iterator predi = nPredecessors.begin();
                    !nPredecessors.done(predi);
                    predi = nPredecessors.advance(predi)) {
            ControlEdge &e = nPredecessors.get(predi);
            if (connectedSearchMark[e.getSource().dfsNum] == ControlNode::unmarked) {
                connectedSearchMark[e.getSource().dfsNum] = ControlNode::unvisited;
                *sp++ = &e.getSource();
                assert(sp < &stack[controlNodeCount]);
            }
        }
    }
    // again, assert that every node was marked
    if (markedCount != controlNodeCount)
        VERIFY_ASSERT(false);
	for (cni = controlNodes.begin(); !controlNodes.done(cni); cni = controlNodes.advance(cni)) 
	{
        ControlNode &cn = controlNodes.get(cni);
        VERIFY_ASSERT(connectedSearchMark[cn.dfsNum] == 1);            /* CONSTRAINT 'CONNECTED2' */
        connectedSearchMark[cn.dfsNum] = ControlNode::unmarked;   // prepare for the next search, below
    }

    // we do a recursive search on all the forward, return and exception edges to spot cycles therein
	SearchStackEntry<ControlEdge> *searchStack = new SearchStackEntry<ControlEdge>[controlNodeCount];
	SearchStackEntry<ControlEdge> *stackEnd = searchStack + controlNodeCount;
	SearchStackEntry<ControlEdge> *esp = searchStack;

	// Prepare to visit the root.
	ControlEdge *n = beginNode.getSuccessorsBegin();
	ControlEdge *l = beginNode.getSuccessorsEnd();

	while (true) {
		if (n == l) {
			// We're done with all successors between n and l, so number the
			// source node (which is on the stack) and pop up one level.
			// Finish when we've marked the root.
			if (esp == searchStack)
				break;
			--esp;
			n = esp->next;
			l = esp->limit;
			connectedSearchMark[n[-1].getTarget().dfsNum] = 1;
		}
        else {
			// We still have to visit more successors between n and l.  Visit the
			// next successor and advance n.
            // But - only visit successors if the edge is a forward, return or exception edge
            if ((n->getSource().dfsNum < n->getTarget().dfsNum)
                    || (n->getSource().hasControlKind(ckReturn) && n->getTarget().hasControlKind(ckEnd))
                    || ((n->getSource().hasControlKind(ckExc)
                                    || n->getSource().hasControlKind(ckAExc) 
                                    || n->getSource().hasControlKind(ckThrow))
                        && (n->getTarget().hasControlKind(ckEnd) || n->getTarget().hasControlKind(ckCatch)))) {
			    ControlNode &node = n->getTarget();
                n++;
			    if (connectedSearchMark[node.dfsNum] == ControlNode::unmarked) {
				    // Visit the successor, saving the current place on the stack.
				    connectedSearchMark[node.dfsNum] = ControlNode::unvisited;
				    assert(esp < stackEnd);
				    esp->next = n;
				    esp->limit = l;
				    esp++;
				    n = node.getSuccessorsBegin();
				    l = node.getSuccessorsEnd();
			    } 
                else
                    VERIFY_ASSERT(connectedSearchMark[node.dfsNum] == 1);             /* CONSTRAINT 'CYCLES1' */
				
            }
            else
                n++;
		}
	}
#ifndef WIN32 // ***** Visual C++ has a bug in the code for delete[].
	delete[] searchStack;
#endif


    // in order to prove that every primtive's inputs are available we calculate
    // the reaching nodes for each node. Then if an input is not in the current node
    // we need simply ask if it's in a reaching node instead.


	for (cni = controlNodes.begin(); !controlNodes.done(cni); cni = controlNodes.advance(cni)) 
	{
        ControlNode &cn = controlNodes.get(cni);
        connectedSearchMark[cn.dfsNum] = ControlNode::unmarked;
    }

    sp = stack;
    *sp++ = &beginNode;
    reachingSet[beginNode.dfsNum] = new FastBitSet(controlNodeCount);
    while (sp != stack) {
        ControlNode *n = *--sp;
        reachingSet[n->dfsNum]->set(n->dfsNum);     // reaching ourselves is o.k. and this is the input to the successors
        connectedSearchMark[n->dfsNum] = 1;         // mark as having been processed
        for (ControlEdge *s = n->getSuccessorsBegin(); s != n->getSuccessorsEnd(); s++) {
            if (connectedSearchMark[s->getTarget().dfsNum] == ControlNode::unmarked) {  // not been processed, ever simply set it to the current
                reachingSet[s->getTarget().dfsNum] = new FastBitSet(*reachingSet[n->dfsNum]);
                connectedSearchMark[s->getTarget().dfsNum] = ControlNode::unvisited;
                *sp++ = &s->getTarget();
                assert(sp < &stack[controlNodeCount]);
            }
            else {
                FastBitSet *old = new FastBitSet(*reachingSet[s->getTarget().dfsNum]);
                *reachingSet[s->getTarget().dfsNum] &= *reachingSet[n->dfsNum];              // we intersect because we want pessimistic data
                if (*old != *reachingSet[s->getTarget().dfsNum]) {                   // if we have new data...
                    if (connectedSearchMark[s->getTarget().dfsNum] != ControlNode::unvisited) {     // ...and not already scheduled for visiting
                        connectedSearchMark[s->getTarget().dfsNum] = ControlNode::unvisited;   
                        *sp++ = &s->getTarget();                                             // then add it
                        assert(sp < &stack[controlNodeCount]);          
                    }
                }
                delete old;         // REMIND - better would've been an & operator that returned a 'changed' flag
            }
        }
    }
/*
	for (cni = controlNodes.begin(); !controlNodes.done(cni); cni = controlNodes.advance(cni)) 
	{
        ControlNode &cn = controlNodes.get(cni);
        fprintf(stderr, "For ControlNode N%d ", cn.dfsNum);
        reachingSet[cn.dfsNum]->printPretty(stderr);
    }
*/

    delete stack;

    // ask each control node to validate itself

	for (cni = controlNodes.begin(); !controlNodes.done(cni); cni = controlNodes.advance(cni)) 
	{
		ControlNode &cn = controlNodes.get(cni);
        cn.validate(reachingSet);
        delete reachingSet[cn.dfsNum];
    }

    delete reachingSet;
    delete connectedSearchMark;   

    UT_LOG(GraphVerifier, PR_LOG_ALWAYS, ("\n*** Verify succeeded \n\n", this));

    // REMIND - assert(foundReturnNode == (method != void_return)); how to get the return type ?
    // REMIND - assert that all successors/predecessors are in this graph ?

}

void ControlNode::validate(FastBitSet **reachingSet)
{
    // check that all contained primitives have this as their container and that all inputs
    // to each primtive are available (either precede their use in this node, or come from
    // a reaching node)
	DoublyLinkedList<Primitive>::iterator pi;

    if (!primitives.empty()) {
 	    for (pi = primitives.begin();
                            !primitives.done(pi);
                            pi = primitives.advance(pi)) {
            Primitive& prim = primitives.get(pi);
            VERIFY_ASSERT(prim.getContainer() == this);
            prim.mark = false;
        }
 	    for (pi = primitives.begin();
                            !primitives.done(pi);
                            pi = primitives.advance(pi)) {
            Primitive& prim = primitives.get(pi);
            VERIFY_ASSERT(prim.getContainer() == this);
            for (DataConsumer *input = prim.getInputsBegin(); input != prim.getInputsEnd(); input++) {
                if (input->isVariable()) {
                    if (input->getVariable().getContainer() == this) {
                        VERIFY_ASSERT(input->getVariable().mark);
                    }
                    else {
                        VERIFY_ASSERT(reachingSet[dfsNum]->test(input->getVariable().getContainer()->dfsNum));
                    }
                }
            }
            prim.mark = true;
        }
    }
    
    // count the number of incoming/outgoing exception, normal and backward (etc) edges

    Uint32 incomingExceptionEdges = 0, incomingNormalEdges = 0, incomingBackwardEdges = 0, incomingReturnEdges = 0;
    Uint32 outgoingExceptionEdges = 0, outgoingNormalEdges = 0, outgoingEndEdges = 0;

    if (!predecessors.empty()) {
        for (DoublyLinkedList<ControlEdge>::iterator predi = predecessors.begin();
                            !predecessors.done(predi);
                            predi = predecessors.advance(predi)) {
            ControlEdge &e = predecessors.get(predi);
            if (e.getSource().dfsNum > dfsNum) incomingBackwardEdges++;
            if (e.getSource().hasControlKind(ckReturn)) 
                incomingReturnEdges++;
            else
                if (e.getSource().hasControlKind(ckThrow))
                    incomingExceptionEdges++;
                else
                    if (e.getSource().hasControlKind(ckExc) || e.getSource().hasControlKind(ckAExc))    // the first successor from an Exc node is normal
                        if (&e.getSource().getSuccessorsBegin()->getTarget() == this)
                            incomingNormalEdges++;
                        else
                            incomingExceptionEdges++;
                    else
                        incomingNormalEdges++;
       }
    }
    for (ControlEdge *successor = successorsBegin; successor != successorsEnd; successor++) {
        if (successor->getTarget().hasControlKind(ckCatch))
            outgoingExceptionEdges++;
        else
            if (successor->getTarget().hasControlKind(ckEnd))
                outgoingEndEdges++;
            else
                outgoingNormalEdges++;
    }

    if (!hasExceptionInputs(controlKind))           VERIFY_ASSERT(incomingExceptionEdges == 0);     /* CONSTRAINT 'BEGIN3' */
    if (!hasNormalInputs(controlKind))              VERIFY_ASSERT(incomingNormalEdges == 0);        /* CONSTRAINT 'BEGIN3', 'END2' */
    if (!hasExceptionOutgoingEdges(controlKind))    VERIFY_ASSERT(outgoingExceptionEdges == 0);     /* CONSTRAINT 'END3', 'EDGE2' */
    if (!hasNormalOutgoingEdges(controlKind))       VERIFY_ASSERT(outgoingNormalEdges == 0);        /* CONSTRAINT 'END3' */
    if (hasOneNormalOutgoingEdge(controlKind))      VERIFY_ASSERT(outgoingNormalEdges == 1);        /* CONSTRAINT 'BEGIN2' */
    if (controlKind != ckEnd)                       VERIFY_ASSERT(incomingReturnEdges == 0);        /* CONSTRAINT 'BEGIN3', 'END2' */
    if (controlKind == ckReturn)                    VERIFY_ASSERT(outgoingEndEdges == 1);           /* CONSTRAINT 'RETURN2' */
    else
        if ((controlKind != ckAExc) 
                && (controlKind != ckExc) 
                    && (controlKind != ckThrow))    VERIFY_ASSERT(outgoingEndEdges == 0);           /* CONSTRAINT 'END3', 'RETURN2' */
    if (controlKind != ckAExc)                      VERIFY_ASSERT(incomingBackwardEdges == 0);      /* CONSTRAINT 'EDGE1' */
    
    if ((controlKind != ckBegin) && (controlKind != ckEnd) && (controlKind != ckCatch)) {
        Uint32 totalOutgoingEdges = outgoingExceptionEdges + outgoingNormalEdges + outgoingEndEdges;
        ControlNode &theSuccessorNode = successorsBegin->getTarget();
        if ((totalOutgoingEdges == 1) 
                    && (!theSuccessorNode.hasControlKind(ckEnd))
                        && (!theSuccessorNode.hasControlKind(ckCatch))) {
            // this node has a single output, prove that it's successor has more than a single input
            const DoublyLinkedList<ControlEdge> &theSuccessorsPredecessors = theSuccessorNode.getPredecessors();
            DoublyLinkedList<ControlEdge>::iterator predi = theSuccessorsPredecessors.begin();
            predi = theSuccessorsPredecessors.advance(predi);
            VERIFY_ASSERT(!theSuccessorsPredecessors.done(predi));   /* CONSTRAINT 'COALESCE1' */
        }
    }

    // now perform control-kind specific primitive checks...
    switch (controlKind) {
        case ckNone :       // shouldn't have any of these
            VERIFY_ASSERT(false);
            break;
        case ckBegin :      // all primitives are Args
            {
	            for (DoublyLinkedList<Primitive>::iterator pi = primitives.begin();
                                    !primitives.done(pi);
                                    pi = primitives.advance(pi)) {
                    Primitive& prim = primitives.get(pi);
                    VERIFY_ASSERT(prim.hasCategory(pcArg));
                }
                // REMIND - assert that the correct number of args is here (does it equal beginExtra.nArguments ?)
                // REMIND - assert that a memory arg is present ?
            }
            break;
        case ckEnd :   // only a result memory primitive (and maybe a phi for it)
            {        
                bool foundTheResult = false;
                for (DoublyLinkedList<Primitive>::iterator pi = primitives.begin();
                                    !primitives.done(pi);
                                    pi = primitives.advance(pi)) {
                    Primitive& prim = primitives.get(pi);
                    if (prim.hasOperation(poResult_M)) {
                        VERIFY_ASSERT(!foundTheResult);            // insist on only one such
                        foundTheResult = true;
                    }
                    else
                        VERIFY_ASSERT(prim.hasOperation(poPhi_M));
                }
                VERIFY_ASSERT(foundTheResult);
            }
            break;
        case ckBlock :  // no exception raising primitives, no control flow primitives
            {
	            for (DoublyLinkedList<Primitive>::iterator pi = primitives.begin();
                                    !primitives.done(pi);
                                    pi = primitives.advance(pi)) {
                    Primitive& prim = primitives.get(pi);
                    VERIFY_ASSERT(!prim.hasFormat(pfControl));
                    VERIFY_ASSERT(!prim.canRaiseException());
                }
            }
            break;
	    case ckIf :     // exactly two exits
            {           // no exception raising primitives, no control flow primitives other than the one If
                VERIFY_ASSERT(successorsEnd == (successorsBegin + 2));
                Primitive *theIf = NULL;
 	            for (DoublyLinkedList<Primitive>::iterator pi = primitives.begin();
                                    !primitives.done(pi);
                                    pi = primitives.advance(pi)) {
                    Primitive& prim = primitives.get(pi);
                    if (prim.hasFormat(pfControl)) {
                        if (prim.hasCategory(pcIfCond)) {
                            VERIFY_ASSERT(theIf == NULL);
                            theIf = &prim;
                        }
                        else
                            VERIFY_ASSERT(false);
                    }
                    VERIFY_ASSERT(!prim.canRaiseException());
               }
                VERIFY_ASSERT(theIf != NULL);
                // the source for the if condition must be in the same control node
                VERIFY_ASSERT(theIf->nthInputVariable(0).getContainer() == this);
            }
            break;
	    case ckSwitch :     // no exception raising primitives, no control flow primitives other than the one Switch
            {
                bool foundTheSwitch = false;
 	            for (DoublyLinkedList<Primitive>::iterator pi = primitives.begin();
                                    !primitives.done(pi);
                                    pi = primitives.advance(pi)) {
                    Primitive& prim = primitives.get(pi);
                    if (prim.hasFormat(pfControl)) {
                        if (prim.hasCategory(pcSwitch)) {
                            VERIFY_ASSERT(!foundTheSwitch);
                            foundTheSwitch = true;
                        }
                        else
                            VERIFY_ASSERT(false);
                    }
                    VERIFY_ASSERT(!prim.canRaiseException());
               }
                VERIFY_ASSERT(foundTheSwitch);
            }
            break;
	    case ckAExc :       // no exception raising primitives, no control nodes
            {
  	            for (DoublyLinkedList<Primitive>::iterator pi = primitives.begin();
                                    !primitives.done(pi);
                                    pi = primitives.advance(pi)) {
                    Primitive& prim = primitives.get(pi);
                    VERIFY_ASSERT(!prim.hasFormat(pfControl));
                    VERIFY_ASSERT(!prim.canRaiseException());
               }
            }
            break;
	    case ckThrow :        // contains no control primitives
            {
   	            for (DoublyLinkedList<Primitive>::iterator pi = primitives.begin();
                                    !primitives.done(pi);
                                    pi = primitives.advance(pi)) {
                    Primitive& prim = primitives.get(pi);
                    VERIFY_ASSERT(!prim.hasFormat(pfControl));
                }
            }
            break;
	    case ckExc :        // contains only one exception raising primitive, no control primitives
            {
                bool foundTheExceptionPrimitive = false;
   	            for (DoublyLinkedList<Primitive>::iterator pi = primitives.begin();
                                    !primitives.done(pi);
                                    pi = primitives.advance(pi)) {
                    Primitive& prim = primitives.get(pi);
                    VERIFY_ASSERT(!prim.hasFormat(pfControl));
                    if (prim.canRaiseException()) {
                        VERIFY_ASSERT(!foundTheExceptionPrimitive);
                        foundTheExceptionPrimitive = true;
                    }
                }
                if (!foundTheExceptionPrimitive) {
                    printPretty(UT_LOG_MODULE(GraphVerifier), 0);
                }
                VERIFY_ASSERT(foundTheExceptionPrimitive);
            }
            break;
	    case ckCatch :      // at most one primitive, must be a catch (with optional phi's), no control primitives
            {               
                bool foundTheCatch = false;
                for (DoublyLinkedList<Primitive>::iterator pi = primitives.begin();
                                    !primitives.done(pi);
                                    pi = primitives.advance(pi)) {
                    Primitive& prim = primitives.get(pi);
                    VERIFY_ASSERT(!prim.hasFormat(pfControl));
                    if (prim.hasCategory(pcCatch)) {
                        VERIFY_ASSERT(!foundTheCatch);
                        foundTheCatch = true;
                    }
                    else
                        VERIFY_ASSERT(prim.hasCategory(pcPhi));
                }
                VERIFY_ASSERT(foundTheCatch);
            }
            break;
	    case ckReturn :     // no exception raising primitives, no control flow primitives
            {
                 for (DoublyLinkedList<Primitive>::iterator pi = primitives.begin();
                                    !primitives.done(pi);
                                    pi = primitives.advance(pi)) {
                    Primitive& prim = primitives.get(pi);
                    VERIFY_ASSERT(!prim.hasFormat(pfControl));
                    VERIFY_ASSERT(!prim.canRaiseException());
                }
            }
            break;
        default :
            VERIFY_ASSERT(false);
            break;
    }

    // make sure that all a primitive's inputs come from primitives outside of this node
    // or one that is before the primitive within the control node


    // now ask each contained primitive to validate itself

    for (pi = primitives.begin();
                        !primitives.done(pi);
                        pi = primitives.advance(pi)) {
        Primitive& prim = primitives.get(pi);
        prim.validate();
    }

}

void DataNode::validate()
{
    InputConstraintPattern theConstraint = inputConstraintPatterns[operation];

    int inputIndex = 0;
    DataConsumer *input = inputsBegin;
    if (input == NULL) {
        if (theConstraint.nInputConstraints != 0)
            printPretty(UT_LOG_MODULE(GraphVerifier), 0);
        VERIFY_ASSERT(theConstraint.nInputConstraints == 0);
    }
    else
        while (true) {

// something about tuples and exceptions ????

            VERIFY_ASSERT((theConstraint.inputConstraints[inputIndex].kind == vkVoid)
                                || (input->getKind() == theConstraint.inputConstraints[inputIndex].kind));
 /*
            if (! ((theConstraint.inputConstraints[inputIndex].origin == aoEither)
                                || (input->isVariable() == (theConstraint.inputConstraints[inputIndex].origin == aoVariable))) )
                printPretty(UT_LOG_MODULE(GraphVerifier), 0);
*/
            VERIFY_ASSERT((theConstraint.inputConstraints[inputIndex].origin == aoEither)
                                || (input->isVariable() == (theConstraint.inputConstraints[inputIndex].origin == aoVariable)));
            inputIndex++;
            input++;
            if (inputIndex == theConstraint.nInputConstraints) {
                if (input == inputsEnd)
                    break;                  // all's well, and we ended together
                else
                    if (theConstraint.repeat)
                        inputIndex--;           // keep matching on the last pattern
                    else
                        VERIFY_ASSERT(false);   // there are no more patterns, but there are more inputs
            }
            else {
                // if we still have more patterns, make sure there are inputs to match OR that we're on
                //  the 'zero or more times' repeatable last pattern and in which case, break out of the loop.
                if (input == inputsEnd) {
                    VERIFY_ASSERT((inputIndex == (theConstraint.nInputConstraints - 1)) && theConstraint.repeat);
                    break;
                }
            }
        }

}

void PrimConst::validate()
{
    InputConstraintPattern theConstraint = getInputConstraintPattern();
    VERIFY_ASSERT(theConstraint.inputConstraints[0].origin == aoConstant);

    if (getKind() != theConstraint.inputConstraints[0].kind) {
        printPretty(UT_LOG_MODULE(GraphVerifier), 0);
    }
    VERIFY_ASSERT(getKind() == theConstraint.inputConstraints[0].kind);
}


#endif  // DEBUG
