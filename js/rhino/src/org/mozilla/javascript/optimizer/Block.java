/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Norris Boyd
 * Igor Bukanov
 * Roger Lawrence
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

package org.mozilla.javascript.optimizer;

import org.mozilla.javascript.*;
import org.mozilla.classfile.*;

import java.util.Hashtable;
import java.util.Enumeration;

import java.io.PrintWriter;
import java.io.StringWriter;

class Block
{

    private static class FatBlock
    {

        FatBlock(int startNodeIndex, int endNodeIndex, Node[] statementNodes)
        {
            realBlock = new Block(startNodeIndex, endNodeIndex, statementNodes);
        }

        private static Block[] reduceToArray(ObjToIntMap map)
        {
            Block[] result = null;
            if (!map.isEmpty()) {
                result = new Block[map.size()];
                int i = 0;
                ObjToIntMap.Iterator iter = map.newIterator();
                for (iter.start(); !iter.done(); iter.next()) {
                    FatBlock fb = (FatBlock)(iter.getKey());
                    result[i++] = fb.realBlock;
                }
            }
            return result;
        }

        void addSuccessor(FatBlock b)  { successors.put(b, 0); }
        void addPredecessor(FatBlock b)  { predecessors.put(b, 0); }

        Block[] getSuccessors() { return reduceToArray(successors); }
        Block[] getPredecessors() { return reduceToArray(predecessors); }

            // all the Blocks that come immediately after this
        private ObjToIntMap successors = new ObjToIntMap();
            // all the Blocks that come immediately before this
        private ObjToIntMap predecessors = new ObjToIntMap();

        Block realBlock;
    }

    private static class CSEHolder
    {
       CSEHolder(Node parent, Node child)
       {
            getPropParent = parent;
            getPropChild = child;
       }

       Node getPropParent;
       Node getPropChild;
    }

    Block(int startNodeIndex, int endNodeIndex, Node[] statementNodes)
    {
        itsStartNodeIndex = startNodeIndex;
        itsEndNodeIndex = endNodeIndex;
        itsStatementNodes = statementNodes;
    }

    void setBlockID(int id)  { itsBlockID = id; }
    int getBlockID()         { return itsBlockID; }
    Node getStartNode()      { return itsStatementNodes[itsStartNodeIndex]; }
    Node getEndNode()        { return itsStatementNodes[itsEndNodeIndex]; }

    Block[] getPredecessorList() { return itsPredecessors; }
    Block[] getSuccessorList()   { return itsSuccessors; }

