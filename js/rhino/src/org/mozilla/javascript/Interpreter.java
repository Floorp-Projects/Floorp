/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Patrick Beard
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

package org.mozilla.javascript;

import java.io.*;

import org.mozilla.javascript.debug.*;

public class Interpreter {

// Additional interpreter-specific codes
    private static final int
    // To indicating a line number change in icodes.
        LINE_ICODE                      = TokenStream.LAST_TOKEN + 1,
        SOURCEFILE_ICODE                = TokenStream.LAST_TOKEN + 2,

    // To store shorts and ints inline
        SHORTNUMBER_ICODE               = TokenStream.LAST_TOKEN + 3,
        INTNUMBER_ICODE                 = TokenStream.LAST_TOKEN + 4,

    // To return undefined value
        RETURN_UNDEF_ICODE              = TokenStream.LAST_TOKEN + 5,

    // Last icode
        END_ICODE                       = TokenStream.LAST_TOKEN + 6;

    public IRFactory createIRFactory(TokenStream ts,
                                     ClassNameHelper nameHelper, Scriptable scope)
    {
        return new IRFactory(ts, scope);
    }

    public Node transform(Node tree, TokenStream ts, Scriptable scope) {
        return (new NodeTransformer()).transform(tree, null, ts, scope);
    }

    public Object compile(Context cx, Scriptable scope, Node tree,
                          Object securityDomain,
                          SecurityController securityController,
                          ClassNameHelper nameHelper)
    {
        version = cx.getLanguageVersion();
        itsData = new InterpreterData(securityDomain);
        if (tree instanceof FunctionNode) {
            FunctionNode f = (FunctionNode) tree;
            itsData.itsFunctionType = f.getFunctionType();
            generateFunctionICode(cx, scope, f);
            return createFunction(cx, scope, itsData, false);
        }
        generateScriptICode(cx, scope, tree);
        return new InterpretedScript(cx, itsData);
    }

    private void generateScriptICode(Context cx, Scriptable scope, Node tree) {
        itsSourceFile = (String) tree.getProp(Node.SOURCENAME_PROP);
        itsData.itsSourceFile = itsSourceFile;
        debugSource = (String) tree.getProp(Node.DEBUGSOURCE_PROP);

        generateNestedFunctions(cx, scope, tree);

        generateRegExpLiterals(cx, scope, tree);

        itsVariableTable = (VariableTable)tree.getProp(Node.VARS_PROP);
        generateICodeFromTree(tree);
        if (Context.printICode) dumpICode(itsData);

        if (cx.debugger != null) {
            cx.debugger.handleCompilationDone(cx, itsData, debugSource);
        }
    }

    private void generateFunctionICode(Context cx, Scriptable scope,
                                       FunctionNode theFunction)
    {
        // check if function has own source, which is the case
        // with Function(...)
        String savedSource = debugSource;
        debugSource = (String)theFunction.getProp(Node.DEBUGSOURCE_PROP);
        if (debugSource == null) {
            debugSource = savedSource;
        }
        generateNestedFunctions(cx, scope, theFunction);

        generateRegExpLiterals(cx, scope, theFunction);

        itsData.itsNeedsActivation = theFunction.requiresActivation();

        itsVariableTable = theFunction.getVariableTable();
        generateICodeFromTree(theFunction.getLastChild());

        itsData.itsName = theFunction.getFunctionName();
        itsData.itsSourceFile = (String) theFunction.getProp(
                                    Node.SOURCENAME_PROP);
        itsData.itsSource = (String)theFunction.getProp(Node.SOURCE_PROP);
        if (Context.printICode) dumpICode(itsData);

        if (cx.debugger != null) {
            cx.debugger.handleCompilationDone(cx, itsData, debugSource);
        }
        debugSource = savedSource;
    }

    private void generateNestedFunctions(Context cx, Scriptable scope,
                                         Node tree)
    {
        ObjArray functionList = (ObjArray) tree.getProp(Node.FUNCTION_PROP);
        if (functionList == null) return;

        int N = functionList.size();
        InterpreterData[] array = new InterpreterData[N];
        for (int i = 0; i != N; i++) {
            FunctionNode def = (FunctionNode)functionList.get(i);
            Interpreter jsi = new Interpreter();
            jsi.itsSourceFile = itsSourceFile;
            jsi.itsData = new InterpreterData(itsData.securityDomain);
            jsi.itsData.itsCheckThis = def.getCheckThis();
            jsi.itsData.itsFunctionType = def.getFunctionType();
            jsi.itsInFunctionFlag = true;
            jsi.debugSource = debugSource;
            jsi.generateFunctionICode(cx, scope, def);
            array[i] = jsi.itsData;
            def.putIntProp(Node.FUNCTION_PROP, i);
        }
        itsData.itsNestedFunctions = array;
    }

    private void generateRegExpLiterals(Context cx,
                                        Scriptable scope,
                                        Node tree)
    {
        ObjArray regexps = (ObjArray)tree.getProp(Node.REGEXP_PROP);
        if (regexps == null) return;

        RegExpProxy rep = cx.getRegExpProxy();
        if (rep == null) {
            throw cx.reportRuntimeError0("msg.no.regexp");
        }
        int N = regexps.size();
        Object[] array = new Object[N];
        for (int i = 0; i != N; i++) {
            Node regexp = (Node) regexps.get(i);
            Node left = regexp.getFirstChild();
            Node right = regexp.getLastChild();
            String source = left.getString();
            String global = (left != right) ? right.getString() : null;
            array[i] = rep.newRegExp(cx, scope, source, global, false);
            regexp.putIntProp(Node.REGEXP_PROP, i);
        }
        itsData.itsRegExpLiterals = array;
    }

    private void generateICodeFromTree(Node tree) {
        int theICodeTop = 0;
        theICodeTop = generateICode(tree, theICodeTop);
        itsLabels.fixLabelGotos(itsData.itsICode);
        // add END_ICODE only to scripts as function always ends with RETURN
        if (itsData.itsFunctionType == 0) {
            theICodeTop = addByte(END_ICODE, theICodeTop);
        }
        itsData.itsICodeTop = theICodeTop;

        if (itsData.itsICode.length != theICodeTop) {
            // Make itsData.itsICode length exactly theICodeTop to save memory
            // and catch bugs with jumps beyound icode as early as possible
            byte[] tmp = new byte[theICodeTop];
            System.arraycopy(itsData.itsICode, 0, tmp, 0, theICodeTop);
            itsData.itsICode = tmp;
        }
        if (itsStrings.size() == 0) {
            itsData.itsStringTable = null;
        } else {
            itsData.itsStringTable = new String[itsStrings.size()];
            ObjToIntMap.Iterator iter = itsStrings.newIterator();
            for (iter.start(); !iter.done(); iter.next()) {
                String str = (String)iter.getKey();
                int index = iter.getValue();
                if (itsData.itsStringTable[index] != null) Context.codeBug();
                itsData.itsStringTable[index] = str;
            }
        }
        if (itsDoubleTableTop == 0) {
            itsData.itsDoubleTable = null;
        } else if (itsData.itsDoubleTable.length != itsDoubleTableTop) {
            double[] tmp = new double[itsDoubleTableTop];
            System.arraycopy(itsData.itsDoubleTable, 0, tmp, 0,
                             itsDoubleTableTop);
            itsData.itsDoubleTable = tmp;
        }

        itsData.itsMaxVars = itsVariableTable.size();
        // itsMaxFrameArray: interpret method needs this amount for its
        // stack and sDbl arrays
        itsData.itsMaxFrameArray = itsData.itsMaxVars
                                   + itsData.itsMaxLocals
                                   + itsData.itsMaxTryDepth
                                   + itsData.itsMaxStack;

        itsData.argNames = new String[itsVariableTable.size()];
        itsVariableTable.getAllVariables(itsData.argNames);
        itsData.argCount = itsVariableTable.getParameterCount();
    }

    private int updateLineNumber(Node node, int iCodeTop) {
        int lineno = node.getLineno();
        if (lineno != itsLineNumber && lineno >= 0) {
            itsLineNumber = lineno;
            iCodeTop = addByte(LINE_ICODE, iCodeTop);
            iCodeTop = addShort(lineno, iCodeTop);
        }
        return iCodeTop;
    }

    private void badTree(Node node) {
        try {
            out = new PrintWriter(new FileOutputStream("icode.txt", true));
            out.println("Un-handled node : " + node.toString());
            out.close();
        }
        catch (IOException x) {}
        throw new RuntimeException("Un-handled node : "
                                        + node.toString());
    }

