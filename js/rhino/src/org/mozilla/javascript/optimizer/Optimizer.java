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

import java.io.PrintWriter;
import java.io.DataOutputStream;
import java.io.FileOutputStream;
import java.io.File;
import java.io.IOException;

import java.util.Hashtable;

class Optimizer
{

    static final int NoType = 0;
    static final int NumberType = 1;
    static final int AnyType = 3;

    static final int TO_OBJECT = Token.LAST_TOKEN + 1;
    static final int TO_DOUBLE = Token.LAST_TOKEN + 2;

    // It is assumed that (NumberType | AnyType) == AnyType

    void optimize(ScriptOrFnNode scriptOrFn, int optLevel)
    {
        itsOptLevel = optLevel;
        //  run on one function at a time for now
        int functionCount = scriptOrFn.getFunctionCount();
        for (int i = 0; i != functionCount; ++i) {
            OptFunctionNode f = OptFunctionNode.get(scriptOrFn, i);
            optimizeFunction(f);
        }
    }

    private void optimizeFunction(OptFunctionNode theFunction)
    {
        if (theFunction.fnode.requiresActivation()) return;

        inDirectCallFunction = theFunction.isTargetOfDirectCall();

        ObjArray statementsArray = new ObjArray();
        buildStatementList_r(theFunction.fnode, statementsArray);
        Node[] theStatementNodes = new Node[statementsArray.size()];
        statementsArray.toArray(theStatementNodes);

        Block[] theBlocks = Block.buildBlocks(theStatementNodes);
        PrintWriter pw = null;
        try {
            if (DEBUG_OPTIMIZER) {
                String fileName = "blocks"+debug_blockCount+".txt";
                ++debug_blockCount;
                pw = new PrintWriter(
                            new DataOutputStream(
                                new FileOutputStream(new File(fileName))));
                pw.println(Block.toString(theBlocks, theStatementNodes));
            }

            theFunction.establishVarsIndices();
            for (int i = 0; i < theStatementNodes.length; i++)
                replaceVariableAccess(theStatementNodes[i], theFunction);

            reachingDefDataFlow(theFunction, theBlocks);
            typeFlow(theFunction, theBlocks);
            findSinglyTypedVars(theFunction, theBlocks);
            localCSE(theBlocks, theFunction);
            if (!theFunction.fnode.requiresActivation()) {
                /*
                 * Now that we know which local vars are in fact always
                 * Numbers, we re-write the tree to take advantage of
                 * that. Any arithmetic or assignment op involving just
                 * Number typed vars is marked so that the codegen will
                 * generate non-object code.
                 */
                parameterUsedInNumberContext = false;
                for (int i = 0; i < theStatementNodes.length; i++) {
                    rewriteForNumberVariables(theStatementNodes[i]);
                }
                theFunction.setParameterNumberContext(parameterUsedInNumberContext);
                //System.out.println("Function " + theFunction.getFunctionName() + " has parameters in number contexts  : " + parameterUsedInNumberContext);
            }
            if (DEBUG_OPTIMIZER) {
                for (int i = 0; i < theBlocks.length; i++) {
                    pw.println("For block " + theBlocks[i].getBlockID());
                    theBlocks[i].printLiveOnEntrySet(pw, theFunction);
                }
                int N = theFunction.getVarCount();
                System.out.println("Variable Table, size = " + N);
                for (int i = 0; i != N; i++) {
                    OptLocalVariable lVar = theFunction.getVar(i);
                    pw.println(lVar.toString());
                }
            }
            if (DEBUG_OPTIMIZER) pw.close();
        }
        catch (IOException x)   // for the DEBUG_OPTIMIZER i/o
        {
        }
        finally {
            if (DEBUG_OPTIMIZER) pw.close();
        }
    }

    private static void
    findSinglyTypedVars(OptFunctionNode fn, Block theBlocks[])
    {
/*
    discover the type events for each non-volatile variable (not live
    across function calls). A type event is a def, which sets the target
    type to the source type.
*/
        if (false) {
            /*
                it's enough to prove that every def point for a local variable
                confers the same type on that variable. If that is the case (and
                that type is 'Number') then we can assign that local variable to
                a Double jReg for the life of the function.
            */
            for (int i = 0; i < theBlocks.length; i++) {
                theBlocks[i].findDefs();
            }
        }
        for (int i = 0; i < fn.getVarCount(); i++) {
            OptLocalVariable lVar = fn.getVar(i);
            if (!lVar.isParameter()) {
                int theType = lVar.getTypeUnion();
                if (theType == NumberType) {
                    lVar.setIsNumber();
                }
            }
        }
    }