    static Block[] buildBlocks(Node[] statementNodes)
    {
            // a mapping from each target node to the block it begins
        Hashtable theTargetBlocks = new Hashtable();
        ObjArray theBlocks = new ObjArray();

            // there's a block that starts at index 0
        int beginNodeIndex = 0;

        for (int i = 0; i < statementNodes.length; i++) {
            switch (statementNodes[i].getType()) {
                case Token.TARGET :
                    {
                        if (i != beginNodeIndex) {
                            FatBlock fb = new FatBlock(beginNodeIndex, i - 1,
                                                       statementNodes);
                            if (statementNodes[beginNodeIndex].getType()
                                                        == Token.TARGET)
                                theTargetBlocks.put(statementNodes[beginNodeIndex], fb);
                            theBlocks.add(fb);
                             // start the next block at this node
                            beginNodeIndex = i;
                        }
                    }
                    break;
                case Token.IFNE :
                case Token.IFEQ :
                case Token.GOTO :
                    {
                        FatBlock fb = new FatBlock(beginNodeIndex, i,
                                                   statementNodes);
                        if (statementNodes[beginNodeIndex].getType()
                                                       == Token.TARGET)
                            theTargetBlocks.put(statementNodes[beginNodeIndex], fb);
                        theBlocks.add(fb);
                            // start the next block at the next node
                        beginNodeIndex = i + 1;
                    }
                    break;
            }
        }

        if ((beginNodeIndex != statementNodes.length)) {
            FatBlock fb = new FatBlock(beginNodeIndex,
                                       statementNodes.length - 1,
                                       statementNodes);
            if (statementNodes[beginNodeIndex].getType() == Token.TARGET)
                theTargetBlocks.put(statementNodes[beginNodeIndex], fb);
            theBlocks.add(fb);
        }

        // build successor and predecessor links

        for (int i = 0; i < theBlocks.size(); i++) {
            FatBlock fb = (FatBlock)(theBlocks.get(i));

            Node blockEndNode = fb.realBlock.getEndNode();
            int blockEndNodeType = blockEndNode.getType();

            if ((blockEndNodeType != Token.GOTO)
                                         && (i < (theBlocks.size() - 1))) {
                FatBlock fallThruTarget = (FatBlock)(theBlocks.get(i + 1));
                fb.addSuccessor(fallThruTarget);
                fallThruTarget.addPredecessor(fb);
            }


            if ( (blockEndNodeType == Token.IFNE)
                        || (blockEndNodeType == Token.IFEQ)
                                || (blockEndNodeType == Token.GOTO) ) {
                Node target = ((Node.Jump)blockEndNode).target;
                FatBlock branchTargetBlock
                                    = (FatBlock)(theTargetBlocks.get(target));
                target.putProp(Node.TARGETBLOCK_PROP,
                                           branchTargetBlock.realBlock);
                fb.addSuccessor(branchTargetBlock);
                branchTargetBlock.addPredecessor(fb);
            }
        }

        Block[] result = new Block[theBlocks.size()];

        for (int i = 0; i < theBlocks.size(); i++) {
            FatBlock fb = (FatBlock)(theBlocks.get(i));
            Block b = fb.realBlock;
            b.setSuccessorList(fb.getSuccessors());
            b.setPredecessorList(fb.getPredecessors());
            b.setBlockID(i);
            result[i] = b;
        }

        return result;
    }

    static String toString(Block[] blockList, Node[] statementNodes)
    {
        if (!Optimizer.DEBUG_OPTIMIZER) return null;

        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);