    private int generateICode(Node node, int iCodeTop) {
        int type = node.getType();
        Node child = node.getFirstChild();
        Node firstChild = child;
        switch (type) {

            case TokenStream.FUNCTION : {
                FunctionNode fn
                    = (FunctionNode) node.getProp(Node.FUNCTION_PROP);
                if (fn.itsFunctionType != FunctionNode.FUNCTION_STATEMENT) {
                    // Only function expressions or function expression
                    // statements needs closure code creating new function
                    // object on stack as function statements are initialized
                    // at script/function start
                    int index = fn.getExistingIntProp(Node.FUNCTION_PROP);
                    iCodeTop = addByte(TokenStream.CLOSURE, iCodeTop);
                    iCodeTop = addIndex(index, iCodeTop);
                    itsStackDepth++;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                }
                break;
            }

            case TokenStream.SCRIPT :
                iCodeTop = updateLineNumber(node, iCodeTop);
                while (child != null) {
                    if (child.getType() != TokenStream.FUNCTION)
                        iCodeTop = generateICode(child, iCodeTop);
                    child = child.getNext();
                }
                break;

            case TokenStream.CASE :
                iCodeTop = updateLineNumber(node, iCodeTop);
                child = child.getNext();
                while (child != null) {
                    iCodeTop = generateICode(child, iCodeTop);
                    child = child.getNext();
                }
                break;

            case TokenStream.LABEL :
            case TokenStream.WITH :
            case TokenStream.LOOP :
            case TokenStream.DEFAULT :
            case TokenStream.BLOCK :
            case TokenStream.VOID :
            case TokenStream.NOP :
                iCodeTop = updateLineNumber(node, iCodeTop);
                while (child != null) {
                    iCodeTop = generateICode(child, iCodeTop);
                    child = child.getNext();
                }
                break;

            case TokenStream.COMMA :
                iCodeTop = generateICode(child, iCodeTop);
                while (null != (child = child.getNext())) {
                    iCodeTop = addByte(TokenStream.POP, iCodeTop);
                    itsStackDepth--;
                    iCodeTop = generateICode(child, iCodeTop);
                }
                break;

            case TokenStream.SWITCH : {
                iCodeTop = updateLineNumber(node, iCodeTop);
                iCodeTop = generateICode(child, iCodeTop);
                int theLocalSlot = itsData.itsMaxLocals++;
                iCodeTop = addByte(TokenStream.NEWTEMP, iCodeTop);
                iCodeTop = addByte(theLocalSlot, iCodeTop);
                iCodeTop = addByte(TokenStream.POP, iCodeTop);
                itsStackDepth--;

                ObjArray cases = (ObjArray) node.getProp(Node.CASES_PROP);
                for (int i = 0; i < cases.size(); i++) {
                    Node thisCase = (Node)cases.get(i);
                    Node first = thisCase.getFirstChild();
                    // the case expression is the firstmost child
                    // the rest will be generated when the case
                    // statements are encountered as siblings of
                    // the switch statement.
                    iCodeTop = generateICode(first, iCodeTop);
                    iCodeTop = addByte(TokenStream.USETEMP, iCodeTop);
                    iCodeTop = addByte(theLocalSlot, iCodeTop);
                    itsStackDepth++;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                    iCodeTop = addByte(TokenStream.SHEQ, iCodeTop);
                    itsStackDepth--;
                    Node target = new Node(TokenStream.TARGET);
                    thisCase.addChildAfter(target, first);
                    iCodeTop = addGoto(target, TokenStream.IFEQ, iCodeTop);
                }

                Node defaultNode = (Node) node.getProp(Node.DEFAULT_PROP);
                if (defaultNode != null) {
                    Node defaultTarget = new Node(TokenStream.TARGET);
                    defaultNode.getFirstChild().
                        addChildToFront(defaultTarget);
                    iCodeTop = addGoto(defaultTarget, TokenStream.GOTO,
                                       iCodeTop);
                }

                Node breakTarget = (Node) node.getProp(Node.BREAK_PROP);
                iCodeTop = addGoto(breakTarget, TokenStream.GOTO,
                                   iCodeTop);
                break;
            }

            case TokenStream.TARGET : {
                markTargetLabel(node, iCodeTop);
                // if this target has a FINALLY_PROP, it is a JSR target
                // and so has a PC value on the top of the stack
                if (node.getProp(Node.FINALLY_PROP) != null) {
                    itsStackDepth = 1;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                }
                break;
            }

            case TokenStream.EQOP :
            case TokenStream.RELOP : {
                iCodeTop = generateICode(child, iCodeTop);
                child = child.getNext();
                iCodeTop = generateICode(child, iCodeTop);
                int op = node.getOperation();
                if (version == Context.VERSION_1_2) {
                    if (op == TokenStream.EQ) {
                        op = TokenStream.SHEQ;
                    } else if (op == TokenStream.NE) {
                        op = TokenStream.SHNE;
                    }
                }
                iCodeTop = addByte(op, iCodeTop);
                itsStackDepth--;
                break;
            }

            case TokenStream.NEW :
            case TokenStream.CALL : {
                if (itsSourceFile != null
                    && (itsData.itsSourceFile == null
                        || !itsSourceFile.equals(itsData.itsSourceFile)))
                {
                    itsData.itsSourceFile = itsSourceFile;
                }
                iCodeTop = addByte(SOURCEFILE_ICODE, iCodeTop);

                int childCount = 0;
                String functionName = null;
                while (child != null) {
                    iCodeTop = generateICode(child, iCodeTop);
                    if (functionName == null) {
                        int childType = child.getType();
                        if (childType == TokenStream.NAME
                            || childType == TokenStream.GETPROP)
                        {
                            functionName = lastAddString;
                        }
                    }
                    child = child.getNext();
                    childCount++;
                }
                if (node.getProp(Node.SPECIALCALL_PROP) != null) {
                    // embed line number and source filename
                    iCodeTop = addByte(TokenStream.CALLSPECIAL, iCodeTop);
                    iCodeTop = addShort(itsLineNumber, iCodeTop);
                    iCodeTop = addString(itsSourceFile, iCodeTop);
                } else {
                    iCodeTop = addByte(type, iCodeTop);
                    iCodeTop = addString(functionName, iCodeTop);
                }

                itsStackDepth -= (childCount - 1);  // always a result value
                // subtract from child count to account for [thisObj &] fun
                if (type == TokenStream.NEW) {
                    childCount -= 1;
                } else {
                    childCount -= 2;
                }
                iCodeTop = addIndex(childCount, iCodeTop);
                if (childCount > itsData.itsMaxCalleeArgs)
                    itsData.itsMaxCalleeArgs = childCount;

                iCodeTop = addByte(SOURCEFILE_ICODE, iCodeTop);
                break;
            }

            case TokenStream.NEWLOCAL :
            case TokenStream.NEWTEMP : {
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addByte(TokenStream.NEWTEMP, iCodeTop);
                iCodeTop = addLocalRef(node, iCodeTop);
                break;
            }

            case TokenStream.USELOCAL : {
                if (node.getProp(Node.TARGET_PROP) != null) {
                    iCodeTop = addByte(TokenStream.RETSUB, iCodeTop);
                } else {
                    iCodeTop = addByte(TokenStream.USETEMP, iCodeTop);
                    itsStackDepth++;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                }
                Node temp = (Node) node.getProp(Node.LOCAL_PROP);
                iCodeTop = addLocalRef(temp, iCodeTop);
                break;
            }

            case TokenStream.USETEMP : {
                iCodeTop = addByte(TokenStream.USETEMP, iCodeTop);
                Node temp = (Node) node.getProp(Node.TEMP_PROP);
                iCodeTop = addLocalRef(temp, iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;
            }

            case TokenStream.IFEQ :
            case TokenStream.IFNE :
                iCodeTop = generateICode(child, iCodeTop);
                itsStackDepth--;    // after the conditional GOTO, really
                    // fall thru...
            case TokenStream.GOTO : {
                Node target = (Node)(node.getProp(Node.TARGET_PROP));
                iCodeTop = addGoto(target, (byte) type, iCodeTop);
                break;
            }

            case TokenStream.JSR : {
                /*
                    mark the target with a FINALLY_PROP to indicate
                    that it will have an incoming PC value on the top
                    of the stack.
                    !!!
                    This only works if the target follows the JSR
                    in the tree.
                    !!!
                */
                Node target = (Node)(node.getProp(Node.TARGET_PROP));
                target.putProp(Node.FINALLY_PROP, node);
                // Bug 115717 is due to adding a GOSUB here before
                // we insert an ENDTRY. I'm not sure of the best way
                // to fix this; perhaps we need to maintain a stack
                // of pending trys and have some knowledge of how
                // many trys we need to close when we perform a
                // GOTO or GOSUB.
                iCodeTop = addGoto(target, TokenStream.GOSUB, iCodeTop);
                break;
            }

            case TokenStream.AND : {
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addByte(TokenStream.DUP, iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                int falseJumpStart = iCodeTop;
                iCodeTop = addForwardGoto(TokenStream.IFNE, iCodeTop);
                iCodeTop = addByte(TokenStream.POP, iCodeTop);
                itsStackDepth--;
                child = child.getNext();
                iCodeTop = generateICode(child, iCodeTop);
                resolveForwardGoto(falseJumpStart, iCodeTop);
                break;
            }

            case TokenStream.OR : {
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addByte(TokenStream.DUP, iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                int trueJumpStart = iCodeTop;
                iCodeTop = addForwardGoto(TokenStream.IFEQ, iCodeTop);
                iCodeTop = addByte(TokenStream.POP, iCodeTop);
                itsStackDepth--;
                child = child.getNext();
                iCodeTop = generateICode(child, iCodeTop);
                resolveForwardGoto(trueJumpStart, iCodeTop);
                break;
            }

            case TokenStream.GETPROP : {
                iCodeTop = generateICode(child, iCodeTop);
                String s = (String) node.getProp(Node.SPECIAL_PROP_PROP);
                if (s != null) {
                    if (s.equals("__proto__")) {
                        iCodeTop = addByte(TokenStream.GETPROTO, iCodeTop);
                    } else if (s.equals("__parent__")) {
                        iCodeTop = addByte(TokenStream.GETSCOPEPARENT,
                                           iCodeTop);
                    } else {
                        badTree(node);
                    }
                } else {
                    child = child.getNext();
                    iCodeTop = generateICode(child, iCodeTop);
                    iCodeTop = addByte(TokenStream.GETPROP, iCodeTop);
                    itsStackDepth--;
                }
                break;
            }

            case TokenStream.DELPROP :
            case TokenStream.BITAND :
            case TokenStream.BITOR :
            case TokenStream.BITXOR :
            case TokenStream.LSH :
            case TokenStream.RSH :
            case TokenStream.URSH :
            case TokenStream.ADD :
            case TokenStream.SUB :
            case TokenStream.MOD :
            case TokenStream.DIV :
            case TokenStream.MUL :
            case TokenStream.GETELEM :
                iCodeTop = generateICode(child, iCodeTop);
                child = child.getNext();
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addByte(type, iCodeTop);
                itsStackDepth--;
                break;

            case TokenStream.CONVERT : {
                iCodeTop = generateICode(child, iCodeTop);
                Object toType = node.getProp(Node.TYPE_PROP);
                if (toType == ScriptRuntime.NumberClass) {
                    iCodeTop = addByte(TokenStream.POS, iCodeTop);
                } else {
                    badTree(node);
                }
                break;
            }

            case TokenStream.UNARYOP :
                iCodeTop = generateICode(child, iCodeTop);
                switch (node.getOperation()) {
                    case TokenStream.VOID :
                        iCodeTop = addByte(TokenStream.POP, iCodeTop);
                        iCodeTop = addByte(TokenStream.UNDEFINED, iCodeTop);
                        break;
                    case TokenStream.NOT : {
                        int trueJumpStart = iCodeTop;
                        iCodeTop = addForwardGoto(TokenStream.IFEQ,
                                                  iCodeTop);
                        iCodeTop = addByte(TokenStream.TRUE, iCodeTop);
                        int beyondJumpStart = iCodeTop;
                        iCodeTop = addForwardGoto(TokenStream.GOTO,
                                                  iCodeTop);
                        resolveForwardGoto(trueJumpStart, iCodeTop);
                        iCodeTop = addByte(TokenStream.FALSE, iCodeTop);
                        resolveForwardGoto(beyondJumpStart, iCodeTop);
                        break;
                    }
                    case TokenStream.BITNOT :
                        iCodeTop = addByte(TokenStream.BITNOT, iCodeTop);
                        break;
                    case TokenStream.TYPEOF :
                        iCodeTop = addByte(TokenStream.TYPEOF, iCodeTop);
                        break;
                    case TokenStream.SUB :
                        iCodeTop = addByte(TokenStream.NEG, iCodeTop);
                        break;
                    case TokenStream.ADD :
                        iCodeTop = addByte(TokenStream.POS, iCodeTop);
                        break;
                    default:
                        badTree(node);
                        break;
                }
                break;

            case TokenStream.SETPROP : {
                iCodeTop = generateICode(child, iCodeTop);
                child = child.getNext();
                iCodeTop = generateICode(child, iCodeTop);
                String s = (String) node.getProp(Node.SPECIAL_PROP_PROP);
                if (s != null) {
                    if (s.equals("__proto__")) {
                        iCodeTop = addByte(TokenStream.SETPROTO, iCodeTop);
                    } else if (s.equals("__parent__")) {
                        iCodeTop = addByte(TokenStream.SETPARENT, iCodeTop);
                    } else {
                        badTree(node);
                    }
                } else {
                    child = child.getNext();
                    iCodeTop = generateICode(child, iCodeTop);
                    iCodeTop = addByte(TokenStream.SETPROP, iCodeTop);
                    itsStackDepth -= 2;
                }
                break;
            }

            case TokenStream.SETELEM :
                iCodeTop = generateICode(child, iCodeTop);
                child = child.getNext();
                iCodeTop = generateICode(child, iCodeTop);
                child = child.getNext();
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addByte(type, iCodeTop);
                itsStackDepth -= 2;
                break;

            case TokenStream.SETNAME :
                iCodeTop = generateICode(child, iCodeTop);
                child = child.getNext();
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addByte(TokenStream.SETNAME, iCodeTop);
                iCodeTop = addString(firstChild.getString(), iCodeTop);
                itsStackDepth--;
                break;

            case TokenStream.TYPEOF : {
                String name = node.getString();
                int index = -1;
                // use typeofname if an activation frame exists
                // since the vars all exist there instead of in jregs
                if (itsInFunctionFlag && !itsData.itsNeedsActivation)
                    index = itsVariableTable.getOrdinal(name);
                if (index == -1) {
                    iCodeTop = addByte(TokenStream.TYPEOFNAME, iCodeTop);
                    iCodeTop = addString(name, iCodeTop);
                } else {
                    iCodeTop = addByte(TokenStream.GETVAR, iCodeTop);
                    iCodeTop = addByte(index, iCodeTop);
                    iCodeTop = addByte(TokenStream.TYPEOF, iCodeTop);
                }
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;
            }

            case TokenStream.PARENT :
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addByte(TokenStream.GETPARENT, iCodeTop);
                break;

            case TokenStream.GETBASE :
            case TokenStream.BINDNAME :
            case TokenStream.NAME :
            case TokenStream.STRING :
                iCodeTop = addByte(type, iCodeTop);
                iCodeTop = addString(node.getString(), iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;

            case TokenStream.INC :
            case TokenStream.DEC : {
                int childType = child.getType();
                switch (childType) {
                    case TokenStream.GETVAR : {
                        String name = child.getString();
                        if (itsData.itsNeedsActivation) {
                            iCodeTop = addByte(TokenStream.SCOPE, iCodeTop);
                            iCodeTop = addByte(TokenStream.STRING, iCodeTop);
                            iCodeTop = addString(name, iCodeTop);
                            itsStackDepth += 2;
                            if (itsStackDepth > itsData.itsMaxStack)
                                itsData.itsMaxStack = itsStackDepth;
                            iCodeTop = addByte(type == TokenStream.INC
                                               ? TokenStream.PROPINC
                                               : TokenStream.PROPDEC,
                                               iCodeTop);
                            itsStackDepth--;
                        } else {
                            int i = itsVariableTable.getOrdinal(name);
                            iCodeTop = addByte(type == TokenStream.INC
                                               ? TokenStream.VARINC
                                               : TokenStream.VARDEC,
                                               iCodeTop);
                            iCodeTop = addByte(i, iCodeTop);
                            itsStackDepth++;
                            if (itsStackDepth > itsData.itsMaxStack)
                                itsData.itsMaxStack = itsStackDepth;
                        }
                        break;
                    }
                    case TokenStream.GETPROP :
                    case TokenStream.GETELEM : {
                        Node getPropChild = child.getFirstChild();
                        iCodeTop = generateICode(getPropChild, iCodeTop);
                        getPropChild = getPropChild.getNext();
                        iCodeTop = generateICode(getPropChild, iCodeTop);
                        if (childType == TokenStream.GETPROP) {
                            iCodeTop = addByte(type == TokenStream.INC
                                               ? TokenStream.PROPINC
                                               : TokenStream.PROPDEC,
                                               iCodeTop);
                        } else {
                            iCodeTop = addByte(type == TokenStream.INC
                                               ? TokenStream.ELEMINC
                                               : TokenStream.ELEMDEC,
                                               iCodeTop);
                        }
                        itsStackDepth--;
                        break;
                    }
                    default : {
                        iCodeTop = addByte(type == TokenStream.INC
                                           ? TokenStream.NAMEINC
                                           : TokenStream.NAMEDEC,
                                           iCodeTop);
                        iCodeTop = addString(child.getString(), iCodeTop);
                        itsStackDepth++;
                        if (itsStackDepth > itsData.itsMaxStack)
                            itsData.itsMaxStack = itsStackDepth;
                        break;
                    }
                }
                break;
            }

            case TokenStream.NUMBER : {
                double num = node.getDouble();
                int inum = (int)num;
                if (inum == num) {
                    if (inum == 0) {
                        iCodeTop = addByte(TokenStream.ZERO, iCodeTop);
                    } else if (inum == 1) {
                        iCodeTop = addByte(TokenStream.ONE, iCodeTop);
                    } else if ((short)inum == inum) {
                        iCodeTop = addByte(SHORTNUMBER_ICODE, iCodeTop);
                        iCodeTop = addShort(inum, iCodeTop);
                    } else {
                        iCodeTop = addByte(INTNUMBER_ICODE, iCodeTop);
                        iCodeTop = addInt(inum, iCodeTop);
                    }
                } else {
                    iCodeTop = addByte(TokenStream.NUMBER, iCodeTop);
                    iCodeTop = addDouble(num, iCodeTop);
                }
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;
            }

            case TokenStream.POP :
            case TokenStream.POPV :
                iCodeTop = updateLineNumber(node, iCodeTop);
            case TokenStream.ENTERWITH :
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addByte(type, iCodeTop);
                itsStackDepth--;
                break;

            case TokenStream.GETTHIS :
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addByte(type, iCodeTop);
                break;

            case TokenStream.NEWSCOPE :
                iCodeTop = addByte(type, iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;

            case TokenStream.LEAVEWITH :
                iCodeTop = addByte(type, iCodeTop);
                break;

            case TokenStream.TRY : {
                itsTryDepth++;
                if (itsTryDepth > itsData.itsMaxTryDepth)
                    itsData.itsMaxTryDepth = itsTryDepth;
                Node catchTarget = (Node)node.getProp(Node.TARGET_PROP);
                Node finallyTarget = (Node)node.getProp(Node.FINALLY_PROP);
                int tryStart = iCodeTop;
                if (catchTarget == null) {
                    iCodeTop = addByte(TokenStream.TRY, iCodeTop);
                    iCodeTop = addShort(0, iCodeTop);
                } else {
                    iCodeTop = addGoto(catchTarget, TokenStream.TRY, iCodeTop);
                }
                iCodeTop = addShort(0, iCodeTop);

                Node lastChild = null;
                /*
                    when we encounter the child of the catchTarget, we
                    set the stackDepth to 1 to account for the incoming
                    exception object.
                */
                boolean insertedEndTry = false;
                while (child != null) {
                    if (catchTarget != null && lastChild == catchTarget) {
                        itsStackDepth = 1;
                        if (itsStackDepth > itsData.itsMaxStack)
                            itsData.itsMaxStack = itsStackDepth;
                    }
                    /*
                        When the following child is the catchTarget
                        (or the finallyTarget if there are no catches),
                        the current child is the goto at the end of
                        the try statemets, we need to emit the endtry
                        before that goto.
                    */
                    Node nextSibling = child.getNext();
                    if (!insertedEndTry && nextSibling != null &&
                        (nextSibling == catchTarget ||
                         nextSibling == finallyTarget))
                    {
                        iCodeTop = addByte(TokenStream.ENDTRY,
                                           iCodeTop);
                        insertedEndTry = true;
                    }
                    iCodeTop = generateICode(child, iCodeTop);
                    lastChild = child;
                    child = child.getNext();
                }
                itsStackDepth = 0;
                if (finallyTarget != null) {
                    // normal flow goes around the finally handler stublet
                    int skippyJumpStart = iCodeTop;
                    iCodeTop = addForwardGoto(TokenStream.GOTO, iCodeTop);
                    int finallyOffset = iCodeTop - tryStart;
                    recordJumpOffset(tryStart + 3, finallyOffset);
                    // on entry the stack will have the exception object
                    itsStackDepth = 1;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                    int theLocalSlot = itsData.itsMaxLocals++;
                    iCodeTop = addByte(TokenStream.NEWTEMP, iCodeTop);
                    iCodeTop = addByte(theLocalSlot, iCodeTop);
                    iCodeTop = addByte(TokenStream.POP, iCodeTop);
                    iCodeTop = addGoto(finallyTarget, TokenStream.GOSUB,
                                       iCodeTop);
                    iCodeTop = addByte(TokenStream.USETEMP, iCodeTop);
                    iCodeTop = addByte(theLocalSlot, iCodeTop);
                    iCodeTop = addByte(TokenStream.JTHROW, iCodeTop);
                    itsStackDepth = 0;
                    resolveForwardGoto(skippyJumpStart, iCodeTop);
                }
                itsTryDepth--;
                break;
            }

            case TokenStream.THROW :
                iCodeTop = updateLineNumber(node, iCodeTop);
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addByte(TokenStream.THROW, iCodeTop);
                itsStackDepth--;
                break;

            case TokenStream.RETURN :
                iCodeTop = updateLineNumber(node, iCodeTop);
                if (child != null) {
                    iCodeTop = generateICode(child, iCodeTop);
                    iCodeTop = addByte(TokenStream.RETURN, iCodeTop);
                    itsStackDepth--;
                } else {
                    iCodeTop = addByte(RETURN_UNDEF_ICODE, iCodeTop);
                }
                break;

            case TokenStream.GETVAR : {
                String name = node.getString();
                if (itsData.itsNeedsActivation) {
                    // SETVAR handled this by turning into a SETPROP, but
                    // we can't do that to a GETVAR without manufacturing
                    // bogus children. Instead we use a special op to
                    // push the current scope.
                    iCodeTop = addByte(TokenStream.SCOPE, iCodeTop);
                    iCodeTop = addByte(TokenStream.STRING, iCodeTop);
                    iCodeTop = addString(name, iCodeTop);
                    itsStackDepth += 2;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                    iCodeTop = addByte(TokenStream.GETPROP, iCodeTop);
                    itsStackDepth--;
                } else {
                    int index = itsVariableTable.getOrdinal(name);
                    iCodeTop = addByte(TokenStream.GETVAR, iCodeTop);
                    iCodeTop = addByte(index, iCodeTop);
                    itsStackDepth++;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                }
                break;
            }

            case TokenStream.SETVAR : {
                if (itsData.itsNeedsActivation) {
                    child.setType(TokenStream.BINDNAME);
                    node.setType(TokenStream.SETNAME);
                    iCodeTop = generateICode(node, iCodeTop);
                } else {
                    String name = child.getString();
                    child = child.getNext();
                    iCodeTop = generateICode(child, iCodeTop);
                    int index = itsVariableTable.getOrdinal(name);
                    iCodeTop = addByte(TokenStream.SETVAR, iCodeTop);
                    iCodeTop = addByte(index, iCodeTop);
                }
                break;
            }

            case TokenStream.PRIMARY:
                iCodeTop = addByte(node.getOperation(), iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;

            case TokenStream.ENUMINIT :
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addByte(TokenStream.ENUMINIT, iCodeTop);
                iCodeTop = addLocalRef(node, iCodeTop);
                itsStackDepth--;
                break;

            case TokenStream.ENUMNEXT : {
                iCodeTop = addByte(TokenStream.ENUMNEXT, iCodeTop);
                Node init = (Node)node.getProp(Node.ENUM_PROP);
                iCodeTop = addLocalRef(init, iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;
            }

            case TokenStream.ENUMDONE :
                // could release the local here??
                break;

            case TokenStream.REGEXP : {
                Node regexp = (Node) node.getProp(Node.REGEXP_PROP);
                int index = regexp.getExistingIntProp(Node.REGEXP_PROP);
                iCodeTop = addByte(TokenStream.REGEXP, iCodeTop);
                iCodeTop = addIndex(index, iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;
            }

            default :
                badTree(node);
                break;
        }
        return iCodeTop;
    }

    private int addLocalRef(Node node, int iCodeTop)
    {
        int theLocalSlot = node.getIntProp(Node.LOCAL_PROP, -1);
        if (theLocalSlot == -1) {
            theLocalSlot = itsData.itsMaxLocals++;
            node.putIntProp(Node.LOCAL_PROP, theLocalSlot);
        }
        iCodeTop = addByte(theLocalSlot, iCodeTop);
        if (theLocalSlot >= itsData.itsMaxLocals)
            itsData.itsMaxLocals = theLocalSlot + 1;
        return iCodeTop;
    }

    private int getTargetLabel(Node target) {
        int targetLabel = target.getIntProp(Node.LABEL_PROP, -1);
        if (targetLabel == -1) {
            targetLabel = itsLabels.acquireLabel();
            target.putIntProp(Node.LABEL_PROP, targetLabel);
        }
        return targetLabel;
    }

    private void markTargetLabel(Node target, int iCodeTop) {
        int label = getTargetLabel(target);
        itsLabels.markLabel(label, iCodeTop);
    }

    private int addGoto(Node target, int gotoOp, int iCodeTop) {
        int targetLabel = getTargetLabel(target);

        int gotoPC = iCodeTop;
        iCodeTop = addByte(gotoOp, iCodeTop);
        iCodeTop = addShort(0, iCodeTop);

        int targetPC = itsLabels.getLabelPC(targetLabel);
        if (targetPC != -1) {
            recordJumpOffset(gotoPC + 1, targetPC - gotoPC);
        } else {
            itsLabels.addLabelFixup(targetLabel, gotoPC + 1);
        }
        return iCodeTop;
    }

    private int addForwardGoto(int gotoOp, int iCodeTop) {
        iCodeTop = addByte(gotoOp, iCodeTop);
        iCodeTop = addShort(0, iCodeTop);
        return iCodeTop;
    }

    private void resolveForwardGoto(int jumpStart, int iCodeTop) {
        if (jumpStart + 3 > iCodeTop) Context.codeBug();
        int offset = iCodeTop - jumpStart;
        // +1 to write after jump icode
        recordJumpOffset(jumpStart + 1, offset);
    }

    private void recordJumpOffset(int pos, int offset) {
        if (offset != (short)offset) {
            throw Context.reportRuntimeError0("msg.too.big.jump");
        }
        itsData.itsICode[pos] = (byte)(offset >> 8);
        itsData.itsICode[pos + 1] = (byte)offset;
    }

    private int addByte(int b, int iCodeTop) {
        byte[] array = itsData.itsICode;
        if (iCodeTop == array.length) {
            array = increaseICodeCapasity(iCodeTop, 1);
        }
        array[iCodeTop++] = (byte)b;
        return iCodeTop;
    }

    private int addShort(int s, int iCodeTop) {
        byte[] array = itsData.itsICode;
        if (iCodeTop + 2 > array.length) {
            array = increaseICodeCapasity(iCodeTop, 2);
        }
        array[iCodeTop] = (byte)(s >>> 8);
        array[iCodeTop + 1] = (byte)s;
        return iCodeTop + 2;
    }

    private int addIndex(int index, int iCodeTop) {
        if (index < 0) Context.codeBug();
        if (index > 0xFFFF) {
            throw Context.reportRuntimeError0("msg.too.big.index");
        }
        byte[] array = itsData.itsICode;
        if (iCodeTop + 2 > array.length) {
            array = increaseICodeCapasity(iCodeTop, 2);
        }
        array[iCodeTop] = (byte)(index >>> 8);
        array[iCodeTop + 1] = (byte)index;
        return iCodeTop + 2;
    }

    private int addInt(int i, int iCodeTop) {
        byte[] array = itsData.itsICode;
        if (iCodeTop + 4 > array.length) {
            array = increaseICodeCapasity(iCodeTop, 4);
        }
        array[iCodeTop] = (byte)(i >>> 24);
        array[iCodeTop + 1] = (byte)(i >>> 16);
        array[iCodeTop + 2] = (byte)(i >>> 8);
        array[iCodeTop + 3] = (byte)i;
        return iCodeTop + 4;
    }

    private int addDouble(double num, int iCodeTop) {
        int index = itsDoubleTableTop;
        if (index == 0) {
            itsData.itsDoubleTable = new double[64];
        } else if (itsData.itsDoubleTable.length == index) {
            double[] na = new double[index * 2];
            System.arraycopy(itsData.itsDoubleTable, 0, na, 0, index);
            itsData.itsDoubleTable = na;
        }
        itsData.itsDoubleTable[index] = num;
        itsDoubleTableTop = index + 1;

        iCodeTop = addIndex(index, iCodeTop);
        return iCodeTop;
    }

    private int addString(String str, int iCodeTop) {
        int index = itsStrings.get(str, -1);
        if (index == -1) {
            index = itsStrings.size();
            itsStrings.put(str, index);
        }
        iCodeTop = addIndex(index, iCodeTop);
        lastAddString = str;
        return iCodeTop;
    }

    private byte[] increaseICodeCapasity(int iCodeTop, int extraSize) {
        int capacity = itsData.itsICode.length;
        if (iCodeTop + extraSize <= capacity) Context.codeBug();
        capacity *= 2;
        if (iCodeTop + extraSize > capacity) {
            capacity = iCodeTop + extraSize;
        }
        byte[] array = new byte[capacity];
        System.arraycopy(itsData.itsICode, 0, array, 0, iCodeTop);
        itsData.itsICode = array;
        return array;
    }

    private static int getShort(byte[] iCode, int pc) {
        return (iCode[pc] << 8) | (iCode[pc + 1] & 0xFF);
    }

    private static int getIndex(byte[] iCode, int pc) {
        return ((iCode[pc] & 0xFF) << 8) | (iCode[pc + 1] & 0xFF);
    }

    private static int getInt(byte[] iCode, int pc) {
        return (iCode[pc] << 24) | ((iCode[pc + 1] & 0xFF) << 16)
               | ((iCode[pc + 2] & 0xFF) << 8) | (iCode[pc + 3] & 0xFF);
    }

    private static int getTarget(byte[] iCode, int pc) {
        int displacement = getShort(iCode, pc);
        return pc - 1 + displacement;
    }

    static PrintWriter out;
    static {
        if (Context.printICode) {
            try {
                out = new PrintWriter(new FileOutputStream("icode.txt"));
                out.close();
            }
            catch (IOException x) {
            }
        }
    }

    private static String icodeToName(int icode) {
        if (Context.printICode) {
            if (icode <= TokenStream.LAST_TOKEN) {
                return TokenStream.tokenToName(icode);
            } else {
                switch (icode) {
                    case LINE_ICODE:         return "line";
                    case SOURCEFILE_ICODE:   return "sourcefile";
                    case SHORTNUMBER_ICODE:  return "shortnumber";
                    case INTNUMBER_ICODE:    return "intnumber";
                    case RETURN_UNDEF_ICODE: return "return_undef";
                    case END_ICODE:          return "end";
                }
            }
            return "<UNKNOWN ICODE: "+icode+">";
        }
        return "";
    }

    private static void dumpICode(InterpreterData idata) {
        if (Context.printICode) {
            try {
                int iCodeLength = idata.itsICodeTop;
                byte iCode[] = idata.itsICode;
                String[] strings = idata.itsStringTable;

                out = new PrintWriter(new FileOutputStream("icode.txt", true));
                out.println("ICode dump, for " + idata.itsName
                            + ", length = " + iCodeLength);
                out.println("MaxStack = " + idata.itsMaxStack);

                for (int pc = 0; pc < iCodeLength; ) {
                    out.print("[" + pc + "] ");
                    int token = iCode[pc] & 0xff;
                    String tname = icodeToName(token);
                    int old_pc = pc;
                    ++pc;
                    int icodeLength = icodeTokenLength(token);
                    switch (token) {
                        default:
                            if (icodeLength != 1) Context.codeBug();
                            out.println(tname);
                            break;

                        case TokenStream.GOSUB :
                        case TokenStream.GOTO :
                        case TokenStream.IFEQ :
                        case TokenStream.IFNE : {
                            int newPC = getTarget(iCode, pc);
                            out.println(tname + " " + newPC);
                            pc += 2;
                            break;
                        }
                        case TokenStream.TRY : {
                            int catch_offset = getShort(iCode, pc);
                            int finally_offset = getShort(iCode, pc + 2);
                            int catch_pc = (catch_offset == 0)
                                           ? -1 : pc - 1 + catch_offset;
                            int finally_pc = (finally_offset == 0)
                                           ? -1 : pc - 1 + finally_offset;
                            out.println(tname + " " + catch_pc
                                        + " " + finally_pc);
                            pc += 4;
                            break;
                        }
                        case TokenStream.RETSUB :
                        case TokenStream.ENUMINIT :
                        case TokenStream.ENUMNEXT :
                        case TokenStream.VARINC :
                        case TokenStream.VARDEC :
                        case TokenStream.GETVAR :
                        case TokenStream.SETVAR :
                        case TokenStream.NEWTEMP :
                        case TokenStream.USETEMP : {
                            int slot = (iCode[pc] & 0xFF);
                            out.println(tname + " " + slot);
                            pc++;
                            break;
                        }
                        case TokenStream.CALLSPECIAL : {
                            int line = getShort(iCode, pc);
                            String name = strings[getIndex(iCode, pc + 2)];
                            int count = getIndex(iCode, pc + 4);
                            out.println(tname + " " + count
                                        + " " + line + " " + name);
                            pc += 6;
                            break;
                        }
                        case TokenStream.REGEXP : {
                            int i = getIndex(iCode, pc);
                            Object regexp = idata.itsRegExpLiterals[i];
                            out.println(tname + " " + regexp);
                            pc += 2;
                            break;
                        }
                        case TokenStream.CLOSURE : {
                            int i = getIndex(iCode, pc);
                            InterpreterData data2 = idata.itsNestedFunctions[i];
                            out.println(tname + " " + data2);
                            pc += 2;
                            break;
                        }
                        case TokenStream.NEW :
                        case TokenStream.CALL : {
                            int count = getIndex(iCode, pc + 2);
                            String name = strings[getIndex(iCode, pc)];
                            out.println(tname + " " + count + " \""
                                        + name + '"');
                            pc += 4;
                            break;
                        }
                        case SHORTNUMBER_ICODE : {
                            int value = getShort(iCode, pc);
                            out.println(tname + " " + value);
                            pc += 2;
                            break;
                        }
                        case INTNUMBER_ICODE : {
                            int value = getInt(iCode, pc);
                            out.println(tname + " " + value);
                            pc += 4;
                            break;
                        }
                        case TokenStream.NUMBER : {
                            int index = getIndex(iCode, pc);
                            double value = idata.itsDoubleTable[index];
                            out.println(tname + " " + value);
                            pc += 2;
                            break;
                        }
                        case TokenStream.TYPEOFNAME :
                        case TokenStream.GETBASE :
                        case TokenStream.BINDNAME :
                        case TokenStream.SETNAME :
                        case TokenStream.NAME :
                        case TokenStream.NAMEINC :
                        case TokenStream.NAMEDEC :
                        case TokenStream.STRING : {
                            String str = strings[getIndex(iCode, pc)];
                            out.println(tname + " \"" + str + '"');
                            pc += 2;
                            break;
                        }
                        case LINE_ICODE : {
                            int line = getShort(iCode, pc);
                            out.println(tname + " : " + line);
                            pc += 2;
                            break;
                        }
                    }
                    if (old_pc + icodeLength != pc) Context.codeBug();
                }
                out.close();
            }
            catch (IOException x) {}
        }
    }

    private static int icodeTokenLength(int icodeToken) {
        switch (icodeToken) {
            case TokenStream.SCOPE :
            case TokenStream.GETPROTO :
            case TokenStream.GETPARENT :
            case TokenStream.GETSCOPEPARENT :
            case TokenStream.SETPROTO :
            case TokenStream.SETPARENT :
            case TokenStream.DELPROP :
            case TokenStream.TYPEOF :
            case TokenStream.NEWSCOPE :
            case TokenStream.ENTERWITH :
            case TokenStream.LEAVEWITH :
            case TokenStream.RETURN :
            case TokenStream.ENDTRY :
            case TokenStream.THROW :
            case TokenStream.JTHROW :
            case TokenStream.GETTHIS :
            case TokenStream.SETELEM :
            case TokenStream.GETELEM :
            case TokenStream.SETPROP :
            case TokenStream.GETPROP :
            case TokenStream.PROPINC :
            case TokenStream.PROPDEC :
            case TokenStream.ELEMINC :
            case TokenStream.ELEMDEC :
            case TokenStream.BITNOT :
            case TokenStream.BITAND :
            case TokenStream.BITOR :
            case TokenStream.BITXOR :
            case TokenStream.LSH :
            case TokenStream.RSH :
            case TokenStream.URSH :
            case TokenStream.NEG :
            case TokenStream.POS :
            case TokenStream.SUB :
            case TokenStream.MUL :
            case TokenStream.DIV :
            case TokenStream.MOD :
            case TokenStream.ADD :
            case TokenStream.POPV :
            case TokenStream.POP :
            case TokenStream.DUP :
            case TokenStream.LT :
            case TokenStream.GT :
            case TokenStream.LE :
            case TokenStream.GE :
            case TokenStream.IN :
            case TokenStream.INSTANCEOF :
            case TokenStream.EQ :
            case TokenStream.NE :
            case TokenStream.SHEQ :
            case TokenStream.SHNE :
            case TokenStream.ZERO :
            case TokenStream.ONE :
            case TokenStream.NULL :
            case TokenStream.THIS :
            case TokenStream.THISFN :
            case TokenStream.FALSE :
            case TokenStream.TRUE :
            case TokenStream.UNDEFINED :
            case SOURCEFILE_ICODE :
            case RETURN_UNDEF_ICODE:
            case END_ICODE:
                return 1;

            case TokenStream.GOSUB :
            case TokenStream.GOTO :
            case TokenStream.IFEQ :
            case TokenStream.IFNE :
                // target pc offset
                return 1 + 2;

            case TokenStream.TRY :
                // catch pc offset or 0
                // finally pc offset or 0
                return 1 + 2 + 2;

            case TokenStream.RETSUB :
            case TokenStream.ENUMINIT :
            case TokenStream.ENUMNEXT :
            case TokenStream.VARINC :
            case TokenStream.VARDEC :
            case TokenStream.GETVAR :
            case TokenStream.SETVAR :
            case TokenStream.NEWTEMP :
            case TokenStream.USETEMP :
                // slot index
                return 1 + 1;

            case TokenStream.CALLSPECIAL :
                // line number
                // name string index
                // arg count
                return 1 + 2 + 2 + 2;

            case TokenStream.REGEXP :
                // regexp index
                return 1 + 2;

            case TokenStream.CLOSURE :
                // index of closure master copy
                return 1 + 2;

            case TokenStream.NEW :
            case TokenStream.CALL :
                // name string index
                // arg count
                return 1 + 2 + 2;

            case SHORTNUMBER_ICODE :
                // short number
                return 1 + 2;

            case INTNUMBER_ICODE :
                // int number
                return 1 + 4;

            case TokenStream.NUMBER :
                // index of double number
                return 1 + 2;

            case TokenStream.TYPEOFNAME :
            case TokenStream.GETBASE :
            case TokenStream.BINDNAME :
            case TokenStream.SETNAME :
            case TokenStream.NAME :
            case TokenStream.NAMEINC :
            case TokenStream.NAMEDEC :
            case TokenStream.STRING :
                // string index
                return 1 + 2;

            case LINE_ICODE :
                // line number
                return 1 + 2;

            default:
                Context.codeBug(); // Bad icodeToken
                return 0;
        }
    }

    static int[] getLineNumbers(InterpreterData data) {
        UintMap presentLines = new UintMap();

        int iCodeLength = data.itsICodeTop;
        byte[] iCode = data.itsICode;
        for (int pc = 0; pc != iCodeLength;) {
            int icodeToken = iCode[pc] & 0xff;
            int icodeLength = icodeTokenLength(icodeToken);
            if (icodeToken == LINE_ICODE) {
                if (icodeLength != 3) Context.codeBug();
                int line = getShort(iCode, pc + 1);
                presentLines.put(line, 0);
            }
            pc += icodeLength;
        }

        return presentLines.getKeys();
    }

    private static InterpretedFunction createFunction(Context cx,
                                                      Scriptable scope,
                                                      InterpreterData idata,
                                                      boolean fromEvalCode)
    {
        InterpretedFunction fn = new InterpretedFunction(cx, idata);
        fn.setPrototype(ScriptableObject.getFunctionPrototype(scope));
        fn.setParentScope(scope);
        if (cx.hasCompileFunctionsWithDynamicScope()) {
            fn.itsUseDynamicScope = true;
        }
        String fnName = idata.itsName;
        if (fnName.length() != 0) {
            int type = idata.itsFunctionType;
            if (type == FunctionNode.FUNCTION_STATEMENT) {
                if (fromEvalCode) {
                    scope.put(fnName, scope, fn);
                } else {
                    // ECMA specifies that functions defined in global and
                    // function scope should have DONTDELETE set.
                    ScriptableObject.defineProperty(scope,
                            fnName, fn, ScriptableObject.PERMANENT);
                }
            } else if (type == FunctionNode.FUNCTION_EXPRESSION_STATEMENT) {
                scope.put(fnName, scope, fn);
            }
        }
        return fn;
    }

    static Object interpret(Context cx, Scriptable scope, Scriptable thisObj,
                            Object[] args, double[] argsDbl,
                            int argShift, int argCount,
                            NativeFunction fnOrScript,
                            InterpreterData idata)
        throws JavaScriptException
    {
        if (cx.interpreterSecurityDomain != idata.securityDomain) {
            return execWithNewDomain(cx, scope, thisObj,
                                     args, argsDbl, argShift, argCount,
                                     fnOrScript, idata);
        }

        final Object DBL_MRK = Interpreter.DBL_MRK;
        final Scriptable undefined = Undefined.instance;

        final int VAR_SHFT = 0;
        final int maxVars = idata.itsMaxVars;
        final int LOCAL_SHFT = VAR_SHFT + maxVars;
        final int TRY_STACK_SHFT = LOCAL_SHFT + idata.itsMaxLocals;
        final int STACK_SHFT = TRY_STACK_SHFT + idata.itsMaxTryDepth;

// stack[VAR_SHFT <= i < LOCAL_SHFT]: variables
// stack[LOCAL_SHFT <= i < TRY_STACK_SHFT]: used for newtemp/usetemp
// stack[TRY_STACK_SHFT <= i < STACK_SHFT]: stack of try scopes
// stack[STACK_SHFT <= i < STACK_SHFT + idata.itsMaxStack]: stack data

// sDbl[TRY_STACK_SHFT <= i < STACK_SHFT]: stack of try block pc, stored as doubles
// sDbl[any other i]: if stack[i] is DBL_MRK, sDbl[i] holds the number value

        int maxFrameArray = idata.itsMaxFrameArray;
        if (maxFrameArray != STACK_SHFT + idata.itsMaxStack)
            Context.codeBug();

        Object[] stack = new Object[maxFrameArray];
        double[] sDbl = new double[maxFrameArray];

        int stackTop = STACK_SHFT - 1;
        int tryStackTop = 0; // add TRY_STACK_SHFT to get real index

        int definedArgs = fnOrScript.argCount;
        if (definedArgs > argCount) { definedArgs = argCount; }
        for (int i = 0; i != definedArgs; ++i) {
            Object arg = args[argShift + i];
            stack[VAR_SHFT + i] = arg;
            if (arg == DBL_MRK) {
                sDbl[VAR_SHFT + i] = argsDbl[argShift + i];
            }
        }
        for (int i = definedArgs; i != maxVars; ++i) {
            stack[VAR_SHFT + i] = undefined;
        }

        DebugFrame debuggerFrame = null;
        if (cx.debugger != null) {
            debuggerFrame = cx.debugger.getFrame(cx, idata);
        }

        if (idata.itsFunctionType != 0) {
            InterpretedFunction f = (InterpretedFunction)fnOrScript;
            if (f.itsClosure != null) {
                scope = f.itsClosure;
            } else if (!f.itsUseDynamicScope) {
                scope = fnOrScript.getParentScope();
            }

            if (idata.itsCheckThis) {
                thisObj = ScriptRuntime.getThis(thisObj);
            }

            if (idata.itsNeedsActivation) {
                if (argsDbl != null) {
                    args = getArgsArray(args, argsDbl, argShift, argCount);
                    argShift = 0;
                    argsDbl = null;
                }
                scope = ScriptRuntime.initVarObj(cx, scope, fnOrScript,
                                                 thisObj, args);
            }

        } else {
            scope = ScriptRuntime.initScript(cx, scope, fnOrScript, thisObj,
                                             idata.itsFromEvalCode);
        }

        if (idata.itsNestedFunctions != null) {
            if (idata.itsFunctionType != 0 && !idata.itsNeedsActivation)
                Context.codeBug();
            for (int i = 0; i < idata.itsNestedFunctions.length; i++) {
                InterpreterData fdata = idata.itsNestedFunctions[i];
                if (fdata.itsFunctionType == FunctionNode.FUNCTION_STATEMENT) {
                    createFunction(cx, scope, fdata, idata.itsFromEvalCode);
                }
            }
        }

        boolean useActivationVars = false;
        if (debuggerFrame != null) {
            if (argsDbl != null) {
                args = getArgsArray(args, argsDbl, argShift, argCount);
                argShift = 0;
                argsDbl = null;
            }
            if (idata.itsFunctionType != 0 && !idata.itsNeedsActivation) {
                useActivationVars = true;
                scope = ScriptRuntime.initVarObj(cx, scope, fnOrScript,
                                                 thisObj, args);
            }
            debuggerFrame.onEnter(cx, scope, thisObj, args);
        }

        Object result = undefined;

        byte[] iCode = idata.itsICode;
        String[] strings = idata.itsStringTable;
        int pc = 0;

        int pcPrevBranch = pc;
        final int instructionThreshold = cx.instructionThreshold;
        // During function call this will be set to -1 so catch can properly
        // adjust it
        int instructionCount = cx.instructionCount;
        // arbitrary number to add to instructionCount when calling
        // other functions
        final int INVOCATION_COST = 100;

        Loop: while (true) {
            try {
                switch (iCode[pc] & 0xff) {
    // Back indent to ease imlementation reading

    case TokenStream.ENDTRY :
        tryStackTop--;
        break;
    case TokenStream.TRY :
        stack[TRY_STACK_SHFT + tryStackTop] = scope;
        sDbl[TRY_STACK_SHFT + tryStackTop] = (double)pc;
        ++tryStackTop;
        // Skip starting pc of catch/finally blocks
        pc += 4;
        break;
    case TokenStream.GE : {
        --stackTop;
        Object rhs = stack[stackTop + 1];
        Object lhs = stack[stackTop];
        boolean valBln;
        if (rhs == DBL_MRK || lhs == DBL_MRK) {
            double rDbl = stack_double(stack, sDbl, stackTop + 1);
            double lDbl = stack_double(stack, sDbl, stackTop);
            valBln = (rDbl == rDbl && lDbl == lDbl && rDbl <= lDbl);
        } else {
            valBln = (1 == ScriptRuntime.cmp_LE(rhs, lhs));
        }
        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
        break;
    }
    case TokenStream.LE : {
        --stackTop;
        Object rhs = stack[stackTop + 1];
        Object lhs = stack[stackTop];
        boolean valBln;
        if (rhs == DBL_MRK || lhs == DBL_MRK) {
            double rDbl = stack_double(stack, sDbl, stackTop + 1);
            double lDbl = stack_double(stack, sDbl, stackTop);
            valBln = (rDbl == rDbl && lDbl == lDbl && lDbl <= rDbl);
        } else {
            valBln = (1 == ScriptRuntime.cmp_LE(lhs, rhs));
        }
        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
        break;
    }
    case TokenStream.GT : {
        --stackTop;
        Object rhs = stack[stackTop + 1];
        Object lhs = stack[stackTop];
        boolean valBln;
        if (rhs == DBL_MRK || lhs == DBL_MRK) {
            double rDbl = stack_double(stack, sDbl, stackTop + 1);
            double lDbl = stack_double(stack, sDbl, stackTop);
            valBln = (rDbl == rDbl && lDbl == lDbl && rDbl < lDbl);
        } else {
            valBln = (1 == ScriptRuntime.cmp_LT(rhs, lhs));
        }
        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
        break;
    }
    case TokenStream.LT : {
        --stackTop;
        Object rhs = stack[stackTop + 1];
        Object lhs = stack[stackTop];
        boolean valBln;
        if (rhs == DBL_MRK || lhs == DBL_MRK) {
            double rDbl = stack_double(stack, sDbl, stackTop + 1);
            double lDbl = stack_double(stack, sDbl, stackTop);
            valBln = (rDbl == rDbl && lDbl == lDbl && lDbl < rDbl);
        } else {
            valBln = (1 == ScriptRuntime.cmp_LT(lhs, rhs));
        }
        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
        break;
    }
    case TokenStream.IN : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        boolean valBln = ScriptRuntime.in(lhs, rhs, scope);
        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
        break;
    }
    case TokenStream.INSTANCEOF : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        boolean valBln = ScriptRuntime.instanceOf(scope, lhs, rhs);
        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
        break;
    }
    case TokenStream.EQ : {
        --stackTop;
        boolean valBln = do_eq(stack, sDbl, stackTop);
        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
        break;
    }
    case TokenStream.NE : {
        --stackTop;
        boolean valBln = !do_eq(stack, sDbl, stackTop);
        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
        break;
    }
    case TokenStream.SHEQ : {
        --stackTop;
        boolean valBln = do_sheq(stack, sDbl, stackTop);
        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
        break;
    }
    case TokenStream.SHNE : {
        --stackTop;
        boolean valBln = !do_sheq(stack, sDbl, stackTop);
        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
        break;
    }
    case TokenStream.IFNE : {
        Object val = stack[stackTop];
        boolean valBln;
        if (val != DBL_MRK) {
            valBln = !ScriptRuntime.toBoolean(val);
        } else {
            double valDbl = sDbl[stackTop];
            valBln = !(valDbl == valDbl && valDbl != 0.0);
        }
        --stackTop;
        if (valBln) {
            if (instructionThreshold != 0) {
                instructionCount += pc + 3 - pcPrevBranch;
                if (instructionCount > instructionThreshold) {
                    cx.observeInstructionCount(instructionCount);
                    instructionCount = 0;
                }
            }
            pcPrevBranch = pc = getTarget(iCode, pc + 1);
            continue;
        }
        pc += 2;
        break;
    }
    case TokenStream.IFEQ : {
        boolean valBln;
        Object val = stack[stackTop];
        if (val != DBL_MRK) {
            valBln = ScriptRuntime.toBoolean(val);
        } else {
            double valDbl = sDbl[stackTop];
            valBln = (valDbl == valDbl && valDbl != 0.0);
        }
        --stackTop;
        if (valBln) {
            if (instructionThreshold != 0) {
                instructionCount += pc + 3 - pcPrevBranch;
                if (instructionCount > instructionThreshold) {
                    cx.observeInstructionCount(instructionCount);
                    instructionCount = 0;
                }
            }
            pcPrevBranch = pc = getTarget(iCode, pc + 1);
            continue;
        }
        pc += 2;
        break;
    }
    case TokenStream.GOTO :
        if (instructionThreshold != 0) {
            instructionCount += pc + 3 - pcPrevBranch;
            if (instructionCount > instructionThreshold) {
                cx.observeInstructionCount(instructionCount);
                instructionCount = 0;
            }
        }
        pcPrevBranch = pc = getTarget(iCode, pc + 1);
        continue;
    case TokenStream.GOSUB :
        sDbl[++stackTop] = pc + 3;
        if (instructionThreshold != 0) {
            instructionCount += pc + 3 - pcPrevBranch;
            if (instructionCount > instructionThreshold) {
                cx.observeInstructionCount(instructionCount);
                instructionCount = 0;
            }
        }
        pcPrevBranch = pc = getTarget(iCode, pc + 1);                                    continue;
    case TokenStream.RETSUB : {
        int slot = (iCode[pc + 1] & 0xFF);
        if (instructionThreshold != 0) {
            instructionCount += pc + 2 - pcPrevBranch;
            if (instructionCount > instructionThreshold) {
                cx.observeInstructionCount(instructionCount);
                instructionCount = 0;
            }
        }
        pcPrevBranch = pc = (int)sDbl[LOCAL_SHFT + slot];
        continue;
    }
    case TokenStream.POP :
        stackTop--;
        break;
    case TokenStream.DUP :
        stack[stackTop + 1] = stack[stackTop];
        sDbl[stackTop + 1] = sDbl[stackTop];
        stackTop++;
        break;
    case TokenStream.POPV :
        result = stack[stackTop];
        if (result == DBL_MRK) result = doubleWrap(sDbl[stackTop]);
        --stackTop;
        break;
    case TokenStream.RETURN :
        result = stack[stackTop];
        if (result == DBL_MRK) result = doubleWrap(sDbl[stackTop]);
        --stackTop;
        break Loop;
    case RETURN_UNDEF_ICODE :
        result = undefined;
        break Loop;
    case END_ICODE:
        break Loop;
    case TokenStream.BITNOT : {
        int rIntValue = stack_int32(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = ~rIntValue;
        break;
    }
    case TokenStream.BITAND : {
        int rIntValue = stack_int32(stack, sDbl, stackTop);
        --stackTop;
        int lIntValue = stack_int32(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lIntValue & rIntValue;
        break;
    }
    case TokenStream.BITOR : {
        int rIntValue = stack_int32(stack, sDbl, stackTop);
        --stackTop;
        int lIntValue = stack_int32(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lIntValue | rIntValue;
        break;
    }
    case TokenStream.BITXOR : {
        int rIntValue = stack_int32(stack, sDbl, stackTop);
        --stackTop;
        int lIntValue = stack_int32(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lIntValue ^ rIntValue;
        break;
    }
    case TokenStream.LSH : {
        int rIntValue = stack_int32(stack, sDbl, stackTop);
        --stackTop;
        int lIntValue = stack_int32(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lIntValue << rIntValue;
        break;
    }
    case TokenStream.RSH : {
        int rIntValue = stack_int32(stack, sDbl, stackTop);
        --stackTop;
        int lIntValue = stack_int32(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lIntValue >> rIntValue;
        break;
    }
    case TokenStream.URSH : {
        int rIntValue = stack_int32(stack, sDbl, stackTop) & 0x1F;
        --stackTop;
        double lDbl = stack_double(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = ScriptRuntime.toUint32(lDbl) >>> rIntValue;
        break;
    }
    case TokenStream.ADD :
        --stackTop;
        do_add(stack, sDbl, stackTop);
        break;
    case TokenStream.SUB : {
        double rDbl = stack_double(stack, sDbl, stackTop);
        --stackTop;
        double lDbl = stack_double(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lDbl - rDbl;
        break;
    }
    case TokenStream.NEG : {
        double rDbl = stack_double(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = -rDbl;
        break;
    }
    case TokenStream.POS : {
        double rDbl = stack_double(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = rDbl;
        break;
    }
    case TokenStream.MUL : {
        double rDbl = stack_double(stack, sDbl, stackTop);
        --stackTop;
        double lDbl = stack_double(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lDbl * rDbl;
        break;
    }
    case TokenStream.DIV : {
        double rDbl = stack_double(stack, sDbl, stackTop);
        --stackTop;
        double lDbl = stack_double(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        // Detect the divide by zero or let Java do it ?
        sDbl[stackTop] = lDbl / rDbl;
        break;
    }
    case TokenStream.MOD : {
        double rDbl = stack_double(stack, sDbl, stackTop);
        --stackTop;
        double lDbl = stack_double(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lDbl % rDbl;
        break;
    }
    case TokenStream.BINDNAME : {
        String name = strings[getIndex(iCode, pc + 1)];
        stack[++stackTop] = ScriptRuntime.bind(scope, name);
        pc += 2;
        break;
    }
    case TokenStream.GETBASE : {
        String name = strings[getIndex(iCode, pc + 1)];
        stack[++stackTop] = ScriptRuntime.getBase(scope, name);
        pc += 2;
        break;
    }
    case TokenStream.SETNAME : {
        String name = strings[getIndex(iCode, pc + 1)];
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        // what about class cast exception here for lhs?
        Scriptable lhs = (Scriptable)stack[stackTop];
        stack[stackTop] = ScriptRuntime.setName(lhs, rhs, scope, name);
        pc += 2;
        break;
    }
    case TokenStream.DELPROP : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.delete(lhs, rhs);
        break;
    }
    case TokenStream.GETPROP : {
        String name = (String)stack[stackTop];
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.getProp(lhs, name, scope);
        break;
    }
    case TokenStream.SETPROP : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        String name = (String)stack[stackTop];
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.setProp(lhs, name, rhs, scope);
        break;
    }
    case TokenStream.GETELEM :
        do_getElem(cx, stack, sDbl, stackTop, scope);
        --stackTop;
        break;
    case TokenStream.SETELEM :
        do_setElem(cx, stack, sDbl, stackTop, scope);
        stackTop -= 2;
        break;
    case TokenStream.PROPINC : {
        String name = (String)stack[stackTop];
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.postIncrement(lhs, name, scope);
        break;
    }
    case TokenStream.PROPDEC : {
        String name = (String)stack[stackTop];
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.postDecrement(lhs, name, scope);
        break;
    }
    case TokenStream.ELEMINC : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.postIncrementElem(lhs, rhs, scope);
        break;
    }
    case TokenStream.ELEMDEC : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.postDecrementElem(lhs, rhs, scope);
        break;
    }
    case TokenStream.GETTHIS : {
        Scriptable lhs = (Scriptable)stack[stackTop];
        stack[stackTop] = ScriptRuntime.getThis(lhs);
        break;
    }
    case TokenStream.NEWTEMP : {
        int slot = (iCode[++pc] & 0xFF);
        stack[LOCAL_SHFT + slot] = stack[stackTop];
        sDbl[LOCAL_SHFT + slot] = sDbl[stackTop];
        break;
    }
    case TokenStream.USETEMP : {
        int slot = (iCode[++pc] & 0xFF);
        ++stackTop;
        stack[stackTop] = stack[LOCAL_SHFT + slot];
        sDbl[stackTop] = sDbl[LOCAL_SHFT + slot];
        break;
    }
    case TokenStream.CALLSPECIAL : {
        if (instructionThreshold != 0) {
            instructionCount += INVOCATION_COST;
            cx.instructionCount = instructionCount;
            instructionCount = -1;
        }
        int lineNum = getShort(iCode, pc + 1);
        String name = strings[getIndex(iCode, pc + 3)];
        int count = getIndex(iCode, pc + 5);
        stackTop -= count;
        Object[] outArgs = getArgsArray(stack, sDbl, stackTop + 1, count);
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.callSpecial(
                            cx, lhs, rhs, outArgs,
                            thisObj, scope, name, lineNum);
        pc += 6;
        instructionCount = cx.instructionCount;
        break;
    }
    case TokenStream.CALL : {
        if (instructionThreshold != 0) {
            instructionCount += INVOCATION_COST;
            cx.instructionCount = instructionCount;
            instructionCount = -1;
        }
        cx.instructionCount = instructionCount;
        int count = getIndex(iCode, pc + 3);
        stackTop -= count;
        int calleeArgShft = stackTop + 1;
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];

        Scriptable calleeScope = scope;
        if (idata.itsNeedsActivation) {
            calleeScope = ScriptableObject.getTopLevelScope(scope);
        }

        Scriptable calleeThis;
        if (rhs instanceof Scriptable || rhs == null) {
            calleeThis = (Scriptable)rhs;
        } else {
            calleeThis = ScriptRuntime.toObject(cx, calleeScope, rhs);
        }

        if (lhs instanceof InterpretedFunction) {
            // Inlining of InterpretedFunction.call not to create
            // argument array
            InterpretedFunction f = (InterpretedFunction)lhs;
            stack[stackTop] = interpret(cx, calleeScope, calleeThis,
                                        stack, sDbl, calleeArgShft, count,
                                        f, f.itsData);
        } else if (lhs instanceof Function) {
            Function f = (Function)lhs;
            Object[] outArgs = getArgsArray(stack, sDbl, calleeArgShft, count);
            stack[stackTop] = f.call(cx, calleeScope, calleeThis, outArgs);
        } else {
            if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
            else if (lhs == undefined) {
                // special code for better error message for call
                // to undefined
                lhs = strings[getIndex(iCode, pc + 1)];
                if (lhs == null) lhs = undefined;
            }
            throw NativeGlobal.typeError1
                ("msg.isnt.function", ScriptRuntime.toString(lhs), calleeScope);
        }

        pc += 4;
        instructionCount = cx.instructionCount;
        break;
    }
    case TokenStream.NEW : {
        if (instructionThreshold != 0) {
            instructionCount += INVOCATION_COST;
            cx.instructionCount = instructionCount;
            instructionCount = -1;
        }
        int count = getIndex(iCode, pc + 3);
        stackTop -= count;
        int calleeArgShft = stackTop + 1;
        Object lhs = stack[stackTop];

        if (lhs instanceof InterpretedFunction) {
            // Inlining of InterpretedFunction.construct not to create
            // argument array
            InterpretedFunction f = (InterpretedFunction)lhs;
            Scriptable newInstance = f.createObject(cx, scope);
            Object callResult = interpret(cx, scope, newInstance,
                                          stack, sDbl, calleeArgShft, count,
                                          f, f.itsData);
            if (callResult instanceof Scriptable && callResult != undefined) {
                stack[stackTop] = callResult;
            } else {
                stack[stackTop] = newInstance;
            }
        } else if (lhs instanceof Function) {
            Function f = (Function)lhs;
            Object[] outArgs = getArgsArray(stack, sDbl, calleeArgShft, count);
            stack[stackTop] = f.construct(cx, scope, outArgs);
        } else {
            if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
            else if (lhs == undefined) {
                // special code for better error message for call
                // to undefined
                lhs = strings[getIndex(iCode, pc + 1)];
                if (lhs == null) lhs = undefined;
            }
            throw NativeGlobal.typeError1
                ("msg.isnt.function", ScriptRuntime.toString(lhs), scope);

        }
        pc += 4;                                                                         instructionCount = cx.instructionCount;
        break;
    }
    case TokenStream.TYPEOF : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.typeof(lhs);
        break;
    }
    case TokenStream.TYPEOFNAME : {
        String name = strings[getIndex(iCode, pc + 1)];
        stack[++stackTop] = ScriptRuntime.typeofName(scope, name);
        pc += 2;
        break;
    }
    case TokenStream.STRING :
        stack[++stackTop] = strings[getIndex(iCode, pc + 1)];
        pc += 2;
        break;
    case SHORTNUMBER_ICODE :
        ++stackTop;
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = getShort(iCode, pc + 1);
        pc += 2;
        break;
    case INTNUMBER_ICODE :
        ++stackTop;
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = getInt(iCode, pc + 1);
        pc += 4;
        break;
    case TokenStream.NUMBER :
        ++stackTop;
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = idata.itsDoubleTable[getIndex(iCode, pc + 1)];
        pc += 2;
        break;
    case TokenStream.NAME : {
        String name = strings[getIndex(iCode, pc + 1)];
        stack[++stackTop] = ScriptRuntime.name(scope, name);
        pc += 2;
        break;
    }
    case TokenStream.NAMEINC : {
        String name = strings[getIndex(iCode, pc + 1)];
        stack[++stackTop] = ScriptRuntime.postIncrement(scope, name);
        pc += 2;
        break;
    }
    case TokenStream.NAMEDEC : {
        String name = strings[getIndex(iCode, pc + 1)];
        stack[++stackTop] = ScriptRuntime.postDecrement(scope, name);
        pc += 2;
        break;
    }
    case TokenStream.SETVAR : {
        int slot = (iCode[++pc] & 0xFF);
        if (!useActivationVars) {
            stack[VAR_SHFT + slot] = stack[stackTop];
            sDbl[VAR_SHFT + slot] = sDbl[stackTop];
        } else {
            Object val = stack[stackTop];
            if (val == DBL_MRK) val = doubleWrap(sDbl[stackTop]);
            activationPut(fnOrScript, scope, slot, val);
        }
        break;
    }
    case TokenStream.GETVAR : {
        int slot = (iCode[++pc] & 0xFF);
        ++stackTop;
        if (!useActivationVars) {
            stack[stackTop] = stack[VAR_SHFT + slot];
            sDbl[stackTop] = sDbl[VAR_SHFT + slot];
        } else {
            stack[stackTop] = activationGet(fnOrScript, scope, slot);
        }
        break;
    }
    case TokenStream.VARINC : {
        int slot = (iCode[++pc] & 0xFF);
        ++stackTop;
        if (!useActivationVars) {
            stack[stackTop] = stack[VAR_SHFT + slot];
            sDbl[stackTop] = sDbl[VAR_SHFT + slot];
            stack[VAR_SHFT + slot] = DBL_MRK;
            sDbl[VAR_SHFT + slot] = stack_double(stack, sDbl, stackTop) + 1.0;
        } else {
            Object val = activationGet(fnOrScript, scope, slot);
            stack[stackTop] = val;
            val = doubleWrap(ScriptRuntime.toNumber(val) + 1.0);
            activationPut(fnOrScript, scope, slot, val);
        }
        break;
    }
    case TokenStream.VARDEC : {
        int slot = (iCode[++pc] & 0xFF);
        ++stackTop;
        if (!useActivationVars) {
            stack[stackTop] = stack[VAR_SHFT + slot];
            sDbl[stackTop] = sDbl[VAR_SHFT + slot];
            stack[VAR_SHFT + slot] = DBL_MRK;
            sDbl[VAR_SHFT + slot] = stack_double(stack, sDbl, stackTop) - 1.0;
        } else {
            Object val = activationGet(fnOrScript, scope, slot);
            stack[stackTop] = val;
            val = doubleWrap(ScriptRuntime.toNumber(val) - 1.0);
            activationPut(fnOrScript, scope, slot, val);
        }
        break;
    }
    case TokenStream.ZERO :
        ++stackTop;
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = 0;
        break;
    case TokenStream.ONE :
        ++stackTop;
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = 1;
        break;
    case TokenStream.NULL :
        stack[++stackTop] = null;
        break;
    case TokenStream.THIS :
        stack[++stackTop] = thisObj;
        break;
    case TokenStream.THISFN :
        stack[++stackTop] = fnOrScript;
        break;
    case TokenStream.FALSE :
        stack[++stackTop] = Boolean.FALSE;
        break;
    case TokenStream.TRUE :
        stack[++stackTop] = Boolean.TRUE;
        break;
    case TokenStream.UNDEFINED :
        stack[++stackTop] = Undefined.instance;
        break;
    case TokenStream.THROW : {
        Object exception = stack[stackTop];
        if (exception == DBL_MRK) exception = doubleWrap(sDbl[stackTop]);
        --stackTop;
        throw new JavaScriptException(exception);
    }
    case TokenStream.JTHROW : {
        Object exception = stack[stackTop];
        // No need to check for DBL_MRK: exception must be Exception
        --stackTop;
        if (exception instanceof JavaScriptException)
            throw (JavaScriptException)exception;
        else
            throw (RuntimeException)exception;
    }
    case TokenStream.ENTERWITH : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        scope = ScriptRuntime.enterWith(lhs, scope);
        break;
    }
    case TokenStream.LEAVEWITH :
        scope = ScriptRuntime.leaveWith(scope);
        break;
    case TokenStream.NEWSCOPE :
        stack[++stackTop] = ScriptRuntime.newScope();
        break;
    case TokenStream.ENUMINIT : {
        int slot = (iCode[++pc] & 0xFF);
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        stack[LOCAL_SHFT + slot] = ScriptRuntime.initEnum(lhs, scope);
        break;
    }
    case TokenStream.ENUMNEXT : {
        int slot = (iCode[++pc] & 0xFF);
        Object val = stack[LOCAL_SHFT + slot];
        ++stackTop;
        stack[stackTop] = ScriptRuntime.nextEnum(val);
        break;
    }
    case TokenStream.GETPROTO : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.getProto(lhs, scope);
        break;
    }
    case TokenStream.GETPARENT : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.getParent(lhs);
        break;
    }
    case TokenStream.GETSCOPEPARENT : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.getParent(lhs, scope);
        break;
    }
    case TokenStream.SETPROTO : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.setProto(lhs, rhs, scope);
        break;
    }
    case TokenStream.SETPARENT : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.setParent(lhs, rhs, scope);
        break;
    }
    case TokenStream.SCOPE :
        stack[++stackTop] = scope;
        break;
    case TokenStream.CLOSURE : {
        int i = getIndex(iCode, pc + 1);
        InterpreterData closureData = idata.itsNestedFunctions[i];
        InterpretedFunction closure = createFunction(cx, scope, closureData,
                                                     idata.itsFromEvalCode);
        closure.itsClosure = scope;
        stack[++stackTop] = closure;
        pc += 2;
        break;
    }
    case TokenStream.REGEXP : {
        int i = getIndex(iCode, pc + 1);
        stack[++stackTop] = idata.itsRegExpLiterals[i];
        pc += 2;
        break;
    }
    case SOURCEFILE_ICODE :
        cx.interpreterSourceFile = idata.itsSourceFile;
        break;
    case LINE_ICODE : {
        int line = getShort(iCode, pc + 1);
        cx.interpreterLine = line;
        if (debuggerFrame != null) {
            debuggerFrame.onLineChange(cx, line);
        }
        pc += 2;
        break;
    }
    default : {
        dumpICode(idata);
        throw new RuntimeException
            ("Unknown icode : "+(iCode[pc] & 0xff)+" @ pc : "+pc);
    }
    // end of interpreter switch
                }
                pc++;
            }
            catch (Throwable ex) {
                if (instructionThreshold != 0) {
                    if (instructionCount < 0) {
                        // throw during function call
                        instructionCount = cx.instructionCount;
                    } else {
                        // throw during any other operation
                        instructionCount += pc - pcPrevBranch;
                        cx.instructionCount = instructionCount;
                    }
                }

                final int SCRIPT_THROW = 0, ECMA = 1, RUNTIME = 2, OTHER = 3;
                int exType;
                Object catchObj = ex; // Object seen by script catch

                for (;;) {
                    if (catchObj instanceof JavaScriptException) {
                        catchObj = ScriptRuntime.unwrapJavaScriptException
                                    ((JavaScriptException)catchObj);
                        exType = SCRIPT_THROW;
                    } else if (catchObj instanceof EcmaError) {
                        // an offical ECMA error object,
                        catchObj = ((EcmaError)catchObj).getErrorObject();
                        exType = ECMA;
                    } else if (catchObj instanceof RuntimeException) {
                        if (catchObj instanceof WrappedException) {
                            Object w = ((WrappedException) catchObj).unwrap();
                            if (w instanceof Throwable) {
                                catchObj = ex = (Throwable) w;
                                continue;
                            }
                        }
                        catchObj = null; // script can not catch this
                        exType = RUNTIME;
                    } else {
                        // Error instance
                        catchObj = null; // script can not catch this
                        exType = OTHER;
                    }
                    break;
                }

                if (exType != OTHER && debuggerFrame != null) {
                    debuggerFrame.onExceptionThrown(cx, ex);
                }

                boolean rethrow = true;
                if (exType != OTHER && tryStackTop > 0) {
                    // Do not allow for JS to interfere with Error instances
                    // (exType == OTHER), as they can be used to terminate
                    // long running script
                    --tryStackTop;
                    int try_pc = (int)sDbl[TRY_STACK_SHFT + tryStackTop];
                    if (exType == SCRIPT_THROW || exType == ECMA) {
                        // Allow JS to catch only JavaScriptException and
                        // EcmaError
                        int catch_offset = getShort(iCode, try_pc + 1);
                        if (catch_offset != 0) {
                            // Has catch block
                            rethrow = false;
                            pc = try_pc + catch_offset;
                            stackTop = STACK_SHFT;
                            stack[stackTop] = catchObj;
                        }
                    }
                    if (rethrow) {
                        int finally_offset = getShort(iCode, try_pc + 3);
                        if (finally_offset != 0) {
                            // has finally block
                            rethrow = false;
                            pc = try_pc + finally_offset;
                            stackTop = STACK_SHFT;
                            stack[stackTop] = ex;
                        }
                    }
                }

                if (rethrow) {
                    if (debuggerFrame != null) {
                        debuggerFrame.onExit(cx, true, ex);
                    }
                    if (idata.itsNeedsActivation) {
                        ScriptRuntime.popActivation(cx);
                    }

                    if (exType == SCRIPT_THROW)
                        throw (JavaScriptException)ex;
                    if (exType == ECMA || exType == RUNTIME)
                        throw (RuntimeException)ex;
                    throw (Error)ex;
                }

                // We caught an exception,

                // Notify instruction observer if necessary
                // and point pcPrevBranch to start of catch/finally block
                if (instructionThreshold != 0) {
                    if (instructionCount > instructionThreshold) {
                        // Note: this can throw Error
                        cx.observeInstructionCount(instructionCount);
                        instructionCount = 0;
                    }
                }
                pcPrevBranch = pc;

                // restore scope at try point
                scope = (Scriptable)stack[TRY_STACK_SHFT + tryStackTop];
            }
        }
        if (debuggerFrame != null) {
            debuggerFrame.onExit(cx, false, result);
        }
        if (idata.itsNeedsActivation) {
            ScriptRuntime.popActivation(cx);
        }

        if (instructionThreshold != 0) {
            if (instructionCount > instructionThreshold) {
                cx.observeInstructionCount(instructionCount);
                instructionCount = 0;
            }
            cx.instructionCount = instructionCount;
        }
        return result;
    }

    private static Object doubleWrap(double x) {
        return new Double(x);
    }

    private static int stack_int32(Object[] stack, double[] stackDbl, int i) {
        Object x = stack[i];
        return (x != DBL_MRK)
            ? ScriptRuntime.toInt32(x)
            : ScriptRuntime.toInt32(stackDbl[i]);
    }

    private static double stack_double(Object[] stack, double[] stackDbl,
                                       int i)
    {
        Object x = stack[i];
        return (x != DBL_MRK) ? ScriptRuntime.toNumber(x) : stackDbl[i];
    }

    private static void do_add(Object[] stack, double[] stackDbl, int stackTop)
    {
        Object rhs = stack[stackTop + 1];
        Object lhs = stack[stackTop];
        if (rhs == DBL_MRK) {
            double rDbl = stackDbl[stackTop + 1];
            if (lhs == DBL_MRK) {
                stackDbl[stackTop] += rDbl;
            } else {
                do_add(lhs, rDbl, stack, stackDbl, stackTop, true);
            }
        } else if (lhs == DBL_MRK) {
            do_add(rhs, stackDbl[stackTop], stack, stackDbl, stackTop, false);
        } else {
            if (lhs instanceof Scriptable)
                lhs = ((Scriptable) lhs).getDefaultValue(null);
            if (rhs instanceof Scriptable)
                rhs = ((Scriptable) rhs).getDefaultValue(null);
            if (lhs instanceof String || rhs instanceof String) {
                stack[stackTop] = ScriptRuntime.toString(lhs)
                                   + ScriptRuntime.toString(rhs);
            } else {
                double lDbl = (lhs instanceof Number)
                    ? ((Number)lhs).doubleValue() : ScriptRuntime.toNumber(lhs);
                double rDbl = (rhs instanceof Number)
                    ? ((Number)rhs).doubleValue() : ScriptRuntime.toNumber(rhs);
                stack[stackTop] = DBL_MRK;
                stackDbl[stackTop] = lDbl + rDbl;
            }
        }
    }

    // x + y when x is Number, see
    private static void do_add
        (Object lhs, double rDbl,
         Object[] stack, double[] stackDbl, int stackTop,
         boolean left_right_order)
    {
        if (lhs instanceof Scriptable) {
            if (lhs == Undefined.instance) {
                lhs = ScriptRuntime.NaNobj;
            } else {
                lhs = ((Scriptable)lhs).getDefaultValue(null);
            }
        }
        if (lhs instanceof String) {
            if (left_right_order) {
                stack[stackTop] = (String)lhs + ScriptRuntime.toString(rDbl);
            } else {
                stack[stackTop] = ScriptRuntime.toString(rDbl) + (String)lhs;
            }
        } else {
            double lDbl = (lhs instanceof Number)
                ? ((Number)lhs).doubleValue() : ScriptRuntime.toNumber(lhs);
            stack[stackTop] = DBL_MRK;
            stackDbl[stackTop] = lDbl + rDbl;
        }
    }

    private static boolean do_eq(Object[] stack, double[] stackDbl,
                                 int stackTop)
    {
        boolean result;
        Object rhs = stack[stackTop + 1];
        Object lhs = stack[stackTop];
        if (rhs == DBL_MRK) {
            if (lhs == DBL_MRK) {
                result = (stackDbl[stackTop] == stackDbl[stackTop + 1]);
            } else {
                result = do_eq(stackDbl[stackTop + 1], lhs);
            }
        } else {
            if (lhs == DBL_MRK) {
                result = do_eq(stackDbl[stackTop], rhs);
            } else {
                result = ScriptRuntime.eq(lhs, rhs);
            }
        }
        return result;
    }

// Optimized version of ScriptRuntime.eq if x is a Number
    private static boolean do_eq(double x, Object y) {
        for (;;) {
            if (y instanceof Number) {
                return x == ((Number) y).doubleValue();
            }
            if (y instanceof String) {
                return x == ScriptRuntime.toNumber((String)y);
            }
            if (y instanceof Boolean) {
                return x == (((Boolean)y).booleanValue() ? 1 : 0);
            }
            if (y instanceof Scriptable) {
                if (y == Undefined.instance) { return false; }
                y = ScriptRuntime.toPrimitive(y);
                continue;
            }
            return false;
        }
    }

    private static boolean do_sheq(Object[] stack, double[] stackDbl,
                                   int stackTop)
    {
        boolean result;
        Object rhs = stack[stackTop + 1];
        Object lhs = stack[stackTop];
        if (rhs == DBL_MRK) {
            double rDbl = stackDbl[stackTop + 1];
            if (lhs == DBL_MRK) {
                result = (stackDbl[stackTop] == rDbl);
            } else {
                result = (lhs instanceof Number);
                if (result) {
                    result = (((Number)lhs).doubleValue() == rDbl);
                }
            }
        } else if (rhs instanceof Number) {
            double rDbl = ((Number)rhs).doubleValue();
            if (lhs == DBL_MRK) {
                result = (stackDbl[stackTop] == rDbl);
            } else {
                result = (lhs instanceof Number);
                if (result) {
                    result = (((Number)lhs).doubleValue() == rDbl);
                }
            }
        } else {
            result = ScriptRuntime.shallowEq(lhs, rhs);
        }
        return result;
    }

    private static void do_getElem(Context cx,
                                   Object[] stack, double[] stackDbl,
                                   int stackTop, Scriptable scope)
    {
        Object lhs = stack[stackTop - 1];
        if (lhs == DBL_MRK) lhs = doubleWrap(stackDbl[stackTop - 1]);

        Object result;
        Object id = stack[stackTop];
        if (id != DBL_MRK) {
            result = ScriptRuntime.getElem(lhs, id, scope);
        } else {
            Scriptable obj = (lhs instanceof Scriptable)
                             ? (Scriptable)lhs
                             : ScriptRuntime.toObject(cx, scope, lhs);
            double val = stackDbl[stackTop];
            int index = (int)val;
            if (index == val) {
                result = ScriptRuntime.getElem(obj, index);
            } else {
                String s = ScriptRuntime.toString(val);
                result = ScriptRuntime.getStrIdElem(obj, s);
            }
        }
        stack[stackTop - 1] = result;
    }

    private static void do_setElem(Context cx,
                                   Object[] stack, double[] stackDbl,
                                   int stackTop, Scriptable scope)
    {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(stackDbl[stackTop]);
        Object lhs = stack[stackTop - 2];
        if (lhs == DBL_MRK) lhs = doubleWrap(stackDbl[stackTop - 2]);

        Object result;
        Object id = stack[stackTop - 1];
        if (id != DBL_MRK) {
            result = ScriptRuntime.setElem(lhs, id, rhs, scope);
        } else {
            Scriptable obj = (lhs instanceof Scriptable)
                             ? (Scriptable)lhs
                             : ScriptRuntime.toObject(cx, scope, lhs);
            double val = stackDbl[stackTop - 1];
            int index = (int)val;
            if (index == val) {
                result = ScriptRuntime.setElem(obj, index, rhs);
            } else {
                String s = ScriptRuntime.toString(val);
                result = ScriptRuntime.setStrIdElem(obj, s, rhs, scope);
            }
        }
        stack[stackTop - 2] = result;
    }

    private static Object[] getArgsArray(Object[] stack, double[] sDbl,
                                         int shift, int count)
    {
        if (count == 0) {
            return ScriptRuntime.emptyArgs;
        }
        Object[] args = new Object[count];
        for (int i = 0; i != count; ++i, ++shift) {
            Object val = stack[shift];
            if (val == DBL_MRK) val = doubleWrap(sDbl[shift]);
            args[i] = val;
        }
        return args;
    }

    private static Object activationGet(NativeFunction f,
                                        Scriptable activation, int slot)
    {
        String name = f.argNames[slot];
        Object val = activation.get(name, activation);
    // Activation parameter or var is permanent
        if (val == Scriptable.NOT_FOUND) Context.codeBug();
        return val;
    }

    private static void activationPut(NativeFunction f,
                                      Scriptable activation, int slot,
                                      Object value)
    {
        String name = f.argNames[slot];
        activation.put(name, activation, value);
    }

    private static Object execWithNewDomain(Context cx, Scriptable scope,
                                            final Scriptable thisObj,
                                            final Object[] args,
                                            final double[] argsDbl,
                                            final int argShift,
                                            final int argCount,
                                            final NativeFunction fnOrScript,
                                            final InterpreterData idata)
        throws JavaScriptException
    {
        if (cx.interpreterSecurityDomain == idata.securityDomain)
            Context.codeBug();

        Script code = new Script() {
            public Object exec(Context cx, Scriptable scope)
                throws JavaScriptException
            {
                return interpret(cx, scope, thisObj,
                                 args, argsDbl, argShift, argCount,
                                 fnOrScript, idata);
            }
        };

        Object savedDomain = cx.interpreterSecurityDomain;
        cx.interpreterSecurityDomain = idata.securityDomain;
        try {
            return cx.getSecurityController().
                    execWithDomain(cx, scope, code, idata.securityDomain);
        }finally {
            cx.interpreterSecurityDomain = savedDomain;
        }
    }


    private boolean itsInFunctionFlag;

    private InterpreterData itsData;
    private VariableTable itsVariableTable;
    private int itsTryDepth = 0;
    private int itsStackDepth = 0;
    private String itsSourceFile;
    private int itsLineNumber = 0;
    private LabelTable itsLabels = new LabelTable();
    private int itsDoubleTableTop;
    private ObjToIntMap itsStrings = new ObjToIntMap(20);
    private String lastAddString;


    private int version;
    private boolean inLineStepMode;
    private String debugSource;

    private static final Object DBL_MRK = new Object();
}
