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

    Block(int startNodeIndex, int endNodeIndex)
    {
        itsStartNodeIndex = startNodeIndex;
        itsEndNodeIndex = endNodeIndex;
    }

    static void runFlowAnalyzes(OptFunctionNode fn, Node[] statementNodes)
    {
        int paramCount = fn.fnode.getParamCount();
        int varCount = fn.fnode.getParamAndVarCount();
        int[] varTypes = new int[varCount];
        // If the variable is a parameter, it could have any type.
        for (int i = 0; i != paramCount; ++i) {
            varTypes[i] = Optimizer.AnyType;
        }
        // If the variable is from a "var" statement, its typeEvent will be set
        // when we see the setVar node.
        for (int i = paramCount; i != varCount; ++i) {
            varTypes[i] = Optimizer.NoType;
        }

        Block[] theBlocks = buildBlocks(statementNodes);

        if (DEBUG) {
            ++debug_blockCount;
            System.out.println("-------------------"+fn.fnode.getFunctionName()+"  "+debug_blockCount+"--------");
            System.out.println(toString(theBlocks, statementNodes));
        }

        reachingDefDataFlow(fn, statementNodes, theBlocks, varTypes);
        typeFlow(fn, statementNodes, theBlocks, varTypes);

        if (DEBUG) {
            for (int i = 0; i < theBlocks.length; i++) {
                System.out.println("For block " + theBlocks[i].itsBlockID);
                theBlocks[i].printLiveOnEntrySet(fn);
            }
            System.out.println("Variable Table, size = " + varCount);
            for (int i = 0; i != varCount; i++) {
                System.out.println("["+i+"] type: "+varTypes[i]);
            }
        }

        for (int i = paramCount; i != varCount; i++) {
            if (varTypes[i] == Optimizer.NumberType) {
                fn.getVar(i).setIsNumber();
            }
        }

    }

    private static Block[] buildBlocks(Node[] statementNodes)
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
                            FatBlock fb = newFatBlock(beginNodeIndex, i - 1);
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
                        FatBlock fb = newFatBlock(beginNodeIndex, i);
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

        if (beginNodeIndex != statementNodes.length) {
            FatBlock fb = newFatBlock(beginNodeIndex, statementNodes.length - 1);
            if (statementNodes[beginNodeIndex].getType() == Token.TARGET)
                theTargetBlocks.put(statementNodes[beginNodeIndex], fb);
            theBlocks.add(fb);
        }

        // build successor and predecessor links

        for (int i = 0; i < theBlocks.size(); i++) {
            FatBlock fb = (FatBlock)(theBlocks.get(i));

            Node blockEndNode = statementNodes[fb.realBlock.itsEndNodeIndex];
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
            b.itsSuccessors = fb.getSuccessors();
            b.itsPredecessors = fb.getPredecessors();
            b.itsBlockID = i;
            result[i] = b;
        }

        return result;
    }

    private static FatBlock newFatBlock(int startNodeIndex, int endNodeIndex)
    {
        FatBlock fb = new FatBlock();
        fb.realBlock = new Block(startNodeIndex, endNodeIndex);
        return fb;
    }

    private static String toString(Block[] blockList, Node[] statementNodes)
    {
        if (!DEBUG) return null;

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
                    pw.print(b.itsPredecessors[j].itsBlockID + " ");
                pw.println();
            }
            else
                pw.println("none");
            pw.print("Successors ");
            if (b.itsSuccessors != null) {
                for (int j = 0; j < b.itsSuccessors.length; j++)
                    pw.print(b.itsSuccessors[j].itsBlockID + " ");
                pw.println();
            }
            else
                pw.println("none");
        }
        return sw.toString();
    }

    private static void reachingDefDataFlow(OptFunctionNode fn, Node[] statementNodes, Block theBlocks[], int[] varTypes)
    {
/*
    initialize the liveOnEntry and liveOnExit sets, then discover the variables
    that are def'd by each function, and those that are used before being def'd
    (hence liveOnEntry)
*/
        for (int i = 0; i < theBlocks.length; i++) {
            theBlocks[i].initLiveOnEntrySets(fn, statementNodes);
        }
/*
    this visits every block starting at the last, re-adding the predecessors of
    any block whose inputs change as a result of the dataflow.
    REMIND, better would be to visit in CFG postorder
*/
        boolean visit[] = new boolean[theBlocks.length];
        boolean doneOnce[] = new boolean[theBlocks.length];
        int vIndex = theBlocks.length - 1;
        boolean needRescan = false;
        visit[vIndex] = true;
        while (true) {
            if (visit[vIndex] || !doneOnce[vIndex]) {
                doneOnce[vIndex] = true;
                visit[vIndex] = false;
                if (theBlocks[vIndex].doReachedUseDataFlow()) {
                    Block pred[] = theBlocks[vIndex].itsPredecessors;
                    if (pred != null) {
                        for (int i = 0; i < pred.length; i++) {
                            int index = pred[i].itsBlockID;
                            visit[index] = true;
                            needRescan |= (index > vIndex);
                        }
                    }
                }
            }
            if (vIndex == 0) {
                if (needRescan) {
                    vIndex = theBlocks.length - 1;
                    needRescan = false;
                }
                else
                    break;
            }
            else
                vIndex--;
        }
/*
        if any variable is live on entry to block 0, we have to mark it as
        not jRegable - since it means that someone is trying to access the
        'undefined'-ness of that variable.
*/

        theBlocks[0].markAnyTypeVariables(varTypes);
    }

    private static void typeFlow(OptFunctionNode fn, Node[] statementNodes, Block theBlocks[], int[] varTypes)
    {
        boolean visit[] = new boolean[theBlocks.length];
        boolean doneOnce[] = new boolean[theBlocks.length];
        int vIndex = 0;
        boolean needRescan = false;
        visit[vIndex] = true;
        while (true) {
            if (visit[vIndex] || !doneOnce[vIndex]) {
                doneOnce[vIndex] = true;
                visit[vIndex] = false;
                if (theBlocks[vIndex].doTypeFlow(statementNodes, varTypes)) {
                    Block succ[] = theBlocks[vIndex].itsSuccessors;
                    if (succ != null) {
                        for (int i = 0; i < succ.length; i++) {
                            int index = succ[i].itsBlockID;
                            visit[index] = true;
                            needRescan |= (index < vIndex);
                        }
                    }
                }
            }
            if (vIndex == (theBlocks.length - 1)) {
                if (needRescan) {
                    vIndex = 0;
                    needRescan = false;
                }
                else
                    break;
            }
            else
                vIndex++;
        }
    }

    private static int getVarIndex(Node n)
    {
        return OptLocalVariable.get(n).getIndex();
    }

    private static boolean assignType(int[] varTypes, int index, int type)
    {
        return type != (varTypes[index] |= type);
    }

    private void markAnyTypeVariables(int[] varTypes)
    {
        for (int i = 0; i != varTypes.length; i++) {
            if (itsLiveOnEntrySet.test(i)) {
                assignType(varTypes, i, Optimizer.AnyType);
            }
        }

    }

    /*
        We're tracking uses and defs - in order to
        build the def set and to identify the last use
        nodes.

        The itsNotDefSet is built reversed then flipped later.

    */
    private void lookForVariableAccess(Node n)
    {
        switch (n.getType()) {
            case Token.DEC :
            case Token.INC :
                {
                    Node child = n.getFirstChild();
                    if (child.getType() == Token.GETVAR) {
                        int varIndex = getVarIndex(child);
                        if (!itsNotDefSet.test(varIndex))
                            itsUseBeforeDefSet.set(varIndex);
                        itsNotDefSet.set(varIndex);
                    }
                }
                break;
            case Token.SETVAR :
                {
                    Node lhs = n.getFirstChild();
                    Node rhs = lhs.getNext();
                    lookForVariableAccess(rhs);
                    itsNotDefSet.set(getVarIndex(n));
                }
                break;
            case Token.GETVAR :
                {
                    int varIndex = getVarIndex(n);
                    if (!itsNotDefSet.test(varIndex))
                        itsUseBeforeDefSet.set(varIndex);
                }
                break;
            default :
                Node child = n.getFirstChild();
                while (child != null) {
                    lookForVariableAccess(child);
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
    private void initLiveOnEntrySets(OptFunctionNode fn, Node[] statementNodes)
    {
        int listLength = fn.getVarCount();
        itsUseBeforeDefSet = new DataFlowBitSet(listLength);
        itsNotDefSet = new DataFlowBitSet(listLength);
        itsLiveOnEntrySet = new DataFlowBitSet(listLength);
        itsLiveOnExitSet = new DataFlowBitSet(listLength);
        for (int i = itsStartNodeIndex; i <= itsEndNodeIndex; i++) {
            Node n = statementNodes[i];
            lookForVariableAccess(n);
        }
        itsNotDefSet.not();         // truth in advertising
    }

    /*
        the liveOnEntry of each successor is the liveOnExit for this block.
        The liveOnEntry for this block is -
        liveOnEntry = liveOnExit - defsInThisBlock + useBeforeDefsInThisBlock

    */
    private boolean doReachedUseDataFlow()
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
    private static int findExpressionType(Node n, int[] varTypes)
    {
        switch (n.getType()) {
          case Token.NUMBER :
              return Optimizer.NumberType;

          case Token.CALL :
          case Token.NEW :
          case Token.REF_CALL :
              return Optimizer.NoType;

          case Token.GETELEM :
             return Optimizer.AnyType;

          case Token.GETVAR :
              return varTypes[getVarIndex(n)];

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
              int lType = findExpressionType(child, varTypes);
              int rType = findExpressionType(child.getNext(), varTypes);
              return lType | rType;    // we're not distinguishng strings yet
          }
        }

        Node child = n.getFirstChild();
        if (child == null) {
            return Optimizer.AnyType;
        } else {
            int result = Optimizer.NoType;
            while (child != null) {
                result |= findExpressionType(child, varTypes);
                child = child.getNext();
            }
            return result;
        }
    }

    private static boolean findDefPoints(Node n, int[] varTypes)
    {
        boolean result = false;
        Node child = n.getFirstChild();
        switch (n.getType()) {
          default :
            while (child != null) {
                result |= findDefPoints(child, varTypes);
                child = child.getNext();
            }
            break;
          case Token.DEC :
          case Token.INC :
            if (child.getType() == Token.GETVAR) {
                // theVar is a Number now
                int i = getVarIndex(child);
                result |= assignType(varTypes, i, Optimizer.NumberType);
            }
            break;
          case Token.SETPROP :
          case Token.SETPROP_OP :
            if (child.getType() == Token.GETVAR) {
                int i = getVarIndex(child);
                assignType(varTypes, i, Optimizer.AnyType);
            }
            while (child != null) {
                result |= findDefPoints(child, varTypes);
                child = child.getNext();
            }
            break;
          case Token.SETVAR : {
            Node rValue = child.getNext();
            int theType = findExpressionType(rValue, varTypes);
            int i = getVarIndex(n);
            result |= assignType(varTypes, i, theType);
            break;
          }
        }
        return result;
    }

    private boolean doTypeFlow(Node[] statementNodes, int[] varTypes)
    {
        boolean changed = false;

        for (int i = itsStartNodeIndex; i <= itsEndNodeIndex; i++) {
            Node n = statementNodes[i];
            if (n != null)
                changed |= findDefPoints(n, varTypes);
        }

        return changed;
    }

    private boolean isLiveOnEntry(int index)
    {
        return (itsLiveOnEntrySet != null) && (itsLiveOnEntrySet.test(index));
    }

    private void printLiveOnEntrySet(OptFunctionNode fn)
    {
        if (DEBUG) {
            for (int i = 0; i < fn.getVarCount(); i++) {
                String name = fn.fnode.getParamOrVarName(i);
                if (itsUseBeforeDefSet.test(i))
                    System.out.println(name + " is used before def'd");
                if (itsNotDefSet.test(i))
                    System.out.println(name + " is not def'd");
                if (itsLiveOnEntrySet.test(i))
                    System.out.println(name + " is live on entry");
                if (itsLiveOnExitSet.test(i))
                    System.out.println(name + " is live on exit");
            }
        }
    }

        // all the Blocks that come immediately after this
    private Block[] itsSuccessors;
        // all the Blocks that come immediately before this
    private Block[] itsPredecessors;

    private int itsStartNodeIndex;       // the Node at the start of the block
    private int itsEndNodeIndex;         // the Node at the end of the block

    private int itsBlockID;               // a unique index for each block

// reaching def bit sets -
    private DataFlowBitSet itsLiveOnEntrySet;
    private DataFlowBitSet itsLiveOnExitSet;
    private DataFlowBitSet itsUseBeforeDefSet;
    private DataFlowBitSet itsNotDefSet;

    static final boolean DEBUG = false;
    private static int debug_blockCount;

}