    private static void
    doBlockLocalCSE(Block theBlocks[], Block b, Hashtable theCSETable,
                    boolean beenThere[], OptFunctionNode theFunction)
    {
        if (!beenThere[b.getBlockID()]) {
            beenThere[b.getBlockID()] = true;
            theCSETable = b.localCSE(theCSETable, theFunction);
            Block succ[] = theBlocks[b.getBlockID()].getSuccessorList();
            if (succ != null) {
                for (int i = 0; i < succ.length; i++) {
                    int index = succ[i].getBlockID();
                    Block pred[] = theBlocks[index].getPredecessorList();
                    if (pred.length == 1)
                        doBlockLocalCSE(theBlocks, succ[i],
                                   (Hashtable)(theCSETable.clone()),
                                                beenThere, theFunction);
                }
            }
        }
    }

    private static void
    localCSE(Block theBlocks[], OptFunctionNode theFunction)
    {
        boolean beenThere[] = new boolean[theBlocks.length];
        doBlockLocalCSE(theBlocks, theBlocks[0], null, beenThere, theFunction);
        for (int i = 0; i < theBlocks.length; i++) {
            if (!beenThere[i]) theBlocks[i].localCSE(null, theFunction);
        }
    }

    private static void
    typeFlow(OptFunctionNode fn, Block theBlocks[])
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
                if (theBlocks[vIndex].doTypeFlow()) {
                    Block succ[] = theBlocks[vIndex].getSuccessorList();
                    if (succ != null) {
                        for (int i = 0; i < succ.length; i++) {
                            int index = succ[i].getBlockID();
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

    private static void
    reachingDefDataFlow(OptFunctionNode fn, Block theBlocks[])
    {
/*
    initialize the liveOnEntry and liveOnExit sets, then discover the variables
    that are def'd by each function, and those that are used before being def'd
    (hence liveOnEntry)
*/
        for (int i = 0; i < theBlocks.length; i++) {
            theBlocks[i].initLiveOnEntrySets(fn);
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
                    Block pred[] = theBlocks[vIndex].getPredecessorList();
                    if (pred != null) {
                        for (int i = 0; i < pred.length; i++) {
                            int index = pred[i].getBlockID();
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
    The liveOnEntry, liveOnExit sets are now complete. Discover the variables
    that are live across function calls.
*/
/*
        if any variable is live on entry to block 0, we have to mark it as
        not jRegable - since it means that someone is trying to access the
        'undefined'-ness of that variable.
*/

        theBlocks[0].markAnyTypeVariables(fn);
    }

/*
        Each directCall parameter is passed as a pair of values - an object
        and a double. The value passed depends on the type of value available at
        the call site. If a double is available, the object in java/lang/Void.TYPE
        is passed as the object value, and if an object value is available, then
        0.0 is passed as the double value.

        The receiving routine always tests the object value before proceeding.
        If the parameter is being accessed in a 'Number Context' then the code
        sequence is :
        if ("parameter_objectValue" == java/lang/Void.TYPE)
            ...fine..., use the parameter_doubleValue
        else
            toNumber(parameter_objectValue)

        and if the parameter is being referenced in an Object context, the code is
        if ("parameter_objectValue" == java/lang/Void.TYPE)
            new Double(parameter_doubleValue)
        else
            ...fine..., use the parameter_objectValue

        If the receiving code never uses the doubleValue, it is converted on
        entry to a Double instead.
*/


/*
        We're referencing a node in a Number context (i.e. we'd prefer it
        was a double value). If the node is a parameter in a directCall
        function, mark it as being referenced in this context.
*/
    private void markDCPNumberContext(Node n)
    {
        if (inDirectCallFunction && (n.getType() == Token.GETVAR))
        {
            OptLocalVariable theVar
                 = (OptLocalVariable)(n.getProp(Node.VARIABLE_PROP));
            if ((theVar != null) && theVar.isParameter()) {
                parameterUsedInNumberContext = true;
            }
        }
    }

    private boolean convertParameter(Node n)
    {
        if (inDirectCallFunction && (n.getType() == Token.GETVAR))
        {
            OptLocalVariable theVar
                 = (OptLocalVariable)(n.getProp(Node.VARIABLE_PROP));
            if ((theVar != null) && theVar.isParameter()) {
                n.removeProp(Node.ISNUMBER_PROP);
                return true;
            }
        }
        return false;
    }

    private int rewriteForNumberVariables(Node n)
    {
        switch (n.getType()) {
            case Token.POP : {
                    Node child = n.getFirstChild();
                    int type = rewriteForNumberVariables(child);
                    if (type == NumberType)
                        n.putIntProp(Node.ISNUMBER_PROP, Node.BOTH);
                     return NoType;
                }
            case Token.NUMBER :
                n.putIntProp(Node.ISNUMBER_PROP, Node.BOTH);
                return NumberType;

            case Token.GETVAR : {
                    OptLocalVariable theVar
                         = (OptLocalVariable)(n.getProp(Node.VARIABLE_PROP));
                    if (theVar != null) {
                        if (inDirectCallFunction && theVar.isParameter()) {
                            n.putIntProp(Node.ISNUMBER_PROP, Node.BOTH);
                            return NumberType;
                        }
                        else
                            if (theVar.isNumber()) {
                                n.putIntProp(Node.ISNUMBER_PROP, Node.BOTH);
                                return NumberType;
                            }
                    }
                    return NoType;
                }

            case Token.INC :
            case Token.DEC : {
                    Node child = n.getFirstChild();     // will be a GETVAR or GETPROP
                    if (child.getType() == Token.GETVAR) {
                        OptLocalVariable theVar
                             = (OptLocalVariable)(child.getProp(Node.VARIABLE_PROP));
                        if ((theVar != null) && theVar.isNumber()) {
                            n.putIntProp(Node.ISNUMBER_PROP, Node.BOTH);
                            markDCPNumberContext(child);
                            return NumberType;
                        }
                        else
                            return NoType;
                    }
                    else
                        return NoType;
                }
            case Token.SETVAR : {
                    Node lChild = n.getFirstChild();
                    Node rChild = lChild.getNext();
                    int rType = rewriteForNumberVariables(rChild);
                    OptLocalVariable theVar
                         = (OptLocalVariable)(n.getProp(Node.VARIABLE_PROP));
                    if (inDirectCallFunction && theVar.isParameter()) {
                        if (rType == NumberType) {
                            if (!convertParameter(rChild)) {
                                n.putIntProp(Node.ISNUMBER_PROP, Node.BOTH);
                                return NumberType;
                            }
                            markDCPNumberContext(rChild);
                            return NoType;
                        }
                        else
                            return rType;
                    }
                    else {
                        if ((theVar != null) && theVar.isNumber()) {
                            if (rType != NumberType) {
                                n.removeChild(rChild);
                                n.addChildToBack(new Node(TO_DOUBLE, rChild));
                            }
                            n.putIntProp(Node.ISNUMBER_PROP, Node.BOTH);
                            markDCPNumberContext(rChild);
                            return NumberType;
                        }
                        else {
                            if (rType == NumberType) {
                                if (!convertParameter(rChild)) {
                                    n.removeChild(rChild);
                                    n.addChildToBack(new Node(TO_OBJECT,
                                                              rChild));
                                }
                            }
                            return NoType;
                        }
                    }
                }
            case Token.LE :
            case Token.LT :
            case Token.GE :
            case Token.GT : {
                    Node lChild = n.getFirstChild();
                    Node rChild = lChild.getNext();
                    int lType = rewriteForNumberVariables(lChild);
                    int rType = rewriteForNumberVariables(rChild);
                    markDCPNumberContext(lChild);
                    markDCPNumberContext(rChild);

                    if (convertParameter(lChild)) {
                        if (convertParameter(rChild)) {
                            return NoType;
                        } else if (rType == NumberType) {
                            n.putIntProp(Node.ISNUMBER_PROP, Node.RIGHT);
                        }
                    }
                    else if (convertParameter(rChild)) {
                        if (lType == NumberType) {
                            n.putIntProp(Node.ISNUMBER_PROP, Node.LEFT);
                        }
                    }
                    else {
                        if (lType == NumberType) {
                            if (rType == NumberType) {
                                n.putIntProp(Node.ISNUMBER_PROP, Node.BOTH);
                            }
                            else {
                                n.putIntProp(Node.ISNUMBER_PROP, Node.LEFT);
                            }
                        }
                        else {
                            if (rType == NumberType) {
                                n.putIntProp(Node.ISNUMBER_PROP, Node.RIGHT);
                            }
                        }
                    }
                    // we actually build a boolean value
                    return NoType;
                }

            case Token.ADD : {
                    Node lChild = n.getFirstChild();
                    Node rChild = lChild.getNext();
                    int lType = rewriteForNumberVariables(lChild);
                    int rType = rewriteForNumberVariables(rChild);


                    if (convertParameter(lChild)) {
                        if (convertParameter(rChild)) {
                            return NoType;
                        }
                        else {
                            if (rType == NumberType) {
                                n.putIntProp(Node.ISNUMBER_PROP, Node.RIGHT);
                            }
                        }
                    }
                    else {
                        if (convertParameter(rChild)) {
                            if (lType == NumberType) {
                                n.putIntProp(Node.ISNUMBER_PROP, Node.LEFT);
                            }
                        }
                        else {
                            if (lType == NumberType) {
                                if (rType == NumberType) {
                                    n.putIntProp(Node.ISNUMBER_PROP, Node.BOTH);
                                    return NumberType;
                                }
                                else {
                                    n.putIntProp(Node.ISNUMBER_PROP, Node.LEFT);
                                }
                            }
                            else {
                                if (rType == NumberType) {
                                    n.putIntProp(Node.ISNUMBER_PROP,
                                                 Node.RIGHT);
                                }
                            }
                        }
                    }
                    return NoType;
                }

            case Token.BITXOR :
            case Token.BITOR :
            case Token.BITAND :
            case Token.RSH :
            case Token.LSH :
            case Token.SUB :
            case Token.MUL :
            case Token.DIV :
            case Token.MOD : {
                    Node lChild = n.getFirstChild();
                    Node rChild = lChild.getNext();
                    int lType = rewriteForNumberVariables(lChild);
                    int rType = rewriteForNumberVariables(rChild);
                    markDCPNumberContext(lChild);
                    markDCPNumberContext(rChild);
                    if (lType == NumberType) {
                        if (rType == NumberType) {
                            n.putIntProp(Node.ISNUMBER_PROP, Node.BOTH);
                            return NumberType;
                        }
                        else {
                            if (!convertParameter(rChild)) {
                                n.removeChild(rChild);
                                n.addChildToBack(new Node(TO_DOUBLE, rChild));
                                n.putIntProp(Node.ISNUMBER_PROP, Node.BOTH);
                            }
                            return NumberType;
                        }
                    }
                    else {
                        if (rType == NumberType) {
                            if (!convertParameter(lChild)) {
                                n.removeChild(lChild);
                                n.addChildToFront(new Node(TO_DOUBLE, lChild));
                                n.putIntProp(Node.ISNUMBER_PROP, Node.BOTH);
                            }
                            return NumberType;
                        }
                        else {
                            if (!convertParameter(lChild)) {
                                n.removeChild(lChild);
                                n.addChildToFront(new Node(TO_DOUBLE, lChild));
                            }
                            if (!convertParameter(rChild)) {
                                n.removeChild(rChild);
                                n.addChildToBack(new Node(TO_DOUBLE, rChild));
                            }
                            n.putIntProp(Node.ISNUMBER_PROP, Node.BOTH);
                            return NumberType;
                        }
                    }
                }
            case Token.SETELEM :
            case Token.SETELEM_OP : {
                    Node arrayBase = n.getFirstChild();
                    Node arrayIndex = arrayBase.getNext();
                    Node rValue = arrayIndex.getNext();
                    int baseType = rewriteForNumberVariables(arrayBase);
                    if (baseType == NumberType) {// can never happen ???
                        if (!convertParameter(arrayBase)) {
                            n.removeChild(arrayBase);
                            n.addChildToFront(new Node(TO_OBJECT, arrayBase));
                        }
                    }
                    int indexType = rewriteForNumberVariables(arrayIndex);
                    if (indexType == NumberType) {
                        // setting the ISNUMBER_PROP signals the codegen
                        // to use the scriptRuntime.setElem that takes
                        // a double index
                        n.putIntProp(Node.ISNUMBER_PROP, Node.LEFT);
                        markDCPNumberContext(arrayIndex);
                    }
                    int rValueType = rewriteForNumberVariables(rValue);
                    if (rValueType == NumberType) {
                        if (!convertParameter(rValue)) {
                            n.removeChild(rValue);
                            n.addChildToBack(new Node(TO_OBJECT, rValue));
                        }
                    }
                    return NoType;
                }
            case Token.GETELEM : {
                    Node arrayBase = n.getFirstChild();
                    Node arrayIndex = arrayBase.getNext();
                    int baseType = rewriteForNumberVariables(arrayBase);
                    if (baseType == NumberType) {// can never happen ???
                        if (!convertParameter(arrayBase)) {
                            n.removeChild(arrayBase);
                            n.addChildToFront(new Node(TO_OBJECT, arrayBase));
                        }
                    }
                    int indexType = rewriteForNumberVariables(arrayIndex);
                    if (indexType == NumberType) {
                        if (!convertParameter(arrayIndex)) {
                            // setting the ISNUMBER_PROP signals the codegen
                            // to use the scriptRuntime.getElem that takes
                            // a double index
                            n.putIntProp(Node.ISNUMBER_PROP, Node.RIGHT);
                        }
                    }
                    return NoType;
                }
            case Token.CALL :
                {
                    OptFunctionNode target
                            = (OptFunctionNode)n.getProp(Node.DIRECTCALL_PROP);
                    if (target != null) {
/*
    we leave each child as a Number if it can be. The codegen will
    handle moving the pairs of parameters.
*/
                        Node child = n.getFirstChild(); // the function
                        rewriteForNumberVariables(child);
                        child = child.getNext(); // the 'this' object
                        rewriteForNumberVariables(child);
                        child = child.getNext(); // the first arg
                        while (child != null) {
                            int type = rewriteForNumberVariables(child);
                            if (type == NumberType) {
                                markDCPNumberContext(child);
                            }
                            child = child.getNext();
                        }
                        return NoType;
                    }
                    // else fall thru...
                }
            default : {
                    Node child = n.getFirstChild();
                    while (child != null) {
                        Node nextChild = child.getNext();
                        int type = rewriteForNumberVariables(child);
                        if (type == NumberType) {
                            if (!convertParameter(child)) {
                                n.removeChild(child);
                                Node nuChild = new Node(TO_OBJECT, child);
                                if (nextChild == null)
                                    n.addChildToBack(nuChild);
                                else
                                    n.addChildBefore(nuChild, nextChild);
                            }
                        }
                        child = nextChild;
                    }
                    return NoType;
                }
        }
    }

    private static void replaceVariableAccess(Node n, OptFunctionNode fn)
    {
        Node child = n.getFirstChild();
        while (child != null) {
            replaceVariableAccess(child, fn);
            child = child.getNext();
        }
        int type = n.getType();
        if (type == Token.SETVAR) {
            String name = n.getFirstChild().getString();
            OptLocalVariable theVar = fn.getVar(name);
            if (theVar != null) {
                n.putProp(Node.VARIABLE_PROP, theVar);
            }
        } else if (type == Token.GETVAR) {
            String name = n.getString();
            OptLocalVariable theVar = fn.getVar(name);
            if (theVar != null) {
                n.putProp(Node.VARIABLE_PROP, theVar);
            }
        }
    }
    private static void buildStatementList_r(Node node, ObjArray statements)
    {
        int type = node.getType();
        if (type == Token.BLOCK
            || type == Token.LOCAL_BLOCK
            || type == Token.LOOP
            || type == Token.FUNCTION)
        {
            Node child = node.getFirstChild();
            while (child != null) {
                buildStatementList_r(child, statements);
                child = child.getNext();
            }
        } else {
            statements.add(node);
        }
    }


    static final boolean DEBUG_OPTIMIZER = false;
    private static int debug_blockCount;

    private int itsOptLevel;
    private boolean inDirectCallFunction;
    private boolean parameterUsedInNumberContext;
}
