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

    // Stack: ... value1 -> ... value1 value1
        Icode_DUP                       = BASE_ICODE + 1,

    // Stack: ... value2 value1 -> ... value2 value1 value2
        Icode_DUPSECOND                 = BASE_ICODE + 2,

    // Stack: ... value2 value1 -> ... value1 value2
        Icode_SWAP                      = BASE_ICODE + 3,

    // To jump conditionally and pop additional stack value
        Icode_IFEQ_POP                  = BASE_ICODE + 4,

    // various types of ++/--
        Icode_NAMEINC                   = BASE_ICODE + 5,
        Icode_PROPINC                   = BASE_ICODE + 6,
        Icode_ELEMINC                   = BASE_ICODE + 7,
        Icode_VARINC                    = BASE_ICODE + 8,
        Icode_NAMEDEC                   = BASE_ICODE + 9,
        Icode_PROPDEC                   = BASE_ICODE + 10,
        Icode_ELEMDEC                   = BASE_ICODE + 11,
        Icode_VARDEC                    = BASE_ICODE + 12,

    // helper codes to deal with activation
        Icode_SCOPE                     = BASE_ICODE + 13,
        Icode_TYPEOFNAME                = BASE_ICODE + 14,

    // helper for function calls
        Icode_NAME_AND_THIS             = BASE_ICODE + 15,
        Icode_PUSH_PARENT               = BASE_ICODE + 16,

    // Access to parent scope and prototype
        Icode_GETPROTO                  = BASE_ICODE + 17,
        Icode_GETSCOPEPARENT            = BASE_ICODE + 18,
        Icode_SETPROTO                  = BASE_ICODE + 19,
        Icode_SETPARENT                 = BASE_ICODE + 20,

    // Create closure object for nested functions
        Icode_CLOSURE                   = BASE_ICODE + 21,

    // Special calls
        Icode_CALLSPECIAL               = BASE_ICODE + 22,

    // To return undefined value
        Icode_RETUNDEF                  = BASE_ICODE + 23,

    // Exception handling implementation
        Icode_CATCH                     = BASE_ICODE + 24,
        Icode_GOSUB                     = BASE_ICODE + 25,
        Icode_RETSUB                    = BASE_ICODE + 26,

    // To indicating a line number change in icodes.
        Icode_LINE                      = BASE_ICODE + 27,

    // To store shorts and ints inline
        Icode_SHORTNUMBER               = BASE_ICODE + 28,
        Icode_INTNUMBER                 = BASE_ICODE + 29,

    // Last icode
        END_ICODE                       = BASE_ICODE + 30;

    public Object compile(Scriptable scope,
                          CompilerEnvirons compilerEnv,
                          ScriptOrFnNode tree,
                          String encodedSource,
                          boolean returnFunction,
                          Object staticSecurityDomain)
    {
        this.compilerEnv = compilerEnv;
        (new NodeTransformer(compilerEnv)).transform(tree);

        if (Token.printTrees) {
            System.out.println(tree.toStringTree(tree));
        }

        if (returnFunction) {
            tree = tree.getFunctionNode(0);
        }

        Context cx = Context.getContext();
        SecurityController sc = cx.getSecurityController();
        Object dynamicDomain;
        if (sc != null) {
            dynamicDomain = sc.getDynamicSecurityDomain(staticSecurityDomain);
        } else {
            if (staticSecurityDomain != null) {
                throw new IllegalArgumentException();
            }
            dynamicDomain = null;
        }

        scriptOrFn = tree;
        itsData = new InterpreterData(sc, dynamicDomain,
                                      compilerEnv.getLanguageVersion(),
                                      scriptOrFn.getSourceName(),
                                      encodedSource);
        itsData.topLevel = true;

        if (tree instanceof FunctionNode) {
            generateFunctionICode();
            return createFunction(cx, scope, itsData, false);
        } else {
            generateICodeFromTree(scriptOrFn);
            itsData.itsFromEvalCode = compilerEnv.isFromEval();
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

    private void generateFunctionICode()
    {
        FunctionNode theFunction = (FunctionNode)scriptOrFn;

        itsData.itsFunctionType = theFunction.getFunctionType();
        itsData.itsNeedsActivation = theFunction.requiresActivation();
        itsData.itsName = theFunction.getFunctionName();
        if ((theFunction.getParamAndVarCount() & ~0xFF) != 0) {
            // Can not optimize vars as their index should fit 1 byte
            itsData.itsNeedsActivation = true;
        }
        if (!theFunction.getIgnoreDynamicScope()) {
            if (compilerEnv.isUseDynamicScope()) {
                itsData.useDynamicScope = true;
            }
        }

        generateICodeFromTree(theFunction.getLastChild());
    }

    private void generateICodeFromTree(Node tree)
    {
        generateNestedFunctions();

        generateRegExpLiterals();

        int theICodeTop = 0;
        theICodeTop = generateICode(tree, theICodeTop);
        fixLabelGotos();
        // add RETURN_POPV only to scripts as function always ends with RETURN
        if (itsData.itsFunctionType == 0) {
            theICodeTop = addToken(Token.RETURN_POPV, theICodeTop);
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

    private void generateNestedFunctions()
    {
        int functionCount = scriptOrFn.getFunctionCount();
        if (functionCount == 0) return;

        InterpreterData[] array = new InterpreterData[functionCount];
        for (int i = 0; i != functionCount; i++) {
            FunctionNode def = scriptOrFn.getFunctionNode(i);
            Interpreter jsi = new Interpreter();
            jsi.compilerEnv = compilerEnv;
            jsi.scriptOrFn = def;
            jsi.itsData = new InterpreterData(itsData);
            jsi.itsData.itsCheckThis = def.getCheckThis();
            jsi.itsInFunctionFlag = true;
            jsi.generateFunctionICode();
            array[i] = jsi.itsData;
        }
        itsData.itsNestedFunctions = array;
    }

    private void generateRegExpLiterals()
    {
        int N = scriptOrFn.getRegexpCount();
        if (N == 0) return;

        Context cx = Context.getContext();
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
        int savedStackDepth = itsStackDepth;
        int stackDelta = 0; // expected stack change for subtree code
        boolean stackShouldBeZero = false;
        switch (type) {

            case Token.FUNCTION : {
                int fnIndex = node.getExistingIntProp(Node.FUNCTION_PROP);
                FunctionNode fn = scriptOrFn.getFunctionNode(fnIndex);
                if (fn.getFunctionType() != FunctionNode.FUNCTION_STATEMENT) {
                    stackDelta = 1;
                    // Only function expressions or function expression
                    // statements needs closure code creating new function
                    // object on stack as function statements are initialized
                    // at script/function start
                    iCodeTop = addIcode(Icode_CLOSURE, iCodeTop);
                    iCodeTop = addIndex(fnIndex, iCodeTop);
                    itsStackDepth++;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                } else {
                    stackShouldBeZero = true;
                }
                break;
            }

            case Token.SCRIPT :
                stackShouldBeZero = true;
                iCodeTop = updateLineNumber(node, iCodeTop);
                while (child != null) {
                    if (child.getType() != Token.FUNCTION)
                        iCodeTop = generateICode(child, iCodeTop);
                    child = child.getNext();
                }
                break;

            case Token.CASE :
                // Skip case condition
                child = child.getNext();
                // fallthrough
            case Token.LABEL :
            case Token.LOOP :
            case Token.DEFAULT :
            case Token.BLOCK :
            case Token.EMPTY :
            case Token.WITH :
                stackShouldBeZero = true;
                iCodeTop = updateLineNumber(node, iCodeTop);
                while (child != null) {
                    iCodeTop = generateICode(child, iCodeTop);
                    child = child.getNext();
                }
                break;

            case Token.LOCAL_BLOCK :
                stackShouldBeZero = true;
                if ((itsLocalTop & ~0xFF) != 0) {
                    throw Context.reportRuntimeError(
                        "Program too complex (out of locals)");
                }
                node.putIntProp(Node.LOCAL_PROP, itsLocalTop);
                ++itsLocalTop;
                if (itsLocalTop > itsData.itsMaxLocals) {
                    itsData.itsMaxLocals = itsLocalTop;
                }
                iCodeTop = updateLineNumber(node, iCodeTop);
                while (child != null) {
                    iCodeTop = generateICode(child, iCodeTop);
                    child = child.getNext();
                }
                --itsLocalTop;
                break;

            case Token.LOCAL_LOAD : {
                stackDelta = 1;
                iCodeTop = addToken(Token.LOCAL_LOAD, iCodeTop);
                iCodeTop = addLocalBlockRef(node, iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;
            }

            case Token.COMMA :
                stackDelta = 1;
                iCodeTop = generateICode(child, iCodeTop);
                while (null != (child = child.getNext())) {
                    if (1 != itsStackDepth - savedStackDepth) Kit.codeBug();
                    iCodeTop = addToken(Token.POP, iCodeTop);
                    itsStackDepth--;
                    iCodeTop = generateICode(child, iCodeTop);
                }
                break;

            case Token.INIT_LIST :
                stackDelta = 1;
                iCodeTop = generateICode(child, iCodeTop);
                while (null != (child = child.getNext())) {
                    if (1 != itsStackDepth - savedStackDepth) Kit.codeBug();
                    iCodeTop = addIcode(Icode_DUP, iCodeTop);
                    // No stack adjusting: USE_STACK in subtree will do it
                    iCodeTop = generateICode(child, iCodeTop);
                    iCodeTop = addToken(Token.POP, iCodeTop);
                    itsStackDepth--;
                }
                break;

            case Token.USE_STACK:
                // Indicates that stack was modified externally,
                // like placed catch object
                stackDelta = 1;
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;

            case Token.SWITCH : {
                stackShouldBeZero = true;
                Node.Jump switchNode = (Node.Jump)node;
                iCodeTop = updateLineNumber(switchNode, iCodeTop);
                iCodeTop = generateICode(child, iCodeTop);

                ObjArray cases = (ObjArray) switchNode.getProp(Node.CASES_PROP);
                for (int i = 0; i < cases.size(); i++) {
                    Node thisCase = (Node)cases.get(i);
                    Node first = thisCase.getFirstChild();
                    // the case expression is the firstmost child
                    // the rest will be generated when the case
                    // statements are encountered as siblings of
                    // the switch statement.
                    iCodeTop = generateICode(first, iCodeTop);
                    iCodeTop = addIcode(Icode_DUPSECOND, iCodeTop);
                    itsStackDepth++;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                    iCodeTop = addToken(Token.SHEQ, iCodeTop);
                    itsStackDepth--;
                    Node.Target target = new Node.Target();
                    thisCase.addChildAfter(target, first);
                    // If true, Icode_IFEQ_POP will jump and remove case value
                    // from stack
                    iCodeTop = addGoto(target, Icode_IFEQ_POP, iCodeTop);
                    itsStackDepth--;
                }
                iCodeTop = addToken(Token.POP, iCodeTop);
                itsStackDepth--;

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
                stackShouldBeZero = true;
                markTargetLabel((Node.Target)node, iCodeTop);
                break;

            case Token.NEW :
            case Token.CALL : {
                stackDelta = 1;
                if (type == Token.NEW) {
                    iCodeTop = generateICode(child, iCodeTop);
                } else {
                    iCodeTop = generateCallFunAndThis(child, iCodeTop);
                    if (itsStackDepth - savedStackDepth != 2)
                        Kit.codeBug();
                }
                String functionName = null;
                int childType = child.getType();
                if (childType == Token.NAME || childType == Token.GETPROP
                    || childType == Token.GETVAR)
                {
                    functionName = lastAddString;
                }
                int argCount = 0;
                while ((child = child.getNext()) != null) {
                    iCodeTop = generateICode(child, iCodeTop);
                    ++argCount;
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
                    iCodeTop = addString(type, functionName, iCodeTop);
                }
                // adjust stack
                if (type == Token.NEW) {
                    // f, args -> results
                   itsStackDepth -= argCount;
                } else {
                    // f, thisObj, args -> results
                   itsStackDepth -= (argCount + 1);
                }
                iCodeTop = addIndex(argCount, iCodeTop);
                if (argCount > itsData.itsMaxCalleeArgs)
                    itsData.itsMaxCalleeArgs = argCount;
                break;
            }

            case Token.IFEQ :
            case Token.IFNE :
                iCodeTop = generateICode(child, iCodeTop);
                itsStackDepth--;    // after the conditional GOTO, really
                    // fall thru...
            case Token.GOTO : {
                stackShouldBeZero = true;
                Node.Target target = ((Node.Jump)node).target;
                iCodeTop = addGoto(target, (byte) type, iCodeTop);
                break;
            }

            case Token.JSR : {
                stackShouldBeZero = true;
                Node.Target target = ((Node.Jump)node).target;
                iCodeTop = addGoto(target, Icode_GOSUB, iCodeTop);
                break;
            }

            case Token.FINALLY : {
                stackShouldBeZero = true;
                // Account for incomming exception or GOTOSUB address
                ++itsStackDepth;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;

                int finallyRegister = getLocalBlockRef(node);
                iCodeTop = addToken(Token.LOCAL_SAVE, iCodeTop);
                iCodeTop = addByte(finallyRegister, iCodeTop);
                itsStackDepth--;
                while (child != null) {
                    iCodeTop = generateICode(child, iCodeTop);
                    if (itsStackDepth != 0) Kit.codeBug();
                    child = child.getNext();
                }
                iCodeTop = addIcode(Icode_RETSUB, iCodeTop);
                iCodeTop = addByte(finallyRegister, iCodeTop);
                break;
            }

            case Token.AND :
            case Token.OR : {
                stackDelta = 1;
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addIcode(Icode_DUP, iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                int afterSecondJumpStart = iCodeTop;
                int jump = (type == Token.AND) ? Token.IFNE : Token.IFEQ;
                iCodeTop = addForwardGoto(jump, iCodeTop);
                itsStackDepth--;
                iCodeTop = addToken(Token.POP, iCodeTop);
                itsStackDepth--;
                child = child.getNext();
                iCodeTop = generateICode(child, iCodeTop);
                resolveForwardGoto(afterSecondJumpStart, iCodeTop);
                break;
            }

            case Token.HOOK : {
                stackDelta = 1;
                Node ifThen = child.getNext();
                Node ifElse = ifThen.getNext();
                iCodeTop = generateICode(child, iCodeTop);
                int elseJumpStart = iCodeTop;
                iCodeTop = addForwardGoto(Token.IFNE, iCodeTop);
                itsStackDepth--;
                iCodeTop = generateICode(ifThen, iCodeTop);
                int afterElseJumpStart = iCodeTop;
                iCodeTop = addForwardGoto(Token.GOTO, iCodeTop);
                resolveForwardGoto(elseJumpStart, iCodeTop);
                itsStackDepth = savedStackDepth;
                iCodeTop = generateICode(ifElse, iCodeTop);
                resolveForwardGoto(afterElseJumpStart, iCodeTop);
                break;
            }

            case Token.GETPROP : {
                stackDelta = 1;
                iCodeTop = generateICode(child, iCodeTop);
                int special = node.getIntProp(Node.SPECIAL_PROP_PROP, 0);
                if (special != 0) {
                    if (special == Node.SPECIAL_PROP_PROTO) {
                        iCodeTop = addIcode(Icode_GETPROTO, iCodeTop);
                    } else if (special == Node.SPECIAL_PROP_PARENT) {
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
                stackDelta = 1;
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
                stackDelta = 1;
                iCodeTop = generateICode(child, iCodeTop);
                if (type == Token.VOID) {
                    iCodeTop = addToken(Token.POP, iCodeTop);
                    iCodeTop = addToken(Token.UNDEFINED, iCodeTop);
                } else {
                    iCodeTop = addToken(type, iCodeTop);
                }
                break;

            case Token.SETPROP :
            case Token.SETPROP_OP : {
                stackDelta = 1;
                iCodeTop = generateICode(child, iCodeTop);
                child = child.getNext();
                int special = node.getIntProp(Node.SPECIAL_PROP_PROP, 0);
                if (special != 0) {
                    if (type == Token.SETPROP_OP) {
                        iCodeTop = addIcode(Icode_DUP, iCodeTop);
                        if (itsStackDepth > itsData.itsMaxStack)
                            itsData.itsMaxStack = itsStackDepth;
                        if (special == Node.SPECIAL_PROP_PROTO) {
                            iCodeTop = addIcode(Icode_GETPROTO, iCodeTop);
                        } else if (special == Node.SPECIAL_PROP_PARENT) {
                            iCodeTop = addIcode(Icode_GETSCOPEPARENT, iCodeTop);
                        } else {
                            badTree(node);
                        }
                        // Compensate for the following USE_STACK
                        itsStackDepth--;
                    }
                    iCodeTop = generateICode(child, iCodeTop);
                    if (special == Node.SPECIAL_PROP_PROTO) {
                        iCodeTop = addIcode(Icode_SETPROTO, iCodeTop);
                    } else if (special == Node.SPECIAL_PROP_PARENT) {
                        iCodeTop = addIcode(Icode_SETPARENT, iCodeTop);
                    } else {
                        badTree(node);
                    }
                    itsStackDepth--;
                } else {
                    iCodeTop = generateICode(child, iCodeTop);
                    child = child.getNext();
                    if (type == Token.SETPROP_OP) {
                        iCodeTop = addIcode(Icode_DUPSECOND, iCodeTop);
                        iCodeTop = addIcode(Icode_DUPSECOND, iCodeTop);
                        itsStackDepth += 2;
                        if (itsStackDepth > itsData.itsMaxStack)
                            itsData.itsMaxStack = itsStackDepth;
                        iCodeTop = addToken(Token.GETPROP, iCodeTop);
                        itsStackDepth--;
                        // Compensate for the following USE_STACK
                        itsStackDepth--;
                    }
                    iCodeTop = generateICode(child, iCodeTop);
                    iCodeTop = addToken(Token.SETPROP, iCodeTop);
                    itsStackDepth -= 2;
                }
                break;
            }

            case Token.SETELEM :
            case Token.SETELEM_OP :
                stackDelta = 1;
                iCodeTop = generateICode(child, iCodeTop);
                child = child.getNext();
                iCodeTop = generateICode(child, iCodeTop);
                child = child.getNext();
                if (type == Token.SETELEM_OP) {
                    iCodeTop = addIcode(Icode_DUPSECOND, iCodeTop);
                    iCodeTop = addIcode(Icode_DUPSECOND, iCodeTop);
                    itsStackDepth += 2;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                    iCodeTop = addToken(Token.GETELEM, iCodeTop);
                    itsStackDepth--;
                    // Compensate for the following USE_STACK
                    itsStackDepth--;
                }
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addToken(Token.SETELEM, iCodeTop);
                itsStackDepth -= 2;
                break;

            case Token.SETNAME :
                stackDelta = 1;
                iCodeTop = generateICode(child, iCodeTop);
                child = child.getNext();
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addString(Token.SETNAME, firstChild.getString(), iCodeTop);
                itsStackDepth--;
                break;

            case Token.TYPEOFNAME : {
                stackDelta = 1;
                String name = node.getString();
                int index = -1;
                // use typeofname if an activation frame exists
                // since the vars all exist there instead of in jregs
                if (itsInFunctionFlag && !itsData.itsNeedsActivation)
                    index = scriptOrFn.getParamOrVarIndex(name);
                if (index == -1) {
                    iCodeTop = addString(Icode_TYPEOFNAME, name, iCodeTop);
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

            case Token.GETBASE :
            case Token.BINDNAME :
            case Token.NAME :
            case Token.STRING :
                stackDelta = 1;
                iCodeTop = addString(type, node.getString(), iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;

            case Token.INC :
            case Token.DEC : {
                stackDelta = 1;
                int childType = child.getType();
                switch (childType) {
                    case Token.GETVAR : {
                        String name = child.getString();
                        if (itsData.itsNeedsActivation) {
                            iCodeTop = addIcode(Icode_SCOPE, iCodeTop);
                            iCodeTop = addString(Token.STRING, name, iCodeTop);
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
                        iCodeTop = addString(type == Token.INC
                                             ? Icode_NAMEINC
                                             : Icode_NAMEDEC,
											 child.getString(), iCodeTop);
                        itsStackDepth++;
                        if (itsStackDepth > itsData.itsMaxStack)
                            itsData.itsMaxStack = itsStackDepth;
                        break;
                    }
                }
                break;
            }

            case Token.NUMBER : {
                stackDelta = 1;
                double num = node.getDouble();
                int inum = (int)num;
                if (inum == num) {
                    if (inum == 0) {
                        iCodeTop = addToken(Token.ZERO, iCodeTop);
                        // Check for negative zero
                        if (1.0 / num < 0.0) {
                            iCodeTop = addToken(Token.NEG, iCodeTop);
                        }
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

            case Token.POPV :
                stackShouldBeZero = true;
                // fallthrough
            case Token.POP :
                iCodeTop = updateLineNumber(node, iCodeTop);
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addToken(type, iCodeTop);
                itsStackDepth--;
                break;

            case Token.ENTERWITH :
                stackShouldBeZero = true;
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addToken(Token.ENTERWITH, iCodeTop);
                itsStackDepth--;
                break;

            case Token.CATCH_SCOPE :
                stackDelta = 1;
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addString(Token.CATCH_SCOPE, node.getString(), iCodeTop);
                break;

            case Token.LEAVEWITH :
                stackShouldBeZero = true;
                iCodeTop = addToken(Token.LEAVEWITH, iCodeTop);
                break;

            case Token.TRY : {
                stackShouldBeZero = true;
                Node.Jump tryNode = (Node.Jump)node;
                int exceptionObjectLocal = getLocalBlockRef(tryNode);
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

                    } else if (child == finallyTarget) {
                        if (tryEnd < 0) {
                            tryEnd = iCodeTop;
                        }
                        finallyStart = iCodeTop;

                        markTargetLabel((Node.Target)child, iCodeTop);
                        generated = true;
                    }

                    if (!generated) {
                        iCodeTop = generateICode(child, iCodeTop);
                    }
                    child = child.getNext();
                }
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
                                    itsWithDepth, exceptionObjectLocal);
                break;
            }

            case Token.THROW :
                stackShouldBeZero = true;
                iCodeTop = updateLineNumber(node, iCodeTop);
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addToken(Token.THROW, iCodeTop);
                iCodeTop = addShort(itsLineNumber, iCodeTop);
                itsStackDepth--;
                break;

            case Token.RETURN :
                stackShouldBeZero = true;
                iCodeTop = updateLineNumber(node, iCodeTop);
                if (child != null) {
                    iCodeTop = generateICode(child, iCodeTop);
                    iCodeTop = addToken(Token.RETURN, iCodeTop);
                    itsStackDepth--;
                } else {
                    iCodeTop = addIcode(Icode_RETUNDEF, iCodeTop);
                }
                break;

            case Token.RETURN_POPV :
                stackShouldBeZero = true;
                iCodeTop = updateLineNumber(node, iCodeTop);
                iCodeTop = addToken(Token.RETURN_POPV, iCodeTop);
                break;

            case Token.GETVAR : {
                stackDelta = 1;
                String name = node.getString();
                if (itsData.itsNeedsActivation) {
                    // SETVAR handled this by turning into a SETPROP, but
                    // we can't do that to a GETVAR without manufacturing
                    // bogus children. Instead we use a special op to
                    // push the current scope.
                    iCodeTop = addIcode(Icode_SCOPE, iCodeTop);
                    iCodeTop = addString(Token.STRING, name, iCodeTop);
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
                stackDelta = 1;
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
                stackDelta = 1;
                iCodeTop = addToken(type, iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;

            case Token.ENUM_INIT :
                stackShouldBeZero = true;
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addToken(Token.ENUM_INIT, iCodeTop);
                iCodeTop = addLocalBlockRef(node, iCodeTop);
                itsStackDepth--;
                break;

            case Token.ENUM_NEXT :
            case Token.ENUM_ID : {
                stackDelta = 1;
                iCodeTop = addToken(type, iCodeTop);
                iCodeTop = addLocalBlockRef(node, iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;
            }

            case Token.REGEXP : {
                stackDelta = 1;
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
        if (stackDelta != itsStackDepth - savedStackDepth) {
            //System.out.println("Bad stack delta: type="+Token.name(type)+" expected="+stackDelta+" real="+ (itsStackDepth - savedStackDepth));
            Kit.codeBug();
        }
        if (stackShouldBeZero && !(stackDelta == 0 && itsStackDepth == 0)) {
            Kit.codeBug();
        }

        return iCodeTop;
    }

    private int generateCallFunAndThis(Node left, int iCodeTop)
    {
        // Generate code to place on stack function and thisObj
        int type = left.getType();
        if (type == Token.NAME) {
            String name = left.getString();
            iCodeTop = addString(Icode_NAME_AND_THIS, name, iCodeTop);
            itsStackDepth += 2;
            if (itsStackDepth > itsData.itsMaxStack)
                itsData.itsMaxStack = itsStackDepth;
        } else if (type == Token.GETPROP || type == Token.GETELEM) {
            // For Call(GetProp(a, b), c, d) or Call(GetElem(a, b), c, d)
            // generate code to calculate a, dup it, calculate
            // GetProp(use_stack, b) or GetElem(use_stack, b),
            // and swap to get function, thisObj layout
            Node leftLeft = left.getFirstChild();
            left.removeChild(leftLeft);
            left.addChildToFront(new Node(Token.USE_STACK));
            iCodeTop = generateICode(leftLeft, iCodeTop);
            iCodeTop = addIcode(Icode_DUP, iCodeTop);
            // No stack adjusting: USE_STACK in subtree will do it
            iCodeTop = generateICode(left, iCodeTop);
            iCodeTop = addIcode(Icode_SWAP, iCodeTop);
        } else {
            // Including Token.GETVAR
            iCodeTop = generateICode(left, iCodeTop);
            iCodeTop = addIcode(Icode_PUSH_PARENT, iCodeTop);
            itsStackDepth += 1;
            if (itsStackDepth > itsData.itsMaxStack)
                itsData.itsMaxStack = itsStackDepth;
        }
        return iCodeTop;
    }

    private int getLocalBlockRef(Node node)
    {
        Node localBlock = (Node)node.getProp(Node.LOCAL_BLOCK_PROP);
        return localBlock.getExistingIntProp(Node.LOCAL_PROP);
    }

    private int addLocalBlockRef(Node node, int iCodeTop)
    {
        int localSlot = getLocalBlockRef(node);
        iCodeTop = addByte(localSlot, iCodeTop);
        return iCodeTop;
    }

    private int getTargetLabel(Node.Target target)
    {
        int label = target.labelId;
        if (label != -1) {
            return label;
        }
        label = itsLabelTableTop;
        if (itsLabelTable == null || label == itsLabelTable.length) {
            if (itsLabelTable == null) {
                itsLabelTable = new int[MIN_LABEL_TABLE_SIZE];
            }else {
                int[] tmp = new int[itsLabelTable.length * 2];
                System.arraycopy(itsLabelTable, 0, tmp, 0, label);
                itsLabelTable = tmp;
            }
        }
        itsLabelTableTop = label + 1;
        itsLabelTable[label] = -1;

        target.labelId = label;
        return label;
    }

    private void markTargetLabel(Node.Target target, int iCodeTop)
    {
        int label = getTargetLabel(target);
        if (itsLabelTable[label] != -1) {
            // Can mark label only once
            Kit.codeBug();
        }
        itsLabelTable[label] = iCodeTop;
    }

    private int addGoto(Node.Target target, int gotoOp, int iCodeTop)
    {
        int label = getTargetLabel(target);
        if (!(label < itsLabelTableTop)) Kit.codeBug();
        int targetPC = itsLabelTable[label];

        int gotoPC = iCodeTop;
        if (gotoOp > BASE_ICODE) {
            iCodeTop = addIcode(gotoOp, iCodeTop);
        } else {
            iCodeTop = addToken(gotoOp, iCodeTop);
        }
        int jumpSite = iCodeTop;
        iCodeTop = addShort(0, iCodeTop);

        if (targetPC != -1) {
            recordJumpOffset(jumpSite, targetPC - gotoPC);
        } else {
            int top = itsFixupTableTop;
            if (itsFixupTable == null || top == itsFixupTable.length) {
                if (itsFixupTable == null) {
                    itsFixupTable = new long[MIN_FIXUP_TABLE_SIZE];
                } else {
                    long[] tmp = new long[itsFixupTable.length * 2];
                    System.arraycopy(itsFixupTable, 0, tmp, 0, top);
                    itsFixupTable = tmp;
                }
            }
            itsFixupTableTop = top + 1;
            itsFixupTable[top] = ((long)label << 32) | jumpSite;
        }
        return iCodeTop;
    }

    private void fixLabelGotos()
    {
        byte[] codeBuffer = itsData.itsICode;
        for (int i = 0; i < itsFixupTableTop; i++) {
            long fixup = itsFixupTable[i];
            int label = (int)(fixup >> 32);
            int fixupSite = (int)fixup;
            int pc = itsLabelTable[label];
            if (pc == -1) {
                // Unlocated label
                throw new RuntimeException();
            }
            // -1 to get delta from instruction start
            int offset = pc - (fixupSite - 1);
            if ((short)offset != offset) {
                throw new RuntimeException
                    ("Program too complex: too big jump offset");
            }
            codeBuffer[fixupSite] = (byte)(offset >> 8);
            codeBuffer[fixupSite + 1] = (byte)offset;
        }
        itsFixupTableTop = 0;
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

    private int addByte(int value, int iCodeTop)
    {
        byte[] array = itsData.itsICode;
        if (iCodeTop == array.length) {
            array = increaseICodeCapasity(iCodeTop, 1);
        }
        array[iCodeTop++] = (byte)value;
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
        if (!(BASE_ICODE < icode && icode < END_ICODE)) Kit.codeBug();
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

    private int addString(int op, String str, int iCodeTop)
    {
        if (op > BASE_ICODE) {
            iCodeTop = addIcode(op, iCodeTop);
        } else {
            iCodeTop = addToken(op, iCodeTop);
        }
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
                                     int withDepth, int exceptionObjectLocal)
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
        table[top + EXCEPTION_LOCAL_SLOT]      = exceptionObjectLocal;

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
                    case Icode_DUP:              return "DUP";
                    case Icode_DUPSECOND:        return "DUPSECOND";
                    case Icode_SWAP:             return "SWAP";
                    case Icode_IFEQ_POP:         return "IFEQ_POP";
                    case Icode_NAMEINC:          return "NAMEINC";
                    case Icode_PROPINC:          return "PROPINC";
                    case Icode_ELEMINC:          return "ELEMINC";
                    case Icode_VARINC:           return "VARINC";
                    case Icode_NAMEDEC:          return "NAMEDEC";
                    case Icode_PROPDEC:          return "PROPDEC";
                    case Icode_ELEMDEC:          return "ELEMDEC";
                    case Icode_VARDEC:           return "VARDEC";
                    case Icode_SCOPE:            return "SCOPE";
                    case Icode_TYPEOFNAME:       return "TYPEOFNAME";
                    case Icode_NAME_AND_THIS:    return "NAME_AND_THIS";
                    case Icode_GETPROTO:         return "GETPROTO";
                    case Icode_PUSH_PARENT:      return "PUSH_PARENT";
                    case Icode_GETSCOPEPARENT:   return "GETSCOPEPARENT";
                    case Icode_SETPROTO:         return "SETPROTO";
                    case Icode_SETPARENT:        return "SETPARENT";
                    case Icode_CLOSURE:          return "CLOSURE";
                    case Icode_CALLSPECIAL:      return "CALLSPECIAL";
                    case Icode_RETUNDEF:         return "RETUNDEF";
                    case Icode_CATCH:            return "CATCH";
                    case Icode_GOSUB:            return "GOSUB";
                    case Icode_RETSUB:           return "RETSUB";
                    case Icode_LINE:             return "LINE";
                    case Icode_SHORTNUMBER:      return "SHORTNUMBER";
                    case Icode_INTNUMBER:        return "INTNUMBER";
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
                int token = iCode[pc] & 0xFF;
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
                    case Token.IFNE :
                    case Icode_IFEQ_POP : {
                        int newPC = getTarget(iCode, pc);
                        out.println(tname + " " + newPC);
                        pc += 2;
                        break;
                    }
                    case Icode_RETSUB :
                    case Token.ENUM_INIT :
                    case Token.ENUM_NEXT :
                    case Token.ENUM_ID :
                    case Icode_VARINC :
                    case Icode_VARDEC :
                    case Token.GETVAR :
                    case Token.SETVAR :
                    case Token.LOCAL_SAVE :
                    case Token.LOCAL_LOAD : {
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
                        pc += 6;
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
                    case Token.THROW : {
                        int line = getShort(iCode, pc);
                        out.println(tname + " : " + line);
                        pc += 2;
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
                    case Token.CATCH_SCOPE :
                    case Icode_TYPEOFNAME :
                    case Icode_NAME_AND_THIS :
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
                    int tryStart       = table[i + EXCEPTION_TRY_START_SLOT];
                    int tryEnd         = table[i + EXCEPTION_TRY_END_SLOT];
                    int catchStart     = table[i + EXCEPTION_CATCH_SLOT];
                    int finallyStart   = table[i + EXCEPTION_FINALLY_SLOT];
                    int withDepth      = table[i + EXCEPTION_WITH_DEPTH_SLOT];
                    int exceptionLocal = table[i + EXCEPTION_LOCAL_SLOT];

                    out.println(" tryStart="+tryStart+" tryEnd="+tryEnd
                                +" catchStart="+catchStart
                                +" finallyStart="+finallyStart
                                +" withDepth="+withDepth
                                +" exceptionLocal="+exceptionLocal);
                }
            }
            out.flush();
        }
    }

    private static int icodeTokenLength(int icodeToken) {
        switch (icodeToken) {
            case Icode_SCOPE :
            case Icode_GETPROTO :
            case Icode_PUSH_PARENT :
            case Icode_GETSCOPEPARENT :
            case Icode_SETPROTO :
            case Icode_SETPARENT :
            case Token.DELPROP :
            case Token.TYPEOF :
            case Token.ENTERWITH :
            case Token.LEAVEWITH :
            case Token.RETURN :
            case Token.RETURN_POPV :
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
            case Icode_DUPSECOND :
            case Icode_SWAP :
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
                return 1;

            case Token.THROW :
                // source line
                return 1 + 2;

            case Icode_GOSUB :
            case Token.GOTO :
            case Token.IFEQ :
            case Token.IFNE :
            case Icode_IFEQ_POP :
                // target pc offset
                return 1 + 2;

            case Icode_RETSUB :
            case Token.ENUM_INIT :
            case Token.ENUM_NEXT :
            case Token.ENUM_ID :
            case Icode_VARINC :
            case Icode_VARDEC :
            case Token.GETVAR :
            case Token.SETVAR :
            case Token.LOCAL_SAVE :
            case Token.LOCAL_LOAD :
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

            case Token.CATCH_SCOPE :
            case Icode_TYPEOFNAME :
            case Icode_NAME_AND_THIS :
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
            int icodeToken = iCode[pc] & 0xFF;
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

    static String getEncodedSource(InterpreterData idata)
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
            if (argsDbl != null) {
                args = getArgsArray(args, argsDbl, argShift, argCount);
            }
            SecurityController sc = idata.securityController;
            Object savedDomain = cx.interpreterSecurityDomain;
            cx.interpreterSecurityDomain = idata.securityDomain;
            try {
                return sc.callWithDomain(idata.securityDomain, cx, fnOrScript,
                                         scope, thisObj, args);
            } finally {
                cx.interpreterSecurityDomain = savedDomain;
            }
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
                switch (iCode[pc] & 0xFF) {
    // Back indent to ease imlementation reading

    case Icode_CATCH: {
        // The following code should be executed inside try/catch inside main
        // loop, not in the loop catch block itself to deal with exceptions
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
            } else if (javaException instanceof EvaluatorException) {
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

        if (doCatch) {
            stackTop = STACK_SHFT - 1;
            int exLocal = idata.itsExceptionTable[
                              handlerOffset + EXCEPTION_LOCAL_SLOT];
            stack[LOCAL_SHFT + exLocal] = ScriptRuntime.getCatchObject(
                                              cx, scope, javaException);
        } else {
            stackTop = STACK_SHFT;
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
            valBln = (rDbl <= lDbl);
        } else {
            valBln = ScriptRuntime.cmp_LE(rhs, lhs);
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
            valBln = (lDbl <= rDbl);
        } else {
            valBln = ScriptRuntime.cmp_LE(lhs, rhs);
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
            valBln = (rDbl < lDbl);
        } else {
            valBln = ScriptRuntime.cmp_LT(rhs, lhs);
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
            valBln = (lDbl < rDbl);
        } else {
            valBln = ScriptRuntime.cmp_LT(lhs, rhs);
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
    case Icode_IFEQ_POP : {
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
            stack[stackTop--] = null;
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
    case Icode_DUPSECOND : {
        stack[stackTop + 1] = stack[stackTop - 1];
        sDbl[stackTop + 1] = sDbl[stackTop - 1];
        stackTop++;
        break;
    }
    case Icode_SWAP : {
        Object o = stack[stackTop];
        stack[stackTop] = stack[stackTop - 1];
        stack[stackTop - 1] = o;
        double d = sDbl[stackTop];
        sDbl[stackTop] = sDbl[stackTop - 1];
        sDbl[stackTop - 1] = d;
        break;
    }
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
    case Token.RETURN_POPV :
        break Loop;
    case Icode_RETUNDEF :
        result = undefined;
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
        stack[stackTop] = ScriptRuntime.postIncrDecr(lhs, name, scope, true);
        break;
    }
    case Icode_PROPDEC : {
        String name = (String)stack[stackTop];
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.postIncrDecr(lhs, name, scope, false);
        break;
    }
    case Icode_ELEMINC : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.postIncrDecrElem(lhs, rhs, scope, true);
        break;
    }
    case Icode_ELEMDEC : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.postIncrDecrElem(lhs, rhs, scope, false);
        break;
    }
    case Token.LOCAL_SAVE : {
        int slot = (iCode[++pc] & 0xFF);
        stack[LOCAL_SHFT + slot] = stack[stackTop];
        sDbl[LOCAL_SHFT + slot] = sDbl[stackTop];
        --stackTop;
        break;
    }
    case Token.LOCAL_LOAD : {
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
    case Icode_NAME_AND_THIS : {
        String name = strings[getIndex(iCode, pc + 1)];
        Scriptable base = ScriptRuntime.getBase(scope, name);
        stack[++stackTop] = ScriptRuntime.getProp(base, name, scope);
        stack[++stackTop] = ScriptRuntime.getThis(base);
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
        stack[++stackTop] = ScriptRuntime.postIncrDecr(scope, name, true);
        pc += 2;
        break;
    }
    case Icode_NAMEDEC : {
        String name = strings[getIndex(iCode, pc + 1)];
        stack[++stackTop] = ScriptRuntime.postIncrDecr(scope, name, false);
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
    case Token.CATCH_SCOPE : {
        String name = strings[getIndex(iCode, pc + 1)];
        stack[stackTop] = ScriptRuntime.newCatchScope(name, stack[stackTop]);
        pc += 2;
        break;
    }
    case Token.ENUM_INIT : {
        int slot = (iCode[++pc] & 0xFF);
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        stack[LOCAL_SHFT + slot] = ScriptRuntime.enumInit(lhs, scope);
        break;
    }
    case Token.ENUM_NEXT :
    case Token.ENUM_ID : {
        int slot = (iCode[++pc] & 0xFF);
        Object val = stack[LOCAL_SHFT + slot];
        ++stackTop;
        stack[stackTop] = ((iCode[pc - 1] & 0xFF) == Token.ENUM_NEXT)
                          ? (Object)ScriptRuntime.enumNext(val)
                          : (Object)ScriptRuntime.enumId(val);
        break;
    }
    case Icode_GETPROTO : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.getProto(lhs, scope);
        break;
    }
    case Icode_PUSH_PARENT : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[++stackTop] = ScriptRuntime.getParent(lhs);
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
            ("Unknown icode : "+(iCode[pc] & 0xFF)+" @ pc : "+pc);
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

    private static int stack_int32(Object[] stack, double[] stackDbl, int i)
    {
        Object x = stack[i];
        double value;
        if (x == DBL_MRK) {
            value = stackDbl[i];
        } else {
            value = ScriptRuntime.toNumber(x);
        }
        return ScriptRuntime.toInt32(value);
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
                result = ScriptRuntime.eqNumber(stackDbl[stackTop + 1], lhs);
            }
        } else {
            if (lhs == DBL_MRK) {
                result = ScriptRuntime.eqNumber(stackDbl[stackTop], rhs);
            } else {
                result = ScriptRuntime.eq(lhs, rhs);
            }
        }
        return result;
    }

    private static boolean do_sheq(Object[] stack, double[] stackDbl,
                                   int stackTop)
    {
        Object rhs = stack[stackTop + 1];
        Object lhs = stack[stackTop];
        double rdbl, ldbl;
        if (rhs == DBL_MRK) {
            rdbl = stackDbl[stackTop + 1];
            if (lhs == DBL_MRK) {
                ldbl = stackDbl[stackTop];
            } else if (lhs instanceof Number) {
                ldbl = ((Number)lhs).doubleValue();
            } else {
                return false;
            }
        } else if (lhs == DBL_MRK) {
            ldbl = stackDbl[stackTop];
            if (rhs == DBL_MRK) {
                rdbl = stackDbl[stackTop + 1];
            } else if (rhs instanceof Number) {
                rdbl = ((Number)rhs).doubleValue();
            } else {
                return false;
            }
        } else {
            return ScriptRuntime.shallowEq(lhs, rhs);
        }
        return ldbl == rdbl;
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

    private static int getJavaCatchPC(byte[] iCode)
    {
        int pc = iCode.length - 1;
        if ((iCode[pc] & 0xFF) != Icode_CATCH) Kit.codeBug();
        return pc;
    }

    private CompilerEnvirons compilerEnv;

    private boolean itsInFunctionFlag;

    private InterpreterData itsData;
    private ScriptOrFnNode scriptOrFn;
    private int itsStackDepth = 0;
    private int itsWithDepth = 0;
    private int itsLineNumber = 0;
    private int itsDoubleTableTop;
    private ObjToIntMap itsStrings = new ObjToIntMap(20);
    private String lastAddString;
    private int itsLocalTop;

    private static final int MIN_LABEL_TABLE_SIZE = 32;
    private static final int MIN_FIXUP_TABLE_SIZE = 40;
    private int[] itsLabelTable;
    private int itsLabelTableTop;
// itsFixupTable[i] = (label_index << 32) | fixup_site
    private long[] itsFixupTable;
    private int itsFixupTableTop;

    private int itsExceptionTableTop;
    // 5 = space for try start/end, catch begin, finally begin and with depth
    private static final int EXCEPTION_SLOT_SIZE       = 6;
    private static final int EXCEPTION_TRY_START_SLOT  = 0;
    private static final int EXCEPTION_TRY_END_SLOT    = 1;
    private static final int EXCEPTION_CATCH_SLOT      = 2;
    private static final int EXCEPTION_FINALLY_SLOT    = 3;
    private static final int EXCEPTION_WITH_DEPTH_SLOT = 4;
    private static final int EXCEPTION_LOCAL_SLOT      = 5;

    private static final Object DBL_MRK = new Object();
}
