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

public class Interpreter
{

// Additional interpreter-specific codes
    private static final int BASE_ICODE = Token.LAST_BYTECODE_TOKEN;

    private static final int
        Icode_DUP                       = BASE_ICODE + 1,

    // various types of ++/--
        Icode_NAMEINC                   = BASE_ICODE + 2,
        Icode_PROPINC                   = BASE_ICODE + 3,
        Icode_ELEMINC                   = BASE_ICODE + 4,
        Icode_VARINC                    = BASE_ICODE + 5,
        Icode_NAMEDEC                   = BASE_ICODE + 6,
        Icode_PROPDEC                   = BASE_ICODE + 7,
        Icode_ELEMDEC                   = BASE_ICODE + 8,
        Icode_VARDEC                    = BASE_ICODE + 9,

    // helper codes to deal with activation
        Icode_SCOPE                     = BASE_ICODE + 10,
        Icode_TYPEOFNAME                = BASE_ICODE + 11,

    // Access to parent scope and prototype
        Icode_GETPROTO                  = BASE_ICODE + 12,
        Icode_GETPARENT                 = BASE_ICODE + 13,
        Icode_GETSCOPEPARENT            = BASE_ICODE + 14,
        Icode_SETPROTO                  = BASE_ICODE + 15,
        Icode_SETPARENT                 = BASE_ICODE + 16,

    // Create closure object for nested functions
        Icode_CLOSURE                   = BASE_ICODE + 17,

    // Special calls
        Icode_CALLSPECIAL               = BASE_ICODE + 18,

    // To return undefined value
        Icode_RETUNDEF                  = BASE_ICODE + 19,

    // Exception handling implementation
        Icode_CATCH                     = BASE_ICODE + 20,
        Icode_GOSUB                     = BASE_ICODE + 21,
        Icode_RETSUB                    = BASE_ICODE + 22,

    // To indicating a line number change in icodes.
        Icode_LINE                      = BASE_ICODE + 23,

    // To store shorts and ints inline
        Icode_SHORTNUMBER               = BASE_ICODE + 24,
        Icode_INTNUMBER                 = BASE_ICODE + 25,

    // Last icode
        Icode_END                       = BASE_ICODE + 26;


    public IRFactory createIRFactory(Context cx, TokenStream ts)
    {
        return new IRFactory(this, ts);
    }

    public FunctionNode createFunctionNode(IRFactory irFactory, String name)
    {
        return new FunctionNode(name);
    }

    public ScriptOrFnNode transform(Context cx, IRFactory irFactory,
                                    ScriptOrFnNode tree)
    {
        (new NodeTransformer(irFactory)).transform(tree);
        return tree;
    }

    public Object compile(Context cx, Scriptable scope, ScriptOrFnNode tree,
                          SecurityController securityController,
                          Object securityDomain, String encodedSource)
    {
        scriptOrFn = tree;
        itsData = new InterpreterData(securityDomain, cx.getLanguageVersion());
        itsData.itsSourceFile = scriptOrFn.getSourceName();
        itsData.encodedSource = encodedSource;
        itsData.topLevel = true;
        if (tree instanceof FunctionNode) {
            generateFunctionICode(cx);
            return createFunction(cx, scope, itsData, false);
        } else {
            generateICodeFromTree(cx, scriptOrFn);
            return new InterpretedScript(itsData);
        }
    }

    public void notifyDebuggerCompilationDone(Context cx,
                                              Object scriptOrFunction,
                                              String debugSource)
    {
        InterpreterData idata;
        if (scriptOrFunction instanceof InterpretedScript) {
            idata = ((InterpretedScript)scriptOrFunction).itsData;
        } else {
            idata = ((InterpretedFunction)scriptOrFunction).itsData;
        }
        notifyDebugger_r(cx, idata, debugSource);
    }

    private static void notifyDebugger_r(Context cx, InterpreterData idata,
                                         String debugSource)
    {
        cx.debugger.handleCompilationDone(cx, idata, debugSource);
        if (idata.itsNestedFunctions != null) {
            for (int i = 0; i != idata.itsNestedFunctions.length; ++i) {
                notifyDebugger_r(cx, idata.itsNestedFunctions[i], debugSource);
            }
        }
    }

    private void generateFunctionICode(Context cx)
    {
        FunctionNode theFunction = (FunctionNode)scriptOrFn;

        itsData.itsFunctionType = theFunction.getFunctionType();
        itsData.itsNeedsActivation = theFunction.requiresActivation();
        itsData.itsName = theFunction.getFunctionName();
        if (!theFunction.getIgnoreDynamicScope()) {
            if (cx.hasCompileFunctionsWithDynamicScope()) {
                itsData.useDynamicScope = true;
            }
        }

        generateICodeFromTree(cx, theFunction.getLastChild());
    }

    private void generateICodeFromTree(Context cx, Node tree)
    {
        generateNestedFunctions(cx);

        generateRegExpLiterals(cx);

        int theICodeTop = 0;
        theICodeTop = generateICode(tree, theICodeTop);
        itsLabels.fixLabelGotos(itsData.itsICode);
        // add Icode_END only to scripts as function always ends with RETURN
        if (itsData.itsFunctionType == 0) {
            theICodeTop = addIcode(Icode_END, theICodeTop);
        }
        // Add special CATCH to simplify Interpreter.interpret logic
        // and workaround lack of goto in Java
        theICodeTop = addIcode(Icode_CATCH, theICodeTop);

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
                if (itsData.itsStringTable[index] != null) Kit.codeBug();
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
        if (itsExceptionTableTop != 0
            && itsData.itsExceptionTable.length != itsExceptionTableTop)
        {
            int[] tmp = new int[itsExceptionTableTop];
            System.arraycopy(itsData.itsExceptionTable, 0, tmp, 0,
                             itsExceptionTableTop);
            itsData.itsExceptionTable = tmp;
        }

        itsData.itsMaxVars = scriptOrFn.getParamAndVarCount();
        // itsMaxFrameArray: interpret method needs this amount for its
        // stack and sDbl arrays
        itsData.itsMaxFrameArray = itsData.itsMaxVars
                                   + itsData.itsMaxLocals
                                   + itsData.itsMaxStack;

        itsData.argNames = scriptOrFn.getParamAndVarNames();
        itsData.argCount = scriptOrFn.getParamCount();

        itsData.encodedSourceStart = scriptOrFn.getEncodedSourceStart();
        itsData.encodedSourceEnd = scriptOrFn.getEncodedSourceEnd();

        if (Token.printICode) dumpICode(itsData);
    }

    private void generateNestedFunctions(Context cx)
    {
        int functionCount = scriptOrFn.getFunctionCount();
        if (functionCount == 0) return;

        InterpreterData[] array = new InterpreterData[functionCount];
        for (int i = 0; i != functionCount; i++) {
            FunctionNode def = scriptOrFn.getFunctionNode(i);
            Interpreter jsi = new Interpreter();
            jsi.scriptOrFn = def;
            jsi.itsData = new InterpreterData(itsData.securityDomain,
                                              itsData.languageVersion);
            jsi.itsData.parentData = itsData;
            jsi.itsData.itsSourceFile = itsData.itsSourceFile;
            jsi.itsData.encodedSource = itsData.encodedSource;
            jsi.itsData.itsCheckThis = def.getCheckThis();
            jsi.itsInFunctionFlag = true;
            jsi.generateFunctionICode(cx);
            array[i] = jsi.itsData;
        }
        itsData.itsNestedFunctions = array;
    }

    private void generateRegExpLiterals(Context cx)
    {
        int N = scriptOrFn.getRegexpCount();
        if (N == 0) return;

        RegExpProxy rep = ScriptRuntime.checkRegExpProxy(cx);
        Object[] array = new Object[N];
        for (int i = 0; i != N; i++) {
            String string = scriptOrFn.getRegexpString(i);
            String flags = scriptOrFn.getRegexpFlags(i);
            array[i] = rep.compileRegExp(cx, string, flags);
        }
        itsData.itsRegExpLiterals = array;
    }

    private int updateLineNumber(Node node, int iCodeTop)
    {
        int lineno = node.getLineno();
        if (lineno != itsLineNumber && lineno >= 0) {
            itsLineNumber = lineno;
            iCodeTop = addIcode(Icode_LINE, iCodeTop);
            iCodeTop = addShort(lineno, iCodeTop);
        }
        return iCodeTop;
    }

    private void badTree(Node node)
    {
        throw new RuntimeException("Un-handled node: "+node.toString());
    }