        pw.println(blockList.length + " Blocks");
        for (int i = 0; i < blockList.length; i++) {
            Block b = blockList[i];
            pw.println("#" + b.itsBlockID);
            pw.println("from " + b.itsStartNodeIndex
                            + " "
                            + statementNodes[b.itsStartNodeIndex].toString());
            pw.println("thru " + b.itsEndNodeIndex
                            + " "
                            + statementNodes[b.itsEndNodeIndex].toString());
            pw.print("Predecessors ");
            if (b.itsPredecessors != null) {
                for (int j = 0; j < b.itsPredecessors.length; j++)
                    pw.print(b.itsPredecessors[j].getBlockID() + " ");
                pw.println();
            }
            else
                pw.println("none");
            pw.print("Successors ");
            if (b.itsSuccessors != null) {
                for (int j = 0; j < b.itsSuccessors.length; j++)
                    pw.print(b.itsSuccessors[j].getBlockID() + " ");
                pw.println();
            }
            else
                pw.println("none");
        }
        return sw.toString();
    }

    /*
        We maintain the liveSet as each statement executes, identifying
        those variables that are live across function calls

    */
    void lookForVariablesAndCalls(Node n, boolean liveSet[],
                                  OptFunctionNode fn)
    {
        switch (n.getType()) {
            case Token.SETVAR :
                {
                    Node lhs = n.getFirstChild();
                    Node rhs = lhs.getNext();
                    lookForVariablesAndCalls(rhs, liveSet, fn);
                    Object theVarProp = n.getProp(Node.VARIABLE_PROP);
                    if (theVarProp != null) {
                        int theVarIndex = ((OptLocalVariable)theVarProp).getIndex();
                        liveSet[theVarIndex] = true;
                    }
                }
                break;
            case Token.CALL : {
                    Node child = n.getFirstChild();
                    while (child != null) {
                        lookForVariablesAndCalls(child, liveSet, fn);
                        child = child.getNext();
                    }
                    for (int i = 0; i < liveSet.length; i++) {
                        if (liveSet[i])
                            fn.getVar(i).markLiveAcrossCall();
                    }
                }
                break;
            case Token.GETVAR :
                {
                    Object theVarProp = n.getProp(Node.VARIABLE_PROP);
                    if (theVarProp != null) {
                        int theVarIndex = ((OptLocalVariable)theVarProp).getIndex();
                        if ((n.getProp(Node.LASTUSE_PROP) != null)
                                && !itsLiveOnExitSet.test(theVarIndex))
                            liveSet[theVarIndex] = false;
                    }
                }
                break;
            default :
                Node child = n.getFirstChild();
                while (child != null) {
                    lookForVariablesAndCalls(child, liveSet, fn);
                    child = child.getNext();
                }
                break;
        }
    }

    void markAnyTypeVariables(OptFunctionNode fn)
    {
        for (int i = 0; i < fn.getVarCount(); i++)
            if (itsLiveOnEntrySet.test(i))
                fn.getVar(i).assignType(Optimizer.AnyType);

    }

    void markVolatileVariables(OptFunctionNode fn)
    {
        boolean liveSet[] = new boolean[fn.getVarCount()];
        for (int i = 0; i < liveSet.length; i++)
            liveSet[i] = itsLiveOnEntrySet.test(i);
        for (int i = itsStartNodeIndex; i <= itsEndNodeIndex; i++) {
            Node n = itsStatementNodes[i];
            lookForVariablesAndCalls(n, liveSet, fn);
        }
    }

    /*
        We're tracking uses and defs - in order to
        build the def set and to identify the last use
        nodes.

        The itsNotDefSet is built reversed then flipped later.

    */
    void lookForVariableAccess(Node n, Node lastUse[])
    {
        switch (n.getType()) {
            case Token.DEC :
            case Token.INC :
                {
                    Node child = n.getFirstChild();
                    if (child.getType() == Token.GETVAR) {
                        Object theVarProp = child.getProp(Node.VARIABLE_PROP);
                        if (theVarProp != null) {
                            int theVarIndex = ((OptLocalVariable)theVarProp).getIndex();
                            if (!itsNotDefSet.test(theVarIndex))
                                itsUseBeforeDefSet.set(theVarIndex);
                            itsNotDefSet.set(theVarIndex);
                        }
                    }
                }
                break;
            case Token.SETVAR :
                {
                    Node lhs = n.getFirstChild();
                    Node rhs = lhs.getNext();
                    lookForVariableAccess(rhs, lastUse);
                    Object theVarProp = n.getProp(Node.VARIABLE_PROP);
                    if (theVarProp != null) {
                        int theVarIndex = ((OptLocalVariable)theVarProp).getIndex();
                        itsNotDefSet.set(theVarIndex);
                        if (lastUse[theVarIndex] != null)
                            lastUse[theVarIndex].putProp(Node.LASTUSE_PROP,
                                                                   theVarProp);
                    }
                }
                break;
            case Token.GETVAR :
                {
                    Object theVarProp = n.getProp(Node.VARIABLE_PROP);
                    if (theVarProp != null) {
                        int theVarIndex = ((OptLocalVariable)theVarProp).getIndex();
                        if (!itsNotDefSet.test(theVarIndex))
                            itsUseBeforeDefSet.set(theVarIndex);
                        lastUse[theVarIndex] = n;
                    }
                }
                break;
            default :
                Node child = n.getFirstChild();
                while (child != null) {
                    lookForVariableAccess(child, lastUse);
                    child = child.getNext();
                }
                break;
        }
    }

    /*
        build the live on entry/exit sets.
        Then walk the trees looking for defs/uses of variables
        and build the def and useBeforeDef sets.
    */
    void initLiveOnEntrySets(OptFunctionNode fn)
    {
        int listLength = fn.getVarCount();
        Node lastUse[] = new Node[listLength];
        itsUseBeforeDefSet = new DataFlowBitSet(listLength);
        itsNotDefSet = new DataFlowBitSet(listLength);
        itsLiveOnEntrySet = new DataFlowBitSet(listLength);
        itsLiveOnExitSet = new DataFlowBitSet(listLength);
        for (int i = itsStartNodeIndex; i <= itsEndNodeIndex; i++) {
            Node n = itsStatementNodes[i];
            lookForVariableAccess(n, lastUse);
        }
        for (int i = 0; i < listLength; i++) {
            if (lastUse[i] != null)
                lastUse[i].putProp(Node.LASTUSE_PROP, this);
        }
        itsNotDefSet.not();         // truth in advertising
    }

    /*
        the liveOnEntry of each successor is the liveOnExit for this block.
        The liveOnEntry for this block is -
        liveOnEntry = liveOnExit - defsInThisBlock + useBeforeDefsInThisBlock

    */
    boolean doReachedUseDataFlow()
    {
        itsLiveOnExitSet.clear();
        if (itsSuccessors != null)
            for (int i = 0; i < itsSuccessors.length; i++)
                itsLiveOnExitSet.or(itsSuccessors[i].itsLiveOnEntrySet);
        return itsLiveOnEntrySet.df2(itsLiveOnExitSet,
                                            itsUseBeforeDefSet, itsNotDefSet);
    }

    /*
        the type of an expression is relatively unknown. Cases we can be sure
        about are -
            Literals,
            Arithmetic operations - always return a Number
    */
    int findExpressionType(Node n)
    {
        switch (n.getType()) {
            case Token.NUMBER :
                return Optimizer.NumberType;

            case Token.NEW :
            case Token.CALL :
                return Optimizer.NoType;

            case Token.GETELEM :
               return Optimizer.AnyType;

            case Token.GETVAR : {
                    OptLocalVariable theVar = (OptLocalVariable)
                                      (n.getProp(Node.VARIABLE_PROP));
                    if (theVar != null)
                        return theVar.getTypeUnion();
                }

            case Token.INC :
            case Token.DEC :
            case Token.DIV:
            case Token.MOD:
            case Token.BITOR:
            case Token.BITXOR:
            case Token.BITAND:
            case Token.LSH:
            case Token.RSH:
            case Token.URSH:
            case Token.SUB :
                return Optimizer.NumberType;

            case Token.ADD : {
                    // if the lhs & rhs are known to be numbers, we can be sure that's
                    // the result, otherwise it could be a string.
                    Node child = n.getFirstChild();
                    int lType = findExpressionType(child);
                    int rType = findExpressionType(child.getNext());
                    return lType | rType;       // we're not distinguishng strings yet
                }
            default : {
                    Node child = n.getFirstChild();
                    if (child == null)
                        return Optimizer.AnyType;
                    else {
                        int result = Optimizer.NoType;
                        while (child != null) {
                            result |= findExpressionType(child);
                            child = child.getNext();
                        }
                        return result;
                    }
                }
        }
    }

    boolean findDefPoints(Node n)
    {
        boolean result = false;
        switch (n.getType()) {
            default : {
                    Node child = n.getFirstChild();
                    while (child != null) {
                        result |= findDefPoints(child);
                        child = child.getNext();
                    }
                }
                break;
            case Token.DEC :
            case Token.INC : {
                    Node firstChild = n.getFirstChild();
                    OptLocalVariable theVar = (OptLocalVariable)
                                      (firstChild.getProp(Node.VARIABLE_PROP));
                    if (theVar != null) {
                        // theVar is a Number now
                        result |= theVar.assignType(Optimizer.NumberType);
                    }
                }
                break;

            case Token.SETPROP :
            case Token.SETPROP_OP : {
                    Node baseChild = n.getFirstChild();
                    Node nameChild = baseChild.getNext();
                    Node rhs = nameChild.getNext();
                    if (baseChild != null) {
                        if (baseChild.getType() == Token.GETVAR) {
                            OptLocalVariable theVar = (OptLocalVariable)
                                              (baseChild.getProp(Node.VARIABLE_PROP));
                            if (theVar != null)
                                theVar.assignType(Optimizer.AnyType);
                        }
                        result |= findDefPoints(baseChild);
                    }
                    if (nameChild != null) result |= findDefPoints(nameChild);
                    if (rhs != null) result |= findDefPoints(rhs);
                }
                break;

            case Token.SETVAR : {
                    Node firstChild = n.getFirstChild();
                    OptLocalVariable theVar = (OptLocalVariable)
                                      (n.getProp(Node.VARIABLE_PROP));
                    if (theVar != null) {
                        Node rValue = firstChild.getNext();
                        int theType = findExpressionType(rValue);
                        result |= theVar.assignType(theType);
                    }
                }
                break;
        }
        return result;
    }

    // a total misnomer for now. To start with we're only trying to find
    // duplicate getProp calls on 'this' that can be merged
    void localCSE(Node parent, Node n, Hashtable theCSETable, OptFunctionNode theFunction)
    {
        switch (n.getType()) {
            default : {
                    Node child = n.getFirstChild();
                    while (child != null) {
                        localCSE(n, child, theCSETable, theFunction);
                        child = child.getNext();
                    }
                }
                break;
            case Token.DEC :
            case Token.INC : {
                    Node child = n.getFirstChild();
                    if (child.getType() == Token.GETPROP) {
                        Node nameChild = child.getFirstChild().getNext();
                        if (nameChild.getType() == Token.STRING)
                            theCSETable.remove(nameChild.getString());
                        else
                            theCSETable.clear();
                    }
                    else
                        if (child.getType() != Token.GETVAR)
                            theCSETable.clear();
                }
                break;
            case Token.SETPROP :
            case Token.SETPROP_OP : {
                    Node baseChild = n.getFirstChild();
                    Node nameChild = baseChild.getNext();
                    Node rhs = nameChild.getNext();
                    if (baseChild != null) localCSE(n, baseChild, theCSETable, theFunction);
                    if (nameChild != null) localCSE(n, nameChild, theCSETable, theFunction);
                    if (rhs != null) localCSE(n, rhs, theCSETable, theFunction);
                    if (nameChild.getType() == Token.STRING) {
                        theCSETable.remove(nameChild.getString());
//                        System.out.println("clear at SETPROP " + ((StringNode)nameChild).getString());
                    }
                    else {
                        theCSETable.clear();
//                        System.out.println("clear all at SETPROP");
                    }
                }
                break;
            case Token.GETPROP : {
                    Node baseChild = n.getFirstChild();
                    if (baseChild != null) {
                        localCSE(n, baseChild, theCSETable, theFunction);
                    }
                    if (baseChild.getType() == Token.THIS) {
                        Node nameChild = baseChild.getNext();
                        if (nameChild.getType() == Token.STRING) {
                            String theName = nameChild.getString();
//            System.out.println("considering " + theName);
                            Object cse = theCSETable.get(theName);
                            if (cse == null) {
                                theCSETable.put(theName, new CSEHolder(parent, n));
                            }
                            else {
                                if (parent != null) {
//                                    System.out.println("Yay for " + theName);
                                    Node theCSE;
                                    if (cse instanceof CSEHolder) {
                                        CSEHolder cseHolder = (CSEHolder)cse;
                                        Node nextChild = cseHolder.getPropChild.getNext();
                                        cseHolder.getPropParent.removeChild(cseHolder.getPropChild);
                                        theCSE = OptTransformer.createNewTemp(cseHolder.getPropChild);
                                        if (nextChild == null)
                                            cseHolder.getPropParent.addChildToBack(theCSE);
                                        else
                                            cseHolder.getPropParent.addChildBefore(theCSE, nextChild);
                                        theCSETable.put(theName, theCSE);
                                    }
                                    else
                                        theCSE = (Node)cse;
                                    Node nextChild = n.getNext();
                                    parent.removeChild(n);
                                    Node cseUse = OptTransformer.createUseTemp(theCSE);
                                    if (nextChild == null)
                                        parent.addChildToBack(cseUse);
                                    else
                                        parent.addChildBefore(cseUse, nextChild);
                                }
                            }
                        }
                    }
                }
                break;
            case Token.SETELEM :
            case Token.SETELEM_OP : {
                    Node lhsBase = n.getFirstChild();
                    Node lhsIndex = lhsBase.getNext();
                    Node rhs = lhsIndex.getNext();
                    if (lhsBase != null) localCSE(n, lhsBase, theCSETable, theFunction);
                    if (lhsIndex != null) localCSE(n, lhsIndex, theCSETable, theFunction);
                    if (rhs != null) localCSE(n, rhs, theCSETable, theFunction);
                    theCSETable.clear();
//System.out.println("clear all at SETELEM");
                }
                break;
            case Token.CALL : {
                    Node child = n.getFirstChild();
                    while (child != null) {
                        localCSE(n, child, theCSETable, theFunction);
                        child = child.getNext();
                    }
                    theCSETable.clear();
//System.out.println("clear all at CALL");
                }
                break;
        }
    }

    Hashtable localCSE(Hashtable theCSETable, OptFunctionNode theFunction)
    {
        if (theCSETable == null) theCSETable = new Hashtable(5);
        for (int i = itsStartNodeIndex; i <= itsEndNodeIndex; i++) {
            Node n = itsStatementNodes[i];
            if (n != null)
                localCSE(null, n, theCSETable, theFunction);
        }
        return theCSETable;
    }

    void findDefs()
    {
        for (int i = itsStartNodeIndex; i <= itsEndNodeIndex; i++) {
            Node n = itsStatementNodes[i];
            if (n != null)
                findDefPoints(n);
        }
    }

    boolean doTypeFlow()
    {
        boolean changed = false;

        for (int i = itsStartNodeIndex; i <= itsEndNodeIndex; i++) {
            Node n = itsStatementNodes[i];
            if (n != null)
                changed |= findDefPoints(n);
        }

        return changed;
    }

    boolean isLiveOnEntry(int index)
    {
        return (itsLiveOnEntrySet != null) && (itsLiveOnEntrySet.test(index));
    }

    void printLiveOnEntrySet(PrintWriter pw, OptFunctionNode fn)
    {
        for (int i = 0; i < fn.getVarCount(); i++) {
            String name = fn.getVar(i).getName();
            if (itsUseBeforeDefSet.test(i))
                pw.println(name + " is used before def'd");
            if (itsNotDefSet.test(i))
                pw.println(name + " is not def'd");
            if (itsLiveOnEntrySet.test(i))
                pw.println(name + " is live on entry");
            if (itsLiveOnExitSet.test(i))
                pw.println(name + " is live on exit");
        }
    }

    void setSuccessorList(Block[] b)    { itsSuccessors = b; }
    void setPredecessorList(Block[] b)  { itsPredecessors = b; }

        // all the Blocks that come immediately after this
    private Block[] itsSuccessors;
        // all the Blocks that come immediately before this
    private Block[] itsPredecessors;

    private int itsStartNodeIndex;       // the Node at the start of the block
    private int itsEndNodeIndex;         // the Node at the end of the block
    private Node itsStatementNodes[];    // the list of all statement nodes

    private int itsBlockID;               // a unique index for each block

// reaching def bit sets -
    private DataFlowBitSet itsLiveOnEntrySet;
    private DataFlowBitSet itsLiveOnExitSet;
    private DataFlowBitSet itsUseBeforeDefSet;
    private DataFlowBitSet itsNotDefSet;

}