    private int generateICode(Node node, int iCodeTop)
    {
        int type = node.getType();
        Node child = node.getFirstChild();
        Node firstChild = child;
        switch (type) {

            case Token.FUNCTION : {
                int fnIndex = node.getExistingIntProp(Node.FUNCTION_PROP);
                FunctionNode fn = scriptOrFn.getFunctionNode(fnIndex);
                if (fn.getFunctionType() != FunctionNode.FUNCTION_STATEMENT) {
                    // Only function expressions or function expression
                    // statements needs closure code creating new function
                    // object on stack as function statements are initialized
                    // at script/function start
                    iCodeTop = addIcode(Icode_CLOSURE, iCodeTop);
                    iCodeTop = addIndex(fnIndex, iCodeTop);
                    itsStackDepth++;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                }
                break;
            }

            case Token.SCRIPT :
                iCodeTop = updateLineNumber(node, iCodeTop);
                while (child != null) {
                    if (child.getType() != Token.FUNCTION)
                        iCodeTop = generateICode(child, iCodeTop);
                    child = child.getNext();
                }
                break;

            case Token.CASE :
                iCodeTop = updateLineNumber(node, iCodeTop);
                child = child.getNext();
                while (child != null) {
                    iCodeTop = generateICode(child, iCodeTop);
                    child = child.getNext();
                }
                break;

            case Token.LABEL :
            case Token.LOOP :
            case Token.DEFAULT :
            case Token.BLOCK :
            case Token.EMPTY :
            case Token.NOP :
                iCodeTop = updateLineNumber(node, iCodeTop);
                while (child != null) {
                    iCodeTop = generateICode(child, iCodeTop);
                    child = child.getNext();
                }
                break;

            case Token.WITH :
                ++itsWithDepth;
                iCodeTop = updateLineNumber(node, iCodeTop);
                while (child != null) {
                    iCodeTop = generateICode(child, iCodeTop);
                    child = child.getNext();
                }
                --itsWithDepth;
                break;

            case Token.COMMA :
                iCodeTop = generateICode(child, iCodeTop);
                while (null != (child = child.getNext())) {
                    iCodeTop = addToken(Token.POP, iCodeTop);
                    itsStackDepth--;
                    iCodeTop = generateICode(child, iCodeTop);
                }
                break;

            case Token.SWITCH : {
                Node.Jump switchNode = (Node.Jump)node;
                iCodeTop = updateLineNumber(switchNode, iCodeTop);
                iCodeTop = generateICode(child, iCodeTop);
                int theLocalSlot = allocateLocal();
                iCodeTop = addToken(Token.NEWTEMP, iCodeTop);
                iCodeTop = addByte(theLocalSlot, iCodeTop);
                iCodeTop = addToken(Token.POP, iCodeTop);
                itsStackDepth--;

                ObjArray cases = (ObjArray) switchNode.getProp(Node.CASES_PROP);
                for (int i = 0; i < cases.size(); i++) {
                    Node thisCase = (Node)cases.get(i);
                    Node first = thisCase.getFirstChild();
                    // the case expression is the firstmost child
                    // the rest will be generated when the case
                    // statements are encountered as siblings of
                    // the switch statement.
                    iCodeTop = generateICode(first, iCodeTop);
                    iCodeTop = addToken(Token.USETEMP, iCodeTop);
                    iCodeTop = addByte(theLocalSlot, iCodeTop);
                    itsStackDepth++;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                    iCodeTop = addToken(Token.SHEQ, iCodeTop);
                    itsStackDepth--;
                    Node.Target target = new Node.Target();
                    thisCase.addChildAfter(target, first);
                    iCodeTop = addGoto(target, Token.IFEQ, iCodeTop);
                }

                Node defaultNode = (Node) switchNode.getProp(Node.DEFAULT_PROP);
                if (defaultNode != null) {
                    Node.Target defaultTarget = new Node.Target();
                    defaultNode.getFirstChild().
                        addChildToFront(defaultTarget);
                    iCodeTop = addGoto(defaultTarget, Token.GOTO,
                                       iCodeTop);
                }

                Node.Target breakTarget = switchNode.target;
                iCodeTop = addGoto(breakTarget, Token.GOTO, iCodeTop);
                break;
            }

            case Token.TARGET :
                markTargetLabel((Node.Target)node, iCodeTop);
                break;

            case Token.NEW :
            case Token.CALL : {
                int childCount = 0;
                String functionName = null;
                while (child != null) {
                    iCodeTop = generateICode(child, iCodeTop);
                    if (functionName == null) {
                        int childType = child.getType();
                        if (childType == Token.NAME
                            || childType == Token.GETPROP)
                        {
                            functionName = lastAddString;
                        }
                    }
                    child = child.getNext();
                    childCount++;
                }
                int callType = node.getIntProp(Node.SPECIALCALL_PROP,
                                               Node.NON_SPECIALCALL);
                if (callType != Node.NON_SPECIALCALL) {
                    // embed line number and source filename
                    iCodeTop = addIcode(Icode_CALLSPECIAL, iCodeTop);
                    iCodeTop = addByte(callType, iCodeTop);
                    iCodeTop = addByte(type == Token.NEW ? 1 : 0, iCodeTop);
                    iCodeTop = addShort(itsLineNumber, iCodeTop);
                } else {
                    iCodeTop = addToken(type, iCodeTop);
                    iCodeTop = addString(functionName, iCodeTop);
                }

                itsStackDepth -= (childCount - 1);  // always a result value
                // subtract from child count to account for [thisObj &] fun
                if (type == Token.NEW) {
                    childCount -= 1;
                } else {
                    childCount -= 2;
                }
                iCodeTop = addIndex(childCount, iCodeTop);
                if (childCount > itsData.itsMaxCalleeArgs)
                    itsData.itsMaxCalleeArgs = childCount;
                break;
            }

            case Token.NEWLOCAL :
            case Token.NEWTEMP : {
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addToken(Token.NEWTEMP, iCodeTop);
                iCodeTop = addLocalRef(node, iCodeTop);
                break;
            }

            case Token.USELOCAL : {
                iCodeTop = addToken(Token.USETEMP, iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                Node temp = (Node) node.getProp(Node.LOCAL_PROP);
                iCodeTop = addLocalRef(temp, iCodeTop);
                break;
            }

            case Token.USETEMP : {
                iCodeTop = addToken(Token.USETEMP, iCodeTop);
                Node temp = (Node) node.getProp(Node.TEMP_PROP);
                iCodeTop = addLocalRef(temp, iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;
            }

            case Token.IFEQ :
            case Token.IFNE :
                iCodeTop = generateICode(child, iCodeTop);
                itsStackDepth--;    // after the conditional GOTO, really
                    // fall thru...
            case Token.GOTO : {
                Node.Target target = ((Node.Jump)node).target;
                iCodeTop = addGoto(target, (byte) type, iCodeTop);
                break;
            }

            case Token.JSR : {
                Node.Target target = ((Node.Jump)node).target;
                iCodeTop = addGoto(target, Icode_GOSUB, iCodeTop);
                break;
            }

            case Token.FINALLY : {
                int finallyRegister = allocateLocal();
                iCodeTop = addToken(Token.NEWTEMP, iCodeTop);
                iCodeTop = addByte(finallyRegister, iCodeTop);
                iCodeTop = addToken(Token.POP, iCodeTop);
                itsStackDepth--;
                while (child != null) {
                    iCodeTop = generateICode(child, iCodeTop);
                    child = child.getNext();
                }
                iCodeTop = addIcode(Icode_RETSUB, iCodeTop);
                iCodeTop = addByte(finallyRegister, iCodeTop);
                releaseLocal(finallyRegister);
                break;
            }

            case Token.AND : {
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addIcode(Icode_DUP, iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                int falseJumpStart = iCodeTop;
                iCodeTop = addForwardGoto(Token.IFNE, iCodeTop);
                iCodeTop = addToken(Token.POP, iCodeTop);
                itsStackDepth--;
                child = child.getNext();
                iCodeTop = generateICode(child, iCodeTop);
                resolveForwardGoto(falseJumpStart, iCodeTop);
                break;
            }

            case Token.OR : {
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addIcode(Icode_DUP, iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                int trueJumpStart = iCodeTop;
                iCodeTop = addForwardGoto(Token.IFEQ, iCodeTop);
                iCodeTop = addToken(Token.POP, iCodeTop);
                itsStackDepth--;
                child = child.getNext();
                iCodeTop = generateICode(child, iCodeTop);
                resolveForwardGoto(trueJumpStart, iCodeTop);
                break;
            }

            case Token.GETPROP : {
                iCodeTop = generateICode(child, iCodeTop);
                String s = (String) node.getProp(Node.SPECIAL_PROP_PROP);
                if (s != null) {
                    if (s.equals("__proto__")) {
                        iCodeTop = addIcode(Icode_GETPROTO, iCodeTop);
                    } else if (s.equals("__parent__")) {
                        iCodeTop = addIcode(Icode_GETSCOPEPARENT, iCodeTop);
                    } else {
                        badTree(node);
                    }
                } else {
                    child = child.getNext();
                    iCodeTop = generateICode(child, iCodeTop);
                    iCodeTop = addToken(Token.GETPROP, iCodeTop);
                    itsStackDepth--;
                }
                break;
            }

            case Token.DELPROP :
            case Token.BITAND :
            case Token.BITOR :
            case Token.BITXOR :
            case Token.LSH :
            case Token.RSH :
            case Token.URSH :
            case Token.ADD :
            case Token.SUB :
            case Token.MOD :
            case Token.DIV :
            case Token.MUL :
            case Token.GETELEM :
            case Token.EQ:
            case Token.NE:
            case Token.SHEQ:
            case Token.SHNE:
            case Token.IN :
            case Token.INSTANCEOF :
            case Token.LE :
            case Token.LT :
            case Token.GE :
            case Token.GT :
                iCodeTop = generateICode(child, iCodeTop);
                child = child.getNext();
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addToken(type, iCodeTop);
                itsStackDepth--;
                break;

            case Token.POS :
            case Token.NEG :
            case Token.NOT :
            case Token.BITNOT :
            case Token.TYPEOF :
            case Token.VOID :
                iCodeTop = generateICode(child, iCodeTop);
                if (type == Token.VOID) {
                    iCodeTop = addToken(Token.POP, iCodeTop);
                    iCodeTop = addToken(Token.UNDEFINED, iCodeTop);
                } else {
                    iCodeTop = addToken(type, iCodeTop);
                }
                break;

            case Token.SETPROP : {
                iCodeTop = generateICode(child, iCodeTop);
                child = child.getNext();
                iCodeTop = generateICode(child, iCodeTop);
                String s = (String) node.getProp(Node.SPECIAL_PROP_PROP);
                if (s != null) {
                    if (s.equals("__proto__")) {
                        iCodeTop = addIcode(Icode_SETPROTO, iCodeTop);
                    } else if (s.equals("__parent__")) {
                        iCodeTop = addIcode(Icode_SETPARENT, iCodeTop);
                    } else {
                        badTree(node);
                    }
                } else {
                    child = child.getNext();
                    iCodeTop = generateICode(child, iCodeTop);
                    iCodeTop = addToken(Token.SETPROP, iCodeTop);
                    itsStackDepth -= 2;
                }
                break;
            }

            case Token.SETELEM :
                iCodeTop = generateICode(child, iCodeTop);
                child = child.getNext();
                iCodeTop = generateICode(child, iCodeTop);
                child = child.getNext();
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addToken(Token.SETELEM, iCodeTop);
                itsStackDepth -= 2;
                break;

            case Token.SETNAME :
                iCodeTop = generateICode(child, iCodeTop);
                child = child.getNext();
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addToken(Token.SETNAME, iCodeTop);
                iCodeTop = addString(firstChild.getString(), iCodeTop);
                itsStackDepth--;
                break;

            case Token.TYPEOFNAME : {
                String name = node.getString();
                int index = -1;
                // use typeofname if an activation frame exists
                // since the vars all exist there instead of in jregs
                if (itsInFunctionFlag && !itsData.itsNeedsActivation)
                    index = scriptOrFn.getParamOrVarIndex(name);
                if (index == -1) {
                    iCodeTop = addIcode(Icode_TYPEOFNAME, iCodeTop);
                    iCodeTop = addString(name, iCodeTop);
                } else {
                    iCodeTop = addToken(Token.GETVAR, iCodeTop);
                    iCodeTop = addByte(index, iCodeTop);
                    iCodeTop = addToken(Token.TYPEOF, iCodeTop);
                }
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;
            }

            case Token.PARENT :
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addIcode(Icode_GETPARENT, iCodeTop);
                break;

            case Token.GETBASE :
            case Token.BINDNAME :
            case Token.NAME :
            case Token.STRING :
                iCodeTop = addToken(type, iCodeTop);
                iCodeTop = addString(node.getString(), iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;

            case Token.INC :
            case Token.DEC : {
                int childType = child.getType();
                switch (childType) {
                    case Token.GETVAR : {
                        String name = child.getString();
                        if (itsData.itsNeedsActivation) {
                            iCodeTop = addIcode(Icode_SCOPE, iCodeTop);
                            iCodeTop = addToken(Token.STRING, iCodeTop);
                            iCodeTop = addString(name, iCodeTop);
                            itsStackDepth += 2;
                            if (itsStackDepth > itsData.itsMaxStack)
                                itsData.itsMaxStack = itsStackDepth;
                            iCodeTop = addIcode(type == Token.INC
                                                ? Icode_PROPINC
                                                : Icode_PROPDEC,
                                                iCodeTop);
                            itsStackDepth--;
                        } else {
                            int i = scriptOrFn.getParamOrVarIndex(name);
                            iCodeTop = addIcode(type == Token.INC
                                                ? Icode_VARINC
                                                : Icode_VARDEC,
                                                iCodeTop);
                            iCodeTop = addByte(i, iCodeTop);
                            itsStackDepth++;
                            if (itsStackDepth > itsData.itsMaxStack)
                                itsData.itsMaxStack = itsStackDepth;
                        }
                        break;
                    }
                    case Token.GETPROP :
                    case Token.GETELEM : {
                        Node getPropChild = child.getFirstChild();
                        iCodeTop = generateICode(getPropChild, iCodeTop);
                        getPropChild = getPropChild.getNext();
                        iCodeTop = generateICode(getPropChild, iCodeTop);
                        int icode;
                        if (childType == Token.GETPROP) {
                            icode = (type == Token.INC)
                                    ? Icode_PROPINC : Icode_PROPDEC;
                        } else {
                            icode = (type == Token.INC)
                                    ? Icode_ELEMINC : Icode_ELEMDEC;
                        }
                        iCodeTop = addIcode(icode, iCodeTop);
                        itsStackDepth--;
                        break;
                    }
                    default : {
                        iCodeTop = addIcode(type == Token.INC
                                            ? Icode_NAMEINC
                                            : Icode_NAMEDEC,
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

            case Token.NUMBER : {
                double num = node.getDouble();
                int inum = (int)num;
                if (inum == num) {
                    if (inum == 0) {
                        iCodeTop = addToken(Token.ZERO, iCodeTop);
                    } else if (inum == 1) {
                        iCodeTop = addToken(Token.ONE, iCodeTop);
                    } else if ((short)inum == inum) {
                        iCodeTop = addIcode(Icode_SHORTNUMBER, iCodeTop);
                        iCodeTop = addShort(inum, iCodeTop);
                    } else {
                        iCodeTop = addIcode(Icode_INTNUMBER, iCodeTop);
                        iCodeTop = addInt(inum, iCodeTop);
                    }
                } else {
                    iCodeTop = addToken(Token.NUMBER, iCodeTop);
                    iCodeTop = addDouble(num, iCodeTop);
                }
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;
            }

            case Token.POP :
            case Token.POPV :
                iCodeTop = updateLineNumber(node, iCodeTop);
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addToken(type, iCodeTop);
                itsStackDepth--;
                break;

            case Token.ENTERWITH :
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addToken(Token.ENTERWITH, iCodeTop);
                itsStackDepth--;
                break;

            case Token.GETTHIS :
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addToken(Token.GETTHIS, iCodeTop);
                break;

            case Token.NEWSCOPE :
                iCodeTop = addToken(Token.NEWSCOPE, iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;

            case Token.LEAVEWITH :
                iCodeTop = addToken(Token.LEAVEWITH, iCodeTop);
                break;

            case Token.TRY : {
                Node.Jump tryNode = (Node.Jump)node;
                Node catchTarget = tryNode.target;
                Node finallyTarget = tryNode.getFinally();

                int tryStart = iCodeTop;
                int tryEnd = -1;
                int catchStart = -1;
                int finallyStart = -1;

                while (child != null) {
                    boolean generated = false;

                    if (child == catchTarget) {
                        if (tryEnd >= 0) Kit.codeBug();
                        tryEnd = iCodeTop;
                        catchStart = iCodeTop;

                        markTargetLabel((Node.Target)child, iCodeTop);
                        generated = true;

                        // Catch code has exception object on the stack
                        itsStackDepth = 1;
                        if (itsStackDepth > itsData.itsMaxStack)
                            itsData.itsMaxStack = itsStackDepth;

                    } else if (child == finallyTarget) {
                        if (tryEnd < 0) {
                            tryEnd = iCodeTop;
                        }
                        finallyStart = iCodeTop;

                        markTargetLabel((Node.Target)child, iCodeTop);
                        generated = true;

                        // Adjust stack for finally code: on the top of the
                        // stack it has either a PC value  when called from
                        // GOSUB or exception object to rethrow when called
                        // from exception handler
                        itsStackDepth = 1;
                        if (itsStackDepth > itsData.itsMaxStack) {
                            itsData.itsMaxStack = itsStackDepth;
                        }
                    }

                    if (!generated) {
                        iCodeTop = generateICode(child, iCodeTop);
                    }
                    child = child.getNext();
                }
                itsStackDepth = 0;
                // [tryStart, tryEnd) contains GOSUB to call finally when it
                // presents at the end of try code and before return, break
                // continue that transfer control outside try.
                // After finally is executed the control will be
                // transfered back into [tryStart, tryEnd) and exception
                // handling assumes that the only code executed after
                // finally returns will be a jump outside try which could not
                // trigger exceptions.
                // It does not hold if instruction observer throws during
                // goto. Currently it may lead to double execution of finally.
                addExceptionHandler(tryStart, tryEnd, catchStart, finallyStart,
                                    itsWithDepth);
                break;
            }

            case Token.THROW :
                iCodeTop = updateLineNumber(node, iCodeTop);
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addToken(Token.THROW, iCodeTop);
                iCodeTop = addShort(itsLineNumber, iCodeTop);
                itsStackDepth--;
                break;

            case Token.RETURN :
                iCodeTop = updateLineNumber(node, iCodeTop);
                if (child != null) {
                    iCodeTop = generateICode(child, iCodeTop);
                    iCodeTop = addToken(Token.RETURN, iCodeTop);
                    itsStackDepth--;
                } else {
                    iCodeTop = addIcode(Icode_RETUNDEF, iCodeTop);
                }
                break;

            case Token.GETVAR : {
                String name = node.getString();
                if (itsData.itsNeedsActivation) {
                    // SETVAR handled this by turning into a SETPROP, but
                    // we can't do that to a GETVAR without manufacturing
                    // bogus children. Instead we use a special op to
                    // push the current scope.
                    iCodeTop = addIcode(Icode_SCOPE, iCodeTop);
                    iCodeTop = addToken(Token.STRING, iCodeTop);
                    iCodeTop = addString(name, iCodeTop);
                    itsStackDepth += 2;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                    iCodeTop = addToken(Token.GETPROP, iCodeTop);
                    itsStackDepth--;
                } else {
                    int index = scriptOrFn.getParamOrVarIndex(name);
                    iCodeTop = addToken(Token.GETVAR, iCodeTop);
                    iCodeTop = addByte(index, iCodeTop);
                    itsStackDepth++;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                }
                break;
            }

            case Token.SETVAR : {
                if (itsData.itsNeedsActivation) {
                    child.setType(Token.BINDNAME);
                    node.setType(Token.SETNAME);
                    iCodeTop = generateICode(node, iCodeTop);
                } else {
                    String name = child.getString();
                    child = child.getNext();
                    iCodeTop = generateICode(child, iCodeTop);
                    int index = scriptOrFn.getParamOrVarIndex(name);
                    iCodeTop = addToken(Token.SETVAR, iCodeTop);
                    iCodeTop = addByte(index, iCodeTop);
                }
                break;
            }

            case Token.NULL:
            case Token.THIS:
            case Token.THISFN:
            case Token.FALSE:
            case Token.TRUE:
            case Token.UNDEFINED:
                iCodeTop = addToken(type, iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;

            case Token.ENUMINIT :
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addToken(Token.ENUMINIT, iCodeTop);
                iCodeTop = addLocalRef(node, iCodeTop);
                itsStackDepth--;
                break;

            case Token.ENUMNEXT : {
                iCodeTop = addToken(Token.ENUMNEXT, iCodeTop);
                Node init = (Node)node.getProp(Node.ENUM_PROP);
                iCodeTop = addLocalRef(init, iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;
            }

            case Token.ENUMDONE :
                // could release the local here??
                break;

            case Token.REGEXP : {
                int index = node.getExistingIntProp(Node.REGEXP_PROP);
                iCodeTop = addToken(Token.REGEXP, iCodeTop);
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
            theLocalSlot = allocateLocal();
            node.putIntProp(Node.LOCAL_PROP, theLocalSlot);
        }
        iCodeTop = addByte(theLocalSlot, iCodeTop);
        return iCodeTop;
    }

    private int allocateLocal()
    {
         return itsData.itsMaxLocals++;
    }

    private void releaseLocal(int local)
    {
    }

    private int getTargetLabel(Node.Target target)
    {
        int targetLabel = target.labelId;
        if (targetLabel == -1) {
            targetLabel = itsLabels.acquireLabel();
            target.labelId = targetLabel;
        }
        return targetLabel;
    }

    private void markTargetLabel(Node.Target target, int iCodeTop)
    {
        int label = getTargetLabel(target);
        itsLabels.markLabel(label, iCodeTop);
    }

    private int addGoto(Node.Target target, int gotoOp, int iCodeTop)
    {
        int targetLabel = getTargetLabel(target);

        int gotoPC = iCodeTop;
        if (gotoOp > BASE_ICODE) {
            iCodeTop = addIcode(gotoOp, iCodeTop);
        } else {
            iCodeTop = addToken(gotoOp, iCodeTop);
        }
        iCodeTop = addShort(0, iCodeTop);

        int targetPC = itsLabels.getLabelPC(targetLabel);
        if (targetPC != -1) {
            recordJumpOffset(gotoPC + 1, targetPC - gotoPC);
        } else {
            itsLabels.addLabelFixup(targetLabel, gotoPC + 1);
        }
        return iCodeTop;
    }

    private int addForwardGoto(int gotoOp, int iCodeTop)
    {
        iCodeTop = addToken(gotoOp, iCodeTop);
        iCodeTop = addShort(0, iCodeTop);
        return iCodeTop;
    }

    private void resolveForwardGoto(int jumpStart, int iCodeTop)
    {
        if (jumpStart + 3 > iCodeTop) Kit.codeBug();
        int offset = iCodeTop - jumpStart;
        // +1 to write after jump icode
        recordJumpOffset(jumpStart + 1, offset);
    }

    private void recordJumpOffset(int pos, int offset)
    {
        if (offset != (short)offset) {
            throw Context.reportRuntimeError0("msg.too.big.jump");
        }
        itsData.itsICode[pos] = (byte)(offset >> 8);
        itsData.itsICode[pos + 1] = (byte)offset;
    }

    private int addByte(int b, int iCodeTop)
    {
        byte[] array = itsData.itsICode;
        if (iCodeTop == array.length) {
            array = increaseICodeCapasity(iCodeTop, 1);
        }
        array[iCodeTop++] = (byte)b;
        return iCodeTop;
    }

    private int addToken(int token, int iCodeTop)
    {
        if (!(Token.FIRST_BYTECODE_TOKEN <= token
            && token <= Token.LAST_BYTECODE_TOKEN))
        {
            Kit.codeBug();
        }
        return addByte(token, iCodeTop);
    }

    private int addIcode(int icode, int iCodeTop)
    {
        if (!(BASE_ICODE < icode && icode <= Icode_END)) Kit.codeBug();
        return addByte(icode, iCodeTop);
    }

    private int addShort(int s, int iCodeTop)
    {
        byte[] array = itsData.itsICode;
        if (iCodeTop + 2 > array.length) {
            array = increaseICodeCapasity(iCodeTop, 2);
        }
        array[iCodeTop] = (byte)(s >>> 8);
        array[iCodeTop + 1] = (byte)s;
        return iCodeTop + 2;
    }

    private int addIndex(int index, int iCodeTop)
    {
        if (index < 0) Kit.codeBug();
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

    private int addInt(int i, int iCodeTop)
    {
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

    private int addDouble(double num, int iCodeTop)
    {
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

    private int addString(String str, int iCodeTop)
    {
        int index = itsStrings.get(str, -1);
        if (index == -1) {
            index = itsStrings.size();
            itsStrings.put(str, index);
        }
        iCodeTop = addIndex(index, iCodeTop);
        lastAddString = str;
        return iCodeTop;
    }

    private void addExceptionHandler(int icodeStart, int icodeEnd,
                                     int catchStart, int finallyStart,
                                     int withDepth)
    {
        int top = itsExceptionTableTop;
        int[] table = itsData.itsExceptionTable;
        if (table == null) {
            if (top != 0) Kit.codeBug();
            table = new int[EXCEPTION_SLOT_SIZE * 2];
            itsData.itsExceptionTable = table;
        } else if (table.length == top) {
            table = new int[table.length * 2];
            System.arraycopy(itsData.itsExceptionTable, 0, table, 0, top);
            itsData.itsExceptionTable = table;
        }
        table[top + EXCEPTION_TRY_START_SLOT]  = icodeStart;
        table[top + EXCEPTION_TRY_END_SLOT]    = icodeEnd;
        table[top + EXCEPTION_CATCH_SLOT]      = catchStart;
        table[top + EXCEPTION_FINALLY_SLOT]    = finallyStart;
        table[top + EXCEPTION_WITH_DEPTH_SLOT] = withDepth;

        itsExceptionTableTop = top + EXCEPTION_SLOT_SIZE;
    }

    private byte[] increaseICodeCapasity(int iCodeTop, int extraSize) {
        int capacity = itsData.itsICode.length;
        if (iCodeTop + extraSize <= capacity) Kit.codeBug();
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

    private static int getExceptionHandler(int[] exceptionTable, int pc)
    {
        // OPT: use binary search
        if (exceptionTable == null) { return -1; }
        int best = -1, bestStart = 0, bestEnd = 0;
        for (int i = 0; i != exceptionTable.length; i += EXCEPTION_SLOT_SIZE) {
            int start = exceptionTable[i + EXCEPTION_TRY_START_SLOT];
            int end = exceptionTable[i + EXCEPTION_TRY_END_SLOT];
            if (start <= pc && pc < end) {
                if (best < 0 || bestStart <= start) {
                    // Check handlers are nested
                    if (best >= 0 && bestEnd < end) Kit.codeBug();
                    best = i;
                    bestStart = start;
                    bestEnd = end;
                }
            }
        }
        return best;
    }

    private static String icodeToName(int icode)
    {
        if (Token.printICode) {
            if (icode <= Token.LAST_BYTECODE_TOKEN) {
                return Token.name(icode);
            } else {
                switch (icode) {
                    case Icode_DUP:              return "dup";
                    case Icode_NAMEINC:          return "nameinc";
                    case Icode_PROPINC:          return "propinc";
                    case Icode_ELEMINC:          return "eleminc";
                    case Icode_VARINC:           return "varinc";
                    case Icode_NAMEDEC:          return "namedec";
                    case Icode_PROPDEC:          return "propdec";
                    case Icode_ELEMDEC:          return "elemdec";
                    case Icode_VARDEC:           return "vardec";
                    case Icode_SCOPE:            return "scope";
                    case Icode_TYPEOFNAME:       return "typeofname";
                    case Icode_GETPROTO:         return "getproto";
                    case Icode_GETPARENT:        return "getparent";
                    case Icode_GETSCOPEPARENT:   return "getscopeparent";
                    case Icode_SETPROTO:         return "setproto";
                    case Icode_SETPARENT:        return "setparent";
                    case Icode_CLOSURE:          return "closure";
                    case Icode_CALLSPECIAL:      return "callspecial";
                    case Icode_RETUNDEF:         return "retundef";
                    case Icode_CATCH:            return "catch";
                    case Icode_GOSUB:            return "gosub";
                    case Icode_RETSUB:           return "retsub";
                    case Icode_LINE:             return "line";
                    case Icode_SHORTNUMBER:      return "shortnumber";
                    case Icode_INTNUMBER:        return "intnumber";
                    case Icode_END:              return "end";
                }
            }
            return "<UNKNOWN ICODE: "+icode+">";
        }
        return "";
    }

    private static void dumpICode(InterpreterData idata)
    {
        if (Token.printICode) {
            int iCodeLength = idata.itsICodeTop;
            byte iCode[] = idata.itsICode;
            String[] strings = idata.itsStringTable;
            PrintStream out = System.out;
            out.println("ICode dump, for " + idata.itsName
                        + ", length = " + iCodeLength);
            out.println("MaxStack = " + idata.itsMaxStack);

            for (int pc = 0; pc < iCodeLength; ) {
                out.flush();
                out.print(" [" + pc + "] ");
                int token = iCode[pc] & 0xff;
                String tname = icodeToName(token);
                int old_pc = pc;
                ++pc;
                int icodeLength = icodeTokenLength(token);
                switch (token) {
                    default:
                        if (icodeLength != 1) Kit.codeBug();
                        out.println(tname);
                        break;

                    case Icode_GOSUB :
                    case Token.GOTO :
                    case Token.IFEQ :
                    case Token.IFNE : {
                        int newPC = getTarget(iCode, pc);
                        out.println(tname + " " + newPC);
                        pc += 2;
                        break;
                    }
                    case Icode_RETSUB :
                    case Token.ENUMINIT :
                    case Token.ENUMNEXT :
                    case Icode_VARINC :
                    case Icode_VARDEC :
                    case Token.GETVAR :
                    case Token.SETVAR :
                    case Token.NEWTEMP :
                    case Token.USETEMP : {
                        int slot = (iCode[pc] & 0xFF);
                        out.println(tname + " " + slot);
                        pc++;
                        break;
                    }
                    case Icode_CALLSPECIAL : {
                        int callType = iCode[pc] & 0xFF;
                        boolean isNew =  (iCode[pc + 1] != 0);
                        int line = getShort(iCode, pc+2);
                        int count = getIndex(iCode, pc + 4);
                        out.println(tname+" "+callType+" "+isNew
                                    +" "+count+" "+line);
                        pc += 8;
                        break;
                    }
                    case Token.REGEXP : {
                        int i = getIndex(iCode, pc);
                        Object regexp = idata.itsRegExpLiterals[i];
                        out.println(tname + " " + regexp);
                        pc += 2;
                        break;
                    }
                    case Icode_CLOSURE : {
                        int i = getIndex(iCode, pc);
                        InterpreterData data2 = idata.itsNestedFunctions[i];
                        out.println(tname + " " + data2);
                        pc += 2;
                        break;
                    }
                    case Token.NEW :
                    case Token.CALL : {
                        int count = getIndex(iCode, pc + 2);
                        String name = strings[getIndex(iCode, pc)];
                        out.println(tname+' '+count+" \""+name+'"');
                        pc += 4;
                        break;
                    }
                    case Icode_SHORTNUMBER : {
                        int value = getShort(iCode, pc);
                        out.println(tname + " " + value);
                        pc += 2;
                        break;
                    }
                    case Icode_INTNUMBER : {
                        int value = getInt(iCode, pc);
                        out.println(tname + " " + value);
                        pc += 4;
                        break;
                    }
                    case Token.NUMBER : {
                        int index = getIndex(iCode, pc);
                        double value = idata.itsDoubleTable[index];
                        out.println(tname + " " + value);
                        pc += 2;
                        break;
                    }
                    case Icode_TYPEOFNAME :
                    case Token.GETBASE :
                    case Token.BINDNAME :
                    case Token.SETNAME :
                    case Token.NAME :
                    case Icode_NAMEINC :
                    case Icode_NAMEDEC :
                    case Token.STRING : {
                        String str = strings[getIndex(iCode, pc)];
                        out.println(tname + " \"" + str + '"');
                        pc += 2;
                        break;
                    }
                    case Icode_LINE : {
                        int line = getShort(iCode, pc);
                        out.println(tname + " : " + line);
                        pc += 2;
                        break;
                    }
                }
                if (old_pc + icodeLength != pc) Kit.codeBug();
            }

            int[] table = idata.itsExceptionTable;
            if (table != null) {
                out.println("Exception handlers: "
                             +table.length / EXCEPTION_SLOT_SIZE);
                for (int i = 0; i != table.length;
                     i += EXCEPTION_SLOT_SIZE)
                {
                    int tryStart     = table[i + EXCEPTION_TRY_START_SLOT];
                    int tryEnd       = table[i + EXCEPTION_TRY_END_SLOT];
                    int catchStart   = table[i + EXCEPTION_CATCH_SLOT];
                    int finallyStart = table[i + EXCEPTION_FINALLY_SLOT];
                    int withDepth    = table[i + EXCEPTION_WITH_DEPTH_SLOT];

                    out.println(" "+tryStart+"\t "+tryEnd+"\t "
                                +catchStart+"\t "+finallyStart
                                +"\t "+withDepth);
                }
            }
            out.flush();
        }
    }

    private static int icodeTokenLength(int icodeToken) {
        switch (icodeToken) {
            case Icode_SCOPE :
            case Icode_GETPROTO :
            case Icode_GETPARENT :
            case Icode_GETSCOPEPARENT :
            case Icode_SETPROTO :
            case Icode_SETPARENT :
            case Token.DELPROP :
            case Token.TYPEOF :
            case Token.NEWSCOPE :
            case Token.ENTERWITH :
            case Token.LEAVEWITH :
            case Token.RETURN :
            case Token.GETTHIS :
            case Token.SETELEM :
            case Token.GETELEM :
            case Token.SETPROP :
            case Token.GETPROP :
            case Icode_PROPINC :
            case Icode_PROPDEC :
            case Icode_ELEMINC :
            case Icode_ELEMDEC :
            case Token.BITNOT :
            case Token.BITAND :
            case Token.BITOR :
            case Token.BITXOR :
            case Token.LSH :
            case Token.RSH :
            case Token.URSH :
            case Token.NOT :
            case Token.POS :
            case Token.NEG :
            case Token.SUB :
            case Token.MUL :
            case Token.DIV :
            case Token.MOD :
            case Token.ADD :
            case Token.POPV :
            case Token.POP :
            case Icode_DUP :
            case Token.LT :
            case Token.GT :
            case Token.LE :
            case Token.GE :
            case Token.IN :
            case Token.INSTANCEOF :
            case Token.EQ :
            case Token.NE :
            case Token.SHEQ :
            case Token.SHNE :
            case Token.ZERO :
            case Token.ONE :
            case Token.NULL :
            case Token.THIS :
            case Token.THISFN :
            case Token.FALSE :
            case Token.TRUE :
            case Token.UNDEFINED :
            case Icode_CATCH:
            case Icode_RETUNDEF:
            case Icode_END:
                return 1;

            case Token.THROW :
                // source line
                return 1 + 2;

            case Icode_GOSUB :
            case Token.GOTO :
            case Token.IFEQ :
            case Token.IFNE :
                // target pc offset
                return 1 + 2;

            case Icode_RETSUB :
            case Token.ENUMINIT :
            case Token.ENUMNEXT :
            case Icode_VARINC :
            case Icode_VARDEC :
            case Token.GETVAR :
            case Token.SETVAR :
            case Token.NEWTEMP :
            case Token.USETEMP :
                // slot index
                return 1 + 1;

            case Icode_CALLSPECIAL :
                // call type
                // is new
                // line number
                // arg count
                return 1 + 1 + 1 + 2 + 2;

            case Token.REGEXP :
                // regexp index
                return 1 + 2;

            case Icode_CLOSURE :
                // index of closure master copy
                return 1 + 2;

            case Token.NEW :
            case Token.CALL :
                // name string index
                // arg count
                return 1 + 2 + 2;

            case Icode_SHORTNUMBER :
                // short number
                return 1 + 2;

            case Icode_INTNUMBER :
                // int number
                return 1 + 4;

            case Token.NUMBER :
                // index of double number
                return 1 + 2;

            case Icode_TYPEOFNAME :
            case Token.GETBASE :
            case Token.BINDNAME :
            case Token.SETNAME :
            case Token.NAME :
            case Icode_NAMEINC :
            case Icode_NAMEDEC :
            case Token.STRING :
                // string index
                return 1 + 2;

            case Icode_LINE :
                // line number
                return 1 + 2;

            default:
                Kit.codeBug(); // Bad icodeToken
                return 0;
        }
    }

    static int[] getLineNumbers(InterpreterData data)
    {
        UintMap presentLines = new UintMap();

        int iCodeLength = data.itsICodeTop;
        byte[] iCode = data.itsICode;
        for (int pc = 0; pc != iCodeLength;) {
            int icodeToken = iCode[pc] & 0xff;
            int icodeLength = icodeTokenLength(icodeToken);
            if (icodeToken == Icode_LINE) {
                if (icodeLength != 3) Kit.codeBug();
                int line = getShort(iCode, pc + 1);
                presentLines.put(line, 0);
            }
            pc += icodeLength;
        }

        return presentLines.getKeys();
    }

    static String getSourcePositionFromStack(Context cx, int[] linep)
    {
        InterpreterData idata = cx.interpreterData;
        linep[0] = getShort(idata.itsICode, cx.interpreterLineIndex);
        return idata.itsSourceFile;
    }

    static Object getEncodedSource(InterpreterData idata)
    {
        if (idata.encodedSource == null) {
            return null;
        }
        return idata.encodedSource.substring(idata.encodedSourceStart,
                                             idata.encodedSourceEnd);
    }

    private static Scriptable[] wrapRegExps(Context cx, Scriptable scope,
                                            InterpreterData idata)
    {
        if (idata.itsRegExpLiterals == null) Kit.codeBug();

        RegExpProxy rep = ScriptRuntime.checkRegExpProxy(cx);
        int N = idata.itsRegExpLiterals.length;
        Scriptable[] array = new Scriptable[N];
        for (int i = 0; i != N; ++i) {
            array[i] = rep.wrapRegExp(cx, scope, idata.itsRegExpLiterals[i]);
        }
        return array;
    }

    private static InterpretedFunction createFunction(Context cx,
                                                      Scriptable scope,
                                                      InterpreterData idata,
                                                      boolean fromEvalCode)
    {
        InterpretedFunction fn = new InterpretedFunction(idata);
        if (idata.itsRegExpLiterals != null) {
            fn.itsRegExps = wrapRegExps(cx, scope, idata);
        }
        ScriptRuntime.initFunction(cx, scope, fn, idata.itsFunctionType,
                                   fromEvalCode);
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
        final int STACK_SHFT = LOCAL_SHFT + idata.itsMaxLocals;

// stack[VAR_SHFT <= i < LOCAL_SHFT]: variables
// stack[LOCAL_SHFT <= i < TRY_STACK_SHFT]: used for newtemp/usetemp
// stack[STACK_SHFT <= i < STACK_SHFT + idata.itsMaxStack]: stack data

// sDbl[i]: if stack[i] is DBL_MRK, sDbl[i] holds the number value

        int maxFrameArray = idata.itsMaxFrameArray;
        if (maxFrameArray != STACK_SHFT + idata.itsMaxStack)
            Kit.codeBug();

        Object[] stack = new Object[maxFrameArray];
        double[] sDbl = new double[maxFrameArray];

        int stackTop = STACK_SHFT - 1;

        int withDepth = 0;

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
            if (!idata.useDynamicScope) {
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
            ScriptRuntime.initScript(cx, scope, fnOrScript, thisObj,
                                     idata.itsFromEvalCode);
        }

        if (idata.itsNestedFunctions != null) {
            if (idata.itsFunctionType != 0 && !idata.itsNeedsActivation)
                Kit.codeBug();
            for (int i = 0; i < idata.itsNestedFunctions.length; i++) {
                InterpreterData fdata = idata.itsNestedFunctions[i];
                if (fdata.itsFunctionType == FunctionNode.FUNCTION_STATEMENT) {
                    createFunction(cx, scope, fdata, idata.itsFromEvalCode);
                }
            }
        }

        // Wrapped regexps for functions are stored in InterpretedFunction
        // but for script which should not contain references to scope
        // the regexps re-wrapped during each script execution
        Scriptable[] scriptRegExps = null;

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

        InterpreterData savedData = cx.interpreterData;
        cx.interpreterData = idata;

        Object result = undefined;
        // If javaException != null on exit, it will be throw instead of
        // normal return
        Throwable javaException = null;
        int exceptionPC = -1;

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

        Loop: for (;;) {
            try {
                switch (iCode[pc] & 0xff) {
    // Back indent to ease imlementation reading

    case Icode_CATCH: {
        // The following code should be executed inside try/catch inside main
        // loop, not in the loop catch block itself to deal withnexceptions
        // from observeInstructionCount. A special bytecode is used only to
        // simplify logic.
        if (javaException == null) Kit.codeBug();

        int pcNew = -1;
        boolean doCatch = false;
        int handlerOffset = getExceptionHandler(idata.itsExceptionTable,
                                                exceptionPC);

        if (handlerOffset >= 0) {

            final int SCRIPT_CAN_CATCH = 0, ONLY_FINALLY = 1, OTHER = 2;
            int exType;
            if (javaException instanceof JavaScriptException) {
                exType = SCRIPT_CAN_CATCH;
            } else if (javaException instanceof EcmaError) {
                // an offical ECMA error object,
                exType = SCRIPT_CAN_CATCH;
            } else if (javaException instanceof WrappedException) {
                exType = SCRIPT_CAN_CATCH;
            } else if (javaException instanceof RuntimeException) {
                exType = ONLY_FINALLY;
            } else {
                // Error instance
                exType = OTHER;
            }

            if (exType != OTHER) {
                // Do not allow for JS to interfere with Error instances
                // (exType == OTHER), as they can be used to terminate
                // long running script
                if (exType == SCRIPT_CAN_CATCH) {
                    // Allow JS to catch only JavaScriptException and
                    // EcmaError
                    pcNew = idata.itsExceptionTable[handlerOffset
                                                    + EXCEPTION_CATCH_SLOT];
                    if (pcNew >= 0) {
                        // Has catch block
                        doCatch = true;
                    }
                }
                if (pcNew < 0) {
                    pcNew = idata.itsExceptionTable[handlerOffset
                                                    + EXCEPTION_FINALLY_SLOT];
                }
            }
        }

        if (debuggerFrame != null && !(javaException instanceof Error)) {
            debuggerFrame.onExceptionThrown(cx, javaException);
        }

        if (pcNew < 0) {
            break Loop;
        }

        // We caught an exception

        // restore scope at try point
        int tryWithDepth = idata.itsExceptionTable[
                               handlerOffset + EXCEPTION_WITH_DEPTH_SLOT];
        while (tryWithDepth != withDepth) {
            if (scope == null) Kit.codeBug();
            scope = ScriptRuntime.leaveWith(scope);
            --withDepth;
        }

        // make stack to contain single exception object
        stackTop = STACK_SHFT;
        if (doCatch) {
            stack[stackTop] = ScriptRuntime.getCatchObject(cx, scope,
                                                           javaException);
        } else {
            // Call finally handler with javaException on stack top to
            // distinguish from normal invocation through GOSUB
            // which would contain DBL_MRK on the stack
            stack[stackTop] = javaException;
        }
        // clear exception
        javaException = null;

        // Notify instruction observer if necessary
        // and point pc and pcPrevBranch to start of catch/finally block
        if (instructionThreshold != 0) {
            if (instructionCount > instructionThreshold) {
                // Note: this can throw Error
                cx.observeInstructionCount(instructionCount);
                instructionCount = 0;
            }
        }
        pcPrevBranch = pc = pcNew;
        continue Loop;
    }
    case Token.THROW: {
        Object value = stack[stackTop];
        if (value == DBL_MRK) value = doubleWrap(sDbl[stackTop]);
        --stackTop;

        int sourceLine = getShort(iCode, pc + 1);
        javaException = new JavaScriptException(value, idata.itsSourceFile,
                                                sourceLine);
        exceptionPC = pc;

        if (instructionThreshold != 0) {
            instructionCount += pc + 1 - pcPrevBranch;
            if (instructionCount > instructionThreshold) {
                cx.observeInstructionCount(instructionCount);
                instructionCount = 0;
            }
        }
        pcPrevBranch = pc = getJavaCatchPC(iCode);
        continue Loop;
    }
    case Token.GE : {
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
    case Token.LE : {
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
    case Token.GT : {
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
    case Token.LT : {
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
    case Token.IN : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        boolean valBln = ScriptRuntime.in(lhs, rhs, scope);
        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
        break;
    }
    case Token.INSTANCEOF : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        boolean valBln = ScriptRuntime.instanceOf(lhs, rhs, scope);
        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
        break;
    }
    case Token.EQ : {
        --stackTop;
        boolean valBln = do_eq(stack, sDbl, stackTop);
        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
        break;
    }
    case Token.NE : {
        --stackTop;
        boolean valBln = !do_eq(stack, sDbl, stackTop);
        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
        break;
    }
    case Token.SHEQ : {
        --stackTop;
        boolean valBln = do_sheq(stack, sDbl, stackTop);
        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
        break;
    }
    case Token.SHNE : {
        --stackTop;
        boolean valBln = !do_sheq(stack, sDbl, stackTop);
        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
        break;
    }
    case Token.IFNE : {
        boolean valBln = stack_boolean(stack, sDbl, stackTop);
        --stackTop;
        if (!valBln) {
            if (instructionThreshold != 0) {
                instructionCount += pc + 3 - pcPrevBranch;
                if (instructionCount > instructionThreshold) {
                    cx.observeInstructionCount(instructionCount);
                    instructionCount = 0;
                }
            }
            pcPrevBranch = pc = getTarget(iCode, pc + 1);
            continue Loop;
        }
        pc += 2;
        break;
    }
    case Token.IFEQ : {
        boolean valBln = stack_boolean(stack, sDbl, stackTop);
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
            continue Loop;
        }
        pc += 2;
        break;
    }
    case Token.GOTO :
        if (instructionThreshold != 0) {
            instructionCount += pc + 3 - pcPrevBranch;
            if (instructionCount > instructionThreshold) {
                cx.observeInstructionCount(instructionCount);
                instructionCount = 0;
            }
        }
        pcPrevBranch = pc = getTarget(iCode, pc + 1);
        continue Loop;
    case Icode_GOSUB :
        ++stackTop;
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = pc + 3;
        if (instructionThreshold != 0) {
            instructionCount += pc + 3 - pcPrevBranch;
            if (instructionCount > instructionThreshold) {
                cx.observeInstructionCount(instructionCount);
                instructionCount = 0;
            }
        }
        pcPrevBranch = pc = getTarget(iCode, pc + 1);                                    continue Loop;
    case Icode_RETSUB : {
        int slot = (iCode[pc + 1] & 0xFF);
        if (instructionThreshold != 0) {
            instructionCount += pc + 2 - pcPrevBranch;
            if (instructionCount > instructionThreshold) {
                cx.observeInstructionCount(instructionCount);
                instructionCount = 0;
            }
        }
        int newPC;
        Object value = stack[LOCAL_SHFT + slot];
        if (value != DBL_MRK) {
            // Invocation from exception handler, restore object to rethrow
            javaException = (Throwable)value;
            exceptionPC = pc;
            newPC = getJavaCatchPC(iCode);
        } else {
            // Normal return from GOSUB
            newPC = (int)sDbl[LOCAL_SHFT + slot];
        }
        pcPrevBranch = pc = newPC;
        continue Loop;
    }
    case Token.POP :
        stack[stackTop] = null;
        stackTop--;
        break;
    case Icode_DUP :
        stack[stackTop + 1] = stack[stackTop];
        sDbl[stackTop + 1] = sDbl[stackTop];
        stackTop++;
        break;
    case Token.POPV :
        result = stack[stackTop];
        if (result == DBL_MRK) result = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = null;
        --stackTop;
        break;
    case Token.RETURN :
        result = stack[stackTop];
        if (result == DBL_MRK) result = doubleWrap(sDbl[stackTop]);
        --stackTop;
        break Loop;
    case Icode_RETUNDEF :
        result = undefined;
        break Loop;
    case Icode_END:
        break Loop;
    case Token.BITNOT : {
        int rIntValue = stack_int32(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = ~rIntValue;
        break;
    }
    case Token.BITAND : {
        int rIntValue = stack_int32(stack, sDbl, stackTop);
        --stackTop;
        int lIntValue = stack_int32(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lIntValue & rIntValue;
        break;
    }
    case Token.BITOR : {
        int rIntValue = stack_int32(stack, sDbl, stackTop);
        --stackTop;
        int lIntValue = stack_int32(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lIntValue | rIntValue;
        break;
    }
    case Token.BITXOR : {
        int rIntValue = stack_int32(stack, sDbl, stackTop);
        --stackTop;
        int lIntValue = stack_int32(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lIntValue ^ rIntValue;
        break;
    }
    case Token.LSH : {
        int rIntValue = stack_int32(stack, sDbl, stackTop);
        --stackTop;
        int lIntValue = stack_int32(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lIntValue << rIntValue;
        break;
    }
    case Token.RSH : {
        int rIntValue = stack_int32(stack, sDbl, stackTop);
        --stackTop;
        int lIntValue = stack_int32(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lIntValue >> rIntValue;
        break;
    }
    case Token.URSH : {
        int rIntValue = stack_int32(stack, sDbl, stackTop) & 0x1F;
        --stackTop;
        double lDbl = stack_double(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = ScriptRuntime.toUint32(lDbl) >>> rIntValue;
        break;
    }
    case Token.ADD :
        --stackTop;
        do_add(stack, sDbl, stackTop);
        break;
    case Token.SUB : {
        double rDbl = stack_double(stack, sDbl, stackTop);
        --stackTop;
        double lDbl = stack_double(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lDbl - rDbl;
        break;
    }
    case Token.NEG : {
        double rDbl = stack_double(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = -rDbl;
        break;
    }
    case Token.POS : {
        double rDbl = stack_double(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = rDbl;
        break;
    }
    case Token.MUL : {
        double rDbl = stack_double(stack, sDbl, stackTop);
        --stackTop;
        double lDbl = stack_double(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lDbl * rDbl;
        break;
    }
    case Token.DIV : {
        double rDbl = stack_double(stack, sDbl, stackTop);
        --stackTop;
        double lDbl = stack_double(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        // Detect the divide by zero or let Java do it ?
        sDbl[stackTop] = lDbl / rDbl;
        break;
    }
    case Token.MOD : {
        double rDbl = stack_double(stack, sDbl, stackTop);
        --stackTop;
        double lDbl = stack_double(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lDbl % rDbl;
        break;
    }
    case Token.NOT : {
        stack[stackTop] = stack_boolean(stack, sDbl, stackTop)
                          ? Boolean.FALSE : Boolean.TRUE;
        break;
    }
    case Token.BINDNAME : {
        String name = strings[getIndex(iCode, pc + 1)];
        stack[++stackTop] = ScriptRuntime.bind(scope, name);
        pc += 2;
        break;
    }
    case Token.GETBASE : {
        String name = strings[getIndex(iCode, pc + 1)];
        stack[++stackTop] = ScriptRuntime.getBase(scope, name);
        pc += 2;
        break;
    }
    case Token.SETNAME : {
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
    case Token.DELPROP : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.delete(cx, scope, lhs, rhs);
        break;
    }
    case Token.GETPROP : {
        String name = (String)stack[stackTop];
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.getProp(lhs, name, scope);
        break;
    }
    case Token.SETPROP : {
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
    case Token.GETELEM :
        do_getElem(cx, stack, sDbl, stackTop, scope);
        --stackTop;
        break;
    case Token.SETELEM :
        do_setElem(cx, stack, sDbl, stackTop, scope);
        stackTop -= 2;
        break;
    case Icode_PROPINC : {
        String name = (String)stack[stackTop];
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.postIncrement(lhs, name, scope);
        break;
    }
    case Icode_PROPDEC : {
        String name = (String)stack[stackTop];
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.postDecrement(lhs, name, scope);
        break;
    }
    case Icode_ELEMINC : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.postIncrementElem(lhs, rhs, scope);
        break;
    }
    case Icode_ELEMDEC : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.postDecrementElem(lhs, rhs, scope);
        break;
    }
    case Token.GETTHIS : {
        Scriptable lhs = (Scriptable)stack[stackTop];
        stack[stackTop] = ScriptRuntime.getThis(lhs);
        break;
    }
    case Token.NEWTEMP : {
        int slot = (iCode[++pc] & 0xFF);
        stack[LOCAL_SHFT + slot] = stack[stackTop];
        sDbl[LOCAL_SHFT + slot] = sDbl[stackTop];
        break;
    }
    case Token.USETEMP : {
        int slot = (iCode[++pc] & 0xFF);
        ++stackTop;
        stack[stackTop] = stack[LOCAL_SHFT + slot];
        sDbl[stackTop] = sDbl[LOCAL_SHFT + slot];
        break;
    }
    case Icode_CALLSPECIAL : {
        if (instructionThreshold != 0) {
            instructionCount += INVOCATION_COST;
            cx.instructionCount = instructionCount;
            instructionCount = -1;
        }
        int callType = iCode[pc + 1] & 0xFF;
        boolean isNew =  (iCode[pc + 2] != 0);
        int sourceLine = getShort(iCode, pc + 3);
        int count = getIndex(iCode, pc + 5);
        stackTop -= count;
        Object[] outArgs = getArgsArray(stack, sDbl, stackTop + 1, count);
        Object functionThis;
        if (isNew) {
            functionThis = null;
        } else {
            functionThis = stack[stackTop];
            if (functionThis == DBL_MRK) {
                functionThis = doubleWrap(sDbl[stackTop]);
            }
            --stackTop;
        }
        Object function = stack[stackTop];
        if (function == DBL_MRK) function = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.callSpecial(
                              cx, function, isNew, functionThis, outArgs,
                              scope, thisObj, callType,
                              idata.itsSourceFile, sourceLine);
        pc += 6;
        instructionCount = cx.instructionCount;
        break;
    }
    case Token.CALL : {
        if (instructionThreshold != 0) {
            instructionCount += INVOCATION_COST;
            cx.instructionCount = instructionCount;
            instructionCount = -1;
        }
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
            throw ScriptRuntime.typeError1("msg.isnt.function",
                                           ScriptRuntime.toString(lhs));
        }

        pc += 4;
        instructionCount = cx.instructionCount;
        break;
    }
    case Token.NEW : {
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
            throw ScriptRuntime.typeError1("msg.isnt.function",
                                           ScriptRuntime.toString(lhs));

        }
        pc += 4;                                                                         instructionCount = cx.instructionCount;
        break;
    }
    case Token.TYPEOF : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.typeof(lhs);
        break;
    }
    case Icode_TYPEOFNAME : {
        String name = strings[getIndex(iCode, pc + 1)];
        stack[++stackTop] = ScriptRuntime.typeofName(scope, name);
        pc += 2;
        break;
    }
    case Token.STRING :
        stack[++stackTop] = strings[getIndex(iCode, pc + 1)];
        pc += 2;
        break;
    case Icode_SHORTNUMBER :
        ++stackTop;
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = getShort(iCode, pc + 1);
        pc += 2;
        break;
    case Icode_INTNUMBER :
        ++stackTop;
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = getInt(iCode, pc + 1);
        pc += 4;
        break;
    case Token.NUMBER :
        ++stackTop;
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = idata.itsDoubleTable[getIndex(iCode, pc + 1)];
        pc += 2;
        break;
    case Token.NAME : {
        String name = strings[getIndex(iCode, pc + 1)];
        stack[++stackTop] = ScriptRuntime.name(scope, name);
        pc += 2;
        break;
    }
    case Icode_NAMEINC : {
        String name = strings[getIndex(iCode, pc + 1)];
        stack[++stackTop] = ScriptRuntime.postIncrement(scope, name);
        pc += 2;
        break;
    }
    case Icode_NAMEDEC : {
        String name = strings[getIndex(iCode, pc + 1)];
        stack[++stackTop] = ScriptRuntime.postDecrement(scope, name);
        pc += 2;
        break;
    }
    case Token.SETVAR : {
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
    case Token.GETVAR : {
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
    case Icode_VARINC : {
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
    case Icode_VARDEC : {
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
    case Token.ZERO :
        ++stackTop;
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = 0;
        break;
    case Token.ONE :
        ++stackTop;
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = 1;
        break;
    case Token.NULL :
        stack[++stackTop] = null;
        break;
    case Token.THIS :
        stack[++stackTop] = thisObj;
        break;
    case Token.THISFN :
        stack[++stackTop] = fnOrScript;
        break;
    case Token.FALSE :
        stack[++stackTop] = Boolean.FALSE;
        break;
    case Token.TRUE :
        stack[++stackTop] = Boolean.TRUE;
        break;
    case Token.UNDEFINED :
        stack[++stackTop] = Undefined.instance;
        break;
    case Token.ENTERWITH : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        scope = ScriptRuntime.enterWith(lhs, scope);
        ++withDepth;
        break;
    }
    case Token.LEAVEWITH :
        scope = ScriptRuntime.leaveWith(scope);
        --withDepth;
        break;
    case Token.NEWSCOPE :
        stack[++stackTop] = ScriptRuntime.newScope();
        break;
    case Token.ENUMINIT : {
        int slot = (iCode[++pc] & 0xFF);
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        stack[LOCAL_SHFT + slot] = ScriptRuntime.initEnum(lhs, scope);
        break;
    }
    case Token.ENUMNEXT : {
        int slot = (iCode[++pc] & 0xFF);
        Object val = stack[LOCAL_SHFT + slot];
        ++stackTop;
        stack[stackTop] = ScriptRuntime.nextEnum(val);
        break;
    }
    case Icode_GETPROTO : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.getProto(lhs, scope);
        break;
    }
    case Icode_GETPARENT : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.getParent(lhs);
        break;
    }
    case Icode_GETSCOPEPARENT : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.getParent(lhs, scope);
        break;
    }
    case Icode_SETPROTO : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.setProto(lhs, rhs, scope);
        break;
    }
    case Icode_SETPARENT : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.setParent(lhs, rhs, scope);
        break;
    }
    case Icode_SCOPE :
        stack[++stackTop] = scope;
        break;
    case Icode_CLOSURE : {
        int i = getIndex(iCode, pc + 1);
        InterpreterData closureData = idata.itsNestedFunctions[i];
        stack[++stackTop] = createFunction(cx, scope, closureData,
                                           idata.itsFromEvalCode);
        pc += 2;
        break;
    }
    case Token.REGEXP : {
        int i = getIndex(iCode, pc + 1);
        Scriptable regexp;
        if (idata.itsFunctionType != 0) {
            regexp = ((InterpretedFunction)fnOrScript).itsRegExps[i];
        } else {
            if (scriptRegExps == null) {
                scriptRegExps = wrapRegExps(cx, scope, idata);
            }
            regexp = scriptRegExps[i];
        }
        stack[++stackTop] = regexp;
        pc += 2;
        break;
    }
    case Icode_LINE : {
        cx.interpreterLineIndex = pc + 1;
        if (debuggerFrame != null) {
            int line = getShort(iCode, pc + 1);
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

                javaException = ex;
                exceptionPC = pc;
                pc = getJavaCatchPC(iCode);
                continue Loop;
            }
        }

        cx.interpreterData = savedData;

        if (debuggerFrame != null) {
            if (javaException != null) {
                    debuggerFrame.onExit(cx, true, javaException);
            } else {
                    debuggerFrame.onExit(cx, false, result);
            }
        }
        if (idata.itsNeedsActivation || debuggerFrame != null) {
            ScriptRuntime.popActivation(cx);
        }

        if (instructionThreshold != 0) {
            if (instructionCount > instructionThreshold) {
                cx.observeInstructionCount(instructionCount);
                instructionCount = 0;
            }
            cx.instructionCount = instructionCount;
        }

        if (javaException != null) {
            if (javaException instanceof JavaScriptException) {
                throw (JavaScriptException)javaException;
            } else if (javaException instanceof RuntimeException) {
                throw (RuntimeException)javaException;
            } else {
                // Must be instance of Error or code bug
                throw (Error)javaException;
            }
        }
        return result;
    }

    private static Object doubleWrap(double x)
    {
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

    private static boolean stack_boolean(Object[] stack, double[] stackDbl,
                                         int i)
    {
        Object x = stack[i];
        if (x == DBL_MRK) {
            double d = stackDbl[i];
            return d == d && d != 0.0;
        } else if (x instanceof Boolean) {
            return ((Boolean)x).booleanValue();
        } else if (x == null || x == Undefined.instance) {
            return false;
        } else if (x instanceof Number) {
            double d = ((Number)x).doubleValue();
            return (d == d && d != 0.0);
        } else {
            return ScriptRuntime.toBoolean(x);
        }
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
            if (lhs instanceof String) {
                String lstr = (String)lhs;
                String rstr = ScriptRuntime.toString(rhs);
                stack[stackTop] = lstr.concat(rstr);
            } else if (rhs instanceof String) {
                String lstr = ScriptRuntime.toString(lhs);
                String rstr = (String)rhs;
                stack[stackTop] = lstr.concat(rstr);
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

    // x + y when x is Number
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
            String lstr = (String)lhs;
            String rstr = ScriptRuntime.toString(rDbl);
            if (left_right_order) {
                stack[stackTop] = lstr.concat(rstr);
            } else {
                stack[stackTop] = rstr.concat(lstr);
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
    private static boolean do_eq(double x, Object y)
    {
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
            double val = stackDbl[stackTop];
            if (lhs == null || lhs == Undefined.instance) {
                throw ScriptRuntime.undefReadError(
                          lhs, ScriptRuntime.toString(val));
            }
            Scriptable obj = (lhs instanceof Scriptable)
                             ? (Scriptable)lhs
                             : ScriptRuntime.toObject(cx, scope, lhs);
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
            double val = stackDbl[stackTop - 1];
            if (lhs == null || lhs == Undefined.instance) {
                throw ScriptRuntime.undefWriteError(
                          lhs, ScriptRuntime.toString(val), rhs);
            }
            Scriptable obj = (lhs instanceof Scriptable)
                             ? (Scriptable)lhs
                             : ScriptRuntime.toObject(cx, scope, lhs);
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
        if (val == Scriptable.NOT_FOUND) Kit.codeBug();
        return val;
    }

    private static void activationPut(NativeFunction f,
                                      Scriptable activation, int slot,
                                      Object value)
    {
        String name = f.argNames[slot];
        activation.put(name, activation, value);
    }

    private static Object execWithNewDomain(Context cx,
                                            final Scriptable scope,
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
            Kit.codeBug();

        CodeBlock code = new CodeBlock() {
            public Object exec(Context cx, Object[] args)
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
                    execWithDomain(cx, idata.securityDomain, code, args);
        } finally {
            cx.interpreterSecurityDomain = savedDomain;
        }
    }

    private static int getJavaCatchPC(byte[] iCode)
    {
        int pc = iCode.length - 1;
        if ((iCode[pc] & 0xFF) != Icode_CATCH) Kit.codeBug();
        return pc;
    }

    private boolean itsInFunctionFlag;

    private InterpreterData itsData;
    private ScriptOrFnNode scriptOrFn;
    private int itsStackDepth = 0;
    private int itsWithDepth = 0;
    private int itsLineNumber = 0;
    private LabelTable itsLabels = new LabelTable();
    private int itsDoubleTableTop;
    private ObjToIntMap itsStrings = new ObjToIntMap(20);
    private String lastAddString;

    private int itsExceptionTableTop = 0;

    // 5 = space for try start/end, catch begin, finally begin and with depth
    private static final int EXCEPTION_SLOT_SIZE       = 5;
    private static final int EXCEPTION_TRY_START_SLOT  = 0;
    private static final int EXCEPTION_TRY_END_SLOT    = 1;
    private static final int EXCEPTION_CATCH_SLOT      = 2;
    private static final int EXCEPTION_FINALLY_SLOT    = 3;
    private static final int EXCEPTION_WITH_DEPTH_SLOT = 4;

    private static final Object DBL_MRK = new Object();
}
