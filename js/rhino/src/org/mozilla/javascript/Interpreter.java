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
 * Ethan Hugg
 * Terry Lucas
 * Roger Lawrence
 * Milen Nankov
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

    private static final int

    // Stack: ... value1 -> ... value1 value1
        Icode_DUP                       = -1,

    // Stack: ... value2 value1 -> ... value2 value1 value2 value1
        Icode_DUP2                      = -2,

    // Stack: ... value2 value1 -> ... value1 value2
        Icode_SWAP                      = -3,

    // Stack: ... value1 -> ...
        Icode_POP                       = -4,

    // Store stack top into return register and then pop it
        Icode_POP_RESULT                = -5,

    // To jump conditionally and pop additional stack value
        Icode_IFEQ_POP                  = -6,

    // various types of ++/--
        Icode_VAR_INC_DEC               = -7,
        Icode_NAME_INC_DEC              = -8,
        Icode_PROP_INC_DEC              = -9,
        Icode_ELEM_INC_DEC              = -10,
        Icode_REF_INC_DEC               = -11,

    // helper codes to deal with activation
        Icode_SCOPE                     = -12,
        Icode_TYPEOFNAME                = -13,

    // helper for function calls
        Icode_NAME_AND_THIS             = -14,
        Icode_PROP_AND_THIS             = -15,
        Icode_ELEM_AND_THIS             = -16,
        Icode_VALUE_AND_THIS            = -17,

    // Create closure object for nested functions
        Icode_CLOSURE_EXPR              = -18,
        Icode_CLOSURE_STMT              = -19,

    // Special calls
        Icode_CALLSPECIAL               = -20,

    // To return undefined value
        Icode_RETUNDEF                  = -21,

    // Exception handling implementation
        Icode_CATCH                     = -22,
        Icode_GOSUB                     = -23,
        Icode_RETSUB                    = -24,

    // To indicating a line number change in icodes.
        Icode_LINE                      = -25,

    // To store shorts and ints inline
        Icode_SHORTNUMBER               = -26,
        Icode_INTNUMBER                 = -27,

    // To create and populate array to hold values for [] and {} literals
        Icode_LITERAL_NEW               = -28,
        Icode_LITERAL_SET               = -29,

    // Array literal with skipped index like [1,,2]
        Icode_SPARE_ARRAYLIT            = -30,

    // Load index register to prepare for the following index operation
        Icode_REG_IND_C0                = -31,
        Icode_REG_IND_C1                = -32,
        Icode_REG_IND_C2                = -33,
        Icode_REG_IND_C3                = -34,
        Icode_REG_IND_C4                = -35,
        Icode_REG_IND_C5                = -36,
        Icode_REG_IND1                  = -37,
        Icode_REG_IND2                  = -38,
        Icode_REG_IND4                  = -39,

    // Load string register to prepare for the following string operation
        Icode_REG_STR_C0                = -40,
        Icode_REG_STR_C1                = -41,
        Icode_REG_STR_C2                = -42,
        Icode_REG_STR_C3                = -43,
        Icode_REG_STR1                  = -44,
        Icode_REG_STR2                  = -45,
        Icode_REG_STR4                  = -46,

    // Version of getvar/setvar that read var index directly from bytecode
        Icode_GETVAR1                   = -47,
        Icode_SETVAR1                   = -48,

    // Load unefined
        Icode_UNDEF                     = -49,
        Icode_ZERO                      = -50,
        Icode_ONE                       = -51,

    // entrance and exit from .()
       Icode_ENTERDQ                    = -52,
       Icode_LEAVEDQ                    = -53,

    // Last icode
        MIN_ICODE                       = -53;

    static {
        // Checks for byte code consistencies, good compiler can eliminate them

        if (Token.LAST_BYTECODE_TOKEN > 127) {
            String str = "Violation of Token.LAST_BYTECODE_TOKEN <= 127";
            System.err.println(str);
            throw new IllegalStateException(str);
        }
        if (MIN_ICODE < -128) {
            String str = "Violation of Interpreter.MIN_ICODE >= -128";
            System.err.println(str);
            throw new IllegalStateException(str);
        }
    }

    private static String bytecodeName(int bytecode)
    {
        if (!validBytecode(bytecode)) {
            throw new IllegalArgumentException(String.valueOf(bytecode));
        }

        if (!Token.printICode) {
            return String.valueOf(bytecode);
        }

        if (validTokenCode(bytecode)) {
            return Token.name(bytecode);
        }

        switch (bytecode) {
          case Icode_DUP:              return "DUP";
          case Icode_DUP2:             return "DUP2";
          case Icode_SWAP:             return "SWAP";
          case Icode_POP:              return "POP";
          case Icode_POP_RESULT:       return "POP_RESULT";
          case Icode_IFEQ_POP:         return "IFEQ_POP";
          case Icode_VAR_INC_DEC:      return "VAR_INC_DEC";
          case Icode_NAME_INC_DEC:     return "NAME_INC_DEC";
          case Icode_PROP_INC_DEC:     return "PROP_INC_DEC";
          case Icode_ELEM_INC_DEC:     return "ELEM_INC_DEC";
          case Icode_REF_INC_DEC:      return "REF_INC_DEC";
          case Icode_SCOPE:            return "SCOPE";
          case Icode_TYPEOFNAME:       return "TYPEOFNAME";
          case Icode_NAME_AND_THIS:    return "NAME_AND_THIS";
          case Icode_PROP_AND_THIS:    return "PROP_AND_THIS";
          case Icode_ELEM_AND_THIS:    return "ELEM_AND_THIS";
          case Icode_VALUE_AND_THIS:   return "VALUE_AND_THIS";
          case Icode_CLOSURE_EXPR:     return "CLOSURE_EXPR";
          case Icode_CLOSURE_STMT:     return "CLOSURE_STMT";
          case Icode_CALLSPECIAL:      return "CALLSPECIAL";
          case Icode_RETUNDEF:         return "RETUNDEF";
          case Icode_CATCH:            return "CATCH";
          case Icode_GOSUB:            return "GOSUB";
          case Icode_RETSUB:           return "RETSUB";
          case Icode_LINE:             return "LINE";
          case Icode_SHORTNUMBER:      return "SHORTNUMBER";
          case Icode_INTNUMBER:        return "INTNUMBER";
          case Icode_LITERAL_NEW:      return "LITERAL_NEW";
          case Icode_LITERAL_SET:      return "LITERAL_SET";
          case Icode_SPARE_ARRAYLIT:   return "SPARE_ARRAYLIT";
          case Icode_REG_IND_C0:       return "REG_IND_C0";
          case Icode_REG_IND_C1:       return "REG_IND_C1";
          case Icode_REG_IND_C2:       return "REG_IND_C2";
          case Icode_REG_IND_C3:       return "REG_IND_C3";
          case Icode_REG_IND_C4:       return "REG_IND_C4";
          case Icode_REG_IND_C5:       return "REG_IND_C5";
          case Icode_REG_IND1:         return "LOAD_IND1";
          case Icode_REG_IND2:         return "LOAD_IND2";
          case Icode_REG_IND4:         return "LOAD_IND4";
          case Icode_REG_STR_C0:       return "REG_STR_C0";
          case Icode_REG_STR_C1:       return "REG_STR_C1";
          case Icode_REG_STR_C2:       return "REG_STR_C2";
          case Icode_REG_STR_C3:       return "REG_STR_C3";
          case Icode_REG_STR1:         return "LOAD_STR1";
          case Icode_REG_STR2:         return "LOAD_STR2";
          case Icode_REG_STR4:         return "LOAD_STR4";
          case Icode_GETVAR1:          return "GETVAR1";
          case Icode_SETVAR1:          return "SETVAR1";
          case Icode_UNDEF:            return "UNDEF";
          case Icode_ZERO:             return "ZERO";
          case Icode_ONE:              return "ONE";
          case Icode_ENTERDQ:          return "ENTERDQ";
          case Icode_LEAVEDQ:          return "LEAVEDQ";
        }

        // icode without name
        throw new IllegalStateException(String.valueOf(bytecode));
    }

    private static boolean validIcode(int icode)
    {
        return MIN_ICODE <= icode && icode <= -1;
    }

    private static boolean validTokenCode(int token)
    {
        return Token.FIRST_BYTECODE_TOKEN <= token
               && token <= Token.LAST_BYTECODE_TOKEN;
    }

    private static boolean validBytecode(int bytecode)
    {
        return validIcode(bytecode) || validTokenCode(bytecode);
    }

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

        if (returnFunction) {
            generateFunctionICode();
            return createFunction(cx, scope, itsData, false);
        } else {
            itsData.itsFromEvalCode = compilerEnv.isFromEval();
            generateICodeFromTree(scriptOrFn);
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
        itsInFunctionFlag = true;

        FunctionNode theFunction = (FunctionNode)scriptOrFn;

        itsData.itsFunctionType = theFunction.getFunctionType();
        itsData.itsNeedsActivation = theFunction.requiresActivation();
        itsData.itsName = theFunction.getFunctionName();
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

        visitStatement(tree);
        fixLabelGotos();
        // add RETURN_RESULT only to scripts as function always ends with RETURN
        if (itsData.itsFunctionType == 0) {
            addToken(Token.RETURN_RESULT);
        }
        // Add special CATCH to simplify Interpreter.interpret logic
        // and workaround lack of goto in Java
        addIcode(Icode_CATCH);

        if (itsData.itsICode.length != itsICodeTop) {
            // Make itsData.itsICode length exactly itsICodeTop to save memory
            // and catch bugs with jumps beyound icode as early as possible
            byte[] tmp = new byte[itsICodeTop];
            System.arraycopy(itsData.itsICode, 0, tmp, 0, itsICodeTop);
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

        if (itsLiteralIds.size() != 0) {
            itsData.literalIds = itsLiteralIds.toArray();
        }

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

    private void updateLineNumber(Node node)
    {
        int lineno = node.getLineno();
        if (lineno != itsLineNumber && lineno >= 0) {
            if (itsData.firstLinePC < 0) {
                itsData.firstLinePC = lineno;
            }
            itsLineNumber = lineno;
            addIcode(Icode_LINE);
            addUint16(lineno);
        }
    }

    private RuntimeException badTree(Node node)
    {
        throw new RuntimeException(node.toString());
    }

    private void visitStatement(Node node)
    {
        int type = node.getType();
        Node child = node.getFirstChild();
        switch (type) {

          case Token.FUNCTION: {
            int fnIndex = node.getExistingIntProp(Node.FUNCTION_PROP);
            int fnType = scriptOrFn.getFunctionNode(fnIndex).getFunctionType();
            // Only function expressions or function expression
            // statements needs closure code creating new function
            // object on stack as function statements are initialized
            // at script/function start
            // In addition function expression can not present here
            // at statement level, they must only present as expressions.
            if (fnType == FunctionNode.FUNCTION_EXPRESSION_STATEMENT) {
                addIndexOp(Icode_CLOSURE_STMT, fnIndex);
            } else {
                if (fnType != FunctionNode.FUNCTION_STATEMENT) {
                    throw Kit.codeBug();
                }
            }
            break;
          }

          case Token.CASE:
            // Skip case condition
            child = child.getNext();
            // fallthrough
          case Token.DEFAULT:
          case Token.SCRIPT:
          case Token.LABEL:
          case Token.LOOP:
          case Token.BLOCK:
          case Token.EMPTY:
            updateLineNumber(node);
            while (child != null) {
                visitStatement(child);
                child = child.getNext();
            }
            break;

          case Token.WITH:
            ++itsWithDepth;
            updateLineNumber(node);
            while (child != null) {
                visitStatement(child);
                child = child.getNext();
            }
            --itsWithDepth;
            break;

          case Token.ENTERWITH:
            visitExpression(child);
            addToken(Token.ENTERWITH);
            stackChange(-1);
            break;

          case Token.LEAVEWITH:
            addToken(Token.LEAVEWITH);
            break;

          case Token.LOCAL_BLOCK:
            node.putIntProp(Node.LOCAL_PROP, itsLocalTop);
            ++itsLocalTop;
            if (itsLocalTop > itsData.itsMaxLocals) {
                itsData.itsMaxLocals = itsLocalTop;
            }
            updateLineNumber(node);
            while (child != null) {
                visitStatement(child);
                child = child.getNext();
            }
            --itsLocalTop;
            break;

          case Token.SWITCH:
            visitSwitch((Node.Jump)node);
            break;

          case Token.TARGET:
            markTargetLabel((Node.Target)node);
            break;

          case Token.IFEQ :
          case Token.IFNE : {
            Node.Target target = ((Node.Jump)node).target;
            visitExpression(child);
            addGoto(target, type);
            stackChange(-1);
            break;
          }

          case Token.GOTO: {
            Node.Target target = ((Node.Jump)node).target;
            addGoto(target, type);
            break;
          }

          case Token.JSR: {
            Node.Target target = ((Node.Jump)node).target;
            addGoto(target, Icode_GOSUB);
            break;
          }

          case Token.FINALLY: {
            // Account for incomming exception or GOTOSUB address
            stackChange(1);
            int finallyRegister = getLocalBlockRef(node);
            addIndexOp(Token.LOCAL_SAVE, finallyRegister);
            stackChange(-1);
            while (child != null) {
                visitStatement(child);
                child = child.getNext();
            }
            addIndexOp(Icode_RETSUB, finallyRegister);
            break;
          }

          case Token.EXPR_VOID:
          case Token.EXPR_RESULT:
            updateLineNumber(node);
            visitExpression(child);
            addIcode((type == Token.EXPR_VOID) ? Icode_POP : Icode_POP_RESULT);
            stackChange(-1);
            break;

          case Token.TRY: {
            Node.Jump tryNode = (Node.Jump)node;
            int exceptionObjectLocal = getLocalBlockRef(tryNode);
            Node catchTarget = tryNode.target;
            Node finallyTarget = tryNode.getFinally();

            int tryStart = itsICodeTop;
            int tryEnd = -1;
            int catchStart = -1;
            int finallyStart = -1;

            while (child != null) {
                boolean generated = false;

                if (child == catchTarget) {
                    if (tryEnd >= 0) Kit.codeBug();
                    tryEnd = itsICodeTop;
                    catchStart = itsICodeTop;

                    markTargetLabel((Node.Target)child);
                    generated = true;

                } else if (child == finallyTarget) {
                    if (tryEnd < 0) {
                        tryEnd = itsICodeTop;
                    }
                    finallyStart = itsICodeTop;

                    markTargetLabel((Node.Target)child);
                    generated = true;
                }

                if (!generated) {
                    visitStatement(child);
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

          case Token.THROW:
            updateLineNumber(node);
            visitExpression(child);
            addToken(Token.THROW);
            addUint16(itsLineNumber);
            stackChange(-1);
            break;

          case Token.RETURN:
            updateLineNumber(node);
            if (child != null) {
                visitExpression(child);
                addToken(Token.RETURN);
                stackChange(-1);
            } else {
                addIcode(Icode_RETUNDEF);
            }
            break;

          case Token.RETURN_RESULT:
            updateLineNumber(node);
            addToken(Token.RETURN_RESULT);
            break;

          case Token.ENUM_INIT_KEYS:
          case Token.ENUM_INIT_VALUES :
            visitExpression(child);
            addIndexOp(type, getLocalBlockRef(node));
            stackChange(-1);
            break;

          default:
            throw badTree(node);
        }

        if (itsStackDepth != 0) {
            throw Kit.codeBug();
        }
    }

    private void visitExpression(Node node)
    {
        int type = node.getType();
        Node child = node.getFirstChild();
        int savedStackDepth = itsStackDepth;
        switch (type) {

          case Token.FUNCTION: {
            int fnIndex = node.getExistingIntProp(Node.FUNCTION_PROP);
            FunctionNode fn = scriptOrFn.getFunctionNode(fnIndex);
            // See comments in visitStatement for Token.FUNCTION case
            if (fn.getFunctionType() != FunctionNode.FUNCTION_EXPRESSION) {
                throw Kit.codeBug();
            }
            addIndexOp(Icode_CLOSURE_EXPR, fnIndex);
            stackChange(1);
            break;
          }

          case Token.LOCAL_LOAD: {
            int localIndex = getLocalBlockRef(node);
            addIndexOp(Token.LOCAL_LOAD, localIndex);
            stackChange(1);
            break;
          }

          case Token.COMMA:
            visitExpression(child);
            while (null != (child = child.getNext())) {
                if (1 != itsStackDepth - savedStackDepth) Kit.codeBug();
                addIcode(Icode_POP);
                stackChange(-1);
                visitExpression(child);
            }
            break;

          case Token.USE_STACK:
            // Indicates that stack was modified externally,
            // like placed catch object
            stackChange(1);
            break;

          case Token.CALL:
          case Token.NEW:
          case Token.REF_CALL:
            visitCall(node);
            break;

          case Token.AND:
          case Token.OR: {
            visitExpression(child);
            addIcode(Icode_DUP);
            stackChange(1);
            int afterSecondJumpStart = itsICodeTop;
            int jump = (type == Token.AND) ? Token.IFNE : Token.IFEQ;
            addForwardGoto(jump);
            stackChange(-1);
            addIcode(Icode_POP);
            stackChange(-1);
            child = child.getNext();
            visitExpression(child);
            resolveForwardGoto(afterSecondJumpStart);
            break;
          }

          case Token.HOOK: {
            Node ifThen = child.getNext();
            Node ifElse = ifThen.getNext();
            visitExpression(child);
            int elseJumpStart = itsICodeTop;
            addForwardGoto(Token.IFNE);
            stackChange(-1);
            visitExpression(ifThen);
            int afterElseJumpStart = itsICodeTop;
            addForwardGoto(Token.GOTO);
            resolveForwardGoto(elseJumpStart);
            itsStackDepth = savedStackDepth;
            visitExpression(ifElse);
            resolveForwardGoto(afterElseJumpStart);
            break;
          }

          case Token.GETPROP:
            visitGetProp(node, child);
            break;

          case Token.GETELEM:
            visitGetElem(node, child);
            break;

          case Token.GET_REF:
            visitGetRef(node, child);
            break;

          case Token.DELPROP:
          case Token.BITAND:
          case Token.BITOR:
          case Token.BITXOR:
          case Token.LSH:
          case Token.RSH:
          case Token.URSH:
          case Token.ADD:
          case Token.SUB:
          case Token.MOD:
          case Token.DIV:
          case Token.MUL:
          case Token.EQ:
          case Token.NE:
          case Token.SHEQ:
          case Token.SHNE:
          case Token.IN:
          case Token.INSTANCEOF:
          case Token.LE:
          case Token.LT:
          case Token.GE:
          case Token.GT:
            visitExpression(child);
            child = child.getNext();
            visitExpression(child);
            addToken(type);
            stackChange(-1);
            break;

          case Token.POS:
          case Token.NEG:
          case Token.NOT:
          case Token.BITNOT:
          case Token.TYPEOF:
          case Token.VOID:
          case Token.DEL_REF:
            visitExpression(child);
            if (type == Token.VOID) {
                addIcode(Icode_POP);
                addIcode(Icode_UNDEF);
            } else {
                addToken(type);
            }
            break;

        case Token.SETPROP:
          case Token.SETPROP_OP: {
            visitExpression(child);
            child = child.getNext();
            String property = child.getString();
            child = child.getNext();
            if (type == Token.SETPROP_OP) {
                addIcode(Icode_DUP);
                stackChange(1);
                addStringOp(Token.GETPROP, property);
                // Compensate for the following USE_STACK
                stackChange(-1);
            }
            visitExpression(child);
            addStringOp(Token.SETPROP, property);
            stackChange(-1);
            break;
          }

          case Token.SETELEM:
          case Token.SETELEM_OP:
            visitExpression(child);
            child = child.getNext();
            visitExpression(child);
            child = child.getNext();
            if (type == Token.SETELEM_OP) {
                addIcode(Icode_DUP2);
                stackChange(2);
                addToken(Token.GETELEM);
                stackChange(-1);
                // Compensate for the following USE_STACK
                stackChange(-1);
            }
            visitExpression(child);
            addToken(Token.SETELEM);
            stackChange(-2);
            break;

          case Token.SET_REF:
          case Token.SET_REF_OP:
            visitExpression(child);
            child = child.getNext();
            if (type == Token.SET_REF_OP) {
                addIcode(Icode_DUP);
                stackChange(1);
                addToken(Token.GET_REF);
                // Compensate for the following USE_STACK
                stackChange(-1);
            }
            visitExpression(child);
            addToken(Token.SET_REF);
            stackChange(-1);
            break;

          case Token.SETNAME: {
            String name = child.getString();
            visitExpression(child);
            child = child.getNext();
            visitExpression(child);
            addStringOp(Token.SETNAME, name);
            stackChange(-1);
            break;
          }

          case Token.TYPEOFNAME: {
            String name = node.getString();
            int index = -1;
            // use typeofname if an activation frame exists
            // since the vars all exist there instead of in jregs
            if (itsInFunctionFlag && !itsData.itsNeedsActivation)
                index = scriptOrFn.getParamOrVarIndex(name);
            if (index == -1) {
                addStringOp(Icode_TYPEOFNAME, name);
                stackChange(1);
            } else {
                addVarOp(Token.GETVAR, index);
                stackChange(1);
                addToken(Token.TYPEOF);
            }
            break;
          }

          case Token.BINDNAME:
          case Token.NAME:
          case Token.STRING:
            addStringOp(type, node.getString());
            stackChange(1);
            break;

          case Token.INC:
          case Token.DEC:
            visitIncDec(node, child);
            break;

          case Token.NUMBER:
            visitNumber(node);
            break;

          case Token.CATCH_SCOPE:
            visitExpression(child);
            addStringOp(Token.CATCH_SCOPE, node.getString());
            break;

          case Token.GETVAR: {
            String name = node.getString();
            if (itsData.itsNeedsActivation) {
                // SETVAR handled this by turning into a SETPROP, but
                // we can't do that to a GETVAR without manufacturing
                // bogus children. Instead we use a special op to
                // push the current scope.
                addIcode(Icode_SCOPE);
                stackChange(1);
                addStringOp(Token.GETPROP, name);
            } else {
                int index = scriptOrFn.getParamOrVarIndex(name);
                addVarOp(Token.GETVAR, index);
                stackChange(1);
            }
            break;
          }

          case Token.SETVAR: {
            if (itsData.itsNeedsActivation) {
                child.setType(Token.BINDNAME);
                node.setType(Token.SETNAME);
                visitExpression(node);
            } else {
                String name = child.getString();
                child = child.getNext();
                visitExpression(child);
                int index = scriptOrFn.getParamOrVarIndex(name);
                addVarOp(Token.SETVAR, index);
            }
            break;
          }

          case Token.NULL:
          case Token.THIS:
          case Token.THISFN:
          case Token.FALSE:
          case Token.TRUE:
            addToken(type);
            stackChange(1);
            break;

          case Token.ENUM_NEXT:
          case Token.ENUM_ID:
            addIndexOp(type, getLocalBlockRef(node));
            stackChange(1);
            break;

          case Token.REGEXP: {
            int index = node.getExistingIntProp(Node.REGEXP_PROP);
            addIndexOp(Token.REGEXP, index);
            stackChange(1);
            break;
          }

          case Token.ARRAYLIT:
          case Token.OBJECTLIT:
            visitLiteral(node, child);
            break;

          case Token.SPECIAL_REF: {
            visitExpression(child);
            String special = (String)node.getProp(Node.SPECIAL_PROP_PROP);
            addStringOp(Token.SPECIAL_REF, special);
            break;
          }

          case Token.XML_REF:
          case Token.GENERIC_REF:
            visitExpression(child);
            addToken(type);
            break;

          case Token.DOTQUERY:
            visitDotQery(node, child);
            break;

          case Token.DEFAULTNAMESPACE :
          case Token.ESCXMLATTR :
          case Token.ESCXMLTEXT :
          case Token.TOATTRNAME :
          case Token.DESCENDANTS :
            visitExpression(child);
            addToken(type);
            break;

          case Token.COLONCOLON : {
            if (child.getType() != Token.STRING)
                throw badTree(child);
            String namespace = child.getString();
            child = child.getNext();
            visitExpression(child);
            addStringOp(Token.COLONCOLON, namespace);
            break;
          }


          default:
            throw badTree(node);
        }
        if (savedStackDepth + 1 != itsStackDepth) {
            Kit.codeBug();
        }
    }

    private void visitSwitch(Node.Jump switchNode)
    {
        Node child = switchNode.getFirstChild();

        updateLineNumber(switchNode);
        visitExpression(child);

        ObjArray cases = (ObjArray) switchNode.getProp(Node.CASES_PROP);
        for (int i = 0; i < cases.size(); i++) {
            Node thisCase = (Node)cases.get(i);
            Node test = thisCase.getFirstChild();
            // the case expression is the firstmost child
            // the rest will be generated when the case
            // statements are encountered as siblings of
            // the switch statement.
            addIcode(Icode_DUP);
            stackChange(1);
            visitExpression(test);
            addToken(Token.SHEQ);
            stackChange(-1);
            Node.Target target = new Node.Target();
            thisCase.addChildAfter(target, test);
            // If true, Icode_IFEQ_POP will jump and remove case value
            // from stack
            addGoto(target, Icode_IFEQ_POP);
            stackChange(-1);
        }
        addIcode(Icode_POP);
        stackChange(-1);

        Node defaultNode = (Node) switchNode.getProp(Node.DEFAULT_PROP);
        if (defaultNode != null) {
            Node.Target defaultTarget = new Node.Target();
            defaultNode.getFirstChild().addChildToFront(defaultTarget);
            addGoto(defaultTarget, Token.GOTO);
        }

        Node.Target breakTarget = switchNode.target;
        addGoto(breakTarget, Token.GOTO);
    }

    private void visitCall(Node node)
    {
        int type = node.getType();
        Node child = node.getFirstChild();
        if (type == Token.NEW) {
            visitExpression(child);
        } else {
            generateCallFunAndThis(child);
        }
        // To get better debugging output for undefined or null calls.
        int debugNameIndex = itsLastStringIndex;
        int argCount = 0;
        while ((child = child.getNext()) != null) {
            visitExpression(child);
            ++argCount;
        }
        int callType = node.getIntProp(Node.SPECIALCALL_PROP,
                                       Node.NON_SPECIALCALL);
        if (callType != Node.NON_SPECIALCALL) {
            // embed line number and source filename
            addIndexOp(Icode_CALLSPECIAL, argCount);
            addUint8(callType);
            addUint8(type == Token.NEW ? 1 : 0);
            addUint16(itsLineNumber);
        } else {
            addIndexOp(type, argCount);
            if (type == Token.NEW) {
                // Code for generateCallFunAndThis takes care about debugging
                // for Token.CALL abd Token.REF_CALL;
                if (debugNameIndex < 0xFFFF) {
                    // Use only 2 bytes to store debug index
                    addUint16(debugNameIndex);
                } else {
                    addUint16(0xFFFF);
                }
            }
        }
        // adjust stack
        if (type == Token.NEW) {
            // f, args -> results
           stackChange(-argCount);
        } else {
            // f, thisObj, args -> results
           stackChange(-1 - argCount);
        }
        if (argCount > itsData.itsMaxCalleeArgs)
            itsData.itsMaxCalleeArgs = argCount;

    }

    private void generateCallFunAndThis(Node left)
    {
        // Generate code to place on stack function and thisObj
        int type = left.getType();
        switch (type) {
          case Token.NAME: {
            String name = left.getString();
            // stack: ... -> ... function thisObj
            addStringOp(Icode_NAME_AND_THIS, name);
            stackChange(2);
            break;
          }
          case Token.GETPROP:
          case Token.GETELEM: {
            Node target = left.getFirstChild();
            visitExpression(target);
            Node id = target.getNext();
            if (type == Token.GETPROP) {
                String property = id.getString();
                // stack: ... target -> ... function thisObj
                addStringOp(Icode_PROP_AND_THIS, property);
                stackChange(1);
            } else {
                visitExpression(id);
                // stack: ... target id -> ... function thisObj
                addIcode(Icode_ELEM_AND_THIS);
            }
            break;
          }
          default:
            // Including Token.GETVAR
            visitExpression(left);
            // stack: ... value -> ... function thisObj
            addIcode(Icode_VALUE_AND_THIS);
            stackChange(1);
            break;
        }
    }

    private void visitGetProp(Node node, Node child)
    {
        visitExpression(child);
        child = child.getNext();
        String property = child.getString();
        addStringOp(Token.GETPROP, property);
    }

    private void visitGetElem(Node node, Node child)
    {
        visitExpression(child);
        child = child.getNext();
        visitExpression(child);
        addToken(Token.GETELEM);
        stackChange(-1);
    }

    private void visitGetRef(Node node, Node child)
    {
        visitExpression(child);
        addToken(Token.GET_REF);
    }

    private void visitIncDec(Node node, Node child)
    {
        int incrDecrMask = node.getExistingIntProp(Node.INCRDECR_PROP);
        int childType = child.getType();
        switch (childType) {
          case Token.GETVAR : {
            String name = child.getString();
            if (itsData.itsNeedsActivation) {
                addIcode(Icode_SCOPE);
                stackChange(1);
                addStringOp(Icode_PROP_INC_DEC, name);
                addUint8(incrDecrMask);
            } else {
                int i = scriptOrFn.getParamOrVarIndex(name);
                addVarOp(Icode_VAR_INC_DEC, i);
                addUint8(incrDecrMask);
                stackChange(1);
            }
            break;
          }
          case Token.NAME : {
            String name = child.getString();
            addStringOp(Icode_NAME_INC_DEC, name);
            addUint8(incrDecrMask);
            stackChange(1);
            break;
          }
          case Token.GETPROP : {
            Node object = child.getFirstChild();
            visitExpression(object);
            String property = object.getNext().getString();
            addStringOp(Icode_PROP_INC_DEC, property);
            addUint8(incrDecrMask);
            break;
          }
          case Token.GETELEM : {
            Node object = child.getFirstChild();
            visitExpression(object);
            Node index = object.getNext();
            visitExpression(index);
            addIcode(Icode_ELEM_INC_DEC);
            addUint8(incrDecrMask);
            stackChange(-1);
            break;
          }
          case Token.GET_REF : {
            Node ref = child.getFirstChild();
            visitExpression(ref);
            addIcode(Icode_REF_INC_DEC);
            addUint8(incrDecrMask);
            break;
          }
          default : {
            throw badTree(node);
          }
        }
    }

    private void visitNumber(Node node)
    {
        double num = node.getDouble();
        int inum = (int)num;
        if (inum == num) {
            if (inum == 0) {
                addIcode(Icode_ZERO);
                // Check for negative zero
                if (1.0 / num < 0.0) {
                    addToken(Token.NEG);
                }
            } else if (inum == 1) {
                addIcode(Icode_ONE);
            } else if ((short)inum == inum) {
                addIcode(Icode_SHORTNUMBER);
                // write short as uin16 bit pattern
                addUint16(inum & 0xFFFF);
            } else {
                addIcode(Icode_INTNUMBER);
                addInt(inum);
            }
        } else {
            int index = getDoubleIndex(num);
            addIndexOp(Token.NUMBER, index);
        }
        stackChange(1);
    }

    private void visitLiteral(Node node, Node child)
    {
        int type = node.getType();
        int count;
        Object[] propertyIds = null;
        if (type == Token.ARRAYLIT) {
            count = 0;
            for (Node n = child; n != null; n = n.getNext()) {
                ++count;
            }
        } else if (type == Token.OBJECTLIT) {
            propertyIds = (Object[])node.getProp(Node.OBJECT_IDS_PROP);
            count = propertyIds.length;
        } else {
            throw badTree(node);
        }
        addIndexOp(Icode_LITERAL_NEW, count);
        stackChange(1);
        while (child != null) {
            visitExpression(child);
            addIcode(Icode_LITERAL_SET);
            stackChange(-1);
            child = child.getNext();
        }
        if (type == Token.ARRAYLIT) {
            int[] skipIndexes = (int[])node.getProp(Node.SKIP_INDEXES_PROP);
            if (skipIndexes == null) {
                addToken(Token.ARRAYLIT);
            } else {
                int index = itsLiteralIds.size();
                itsLiteralIds.add(skipIndexes);
                addIndexOp(Icode_SPARE_ARRAYLIT, index);
            }
        } else {
            int index = itsLiteralIds.size();
            itsLiteralIds.add(propertyIds);
            addIndexOp(Token.OBJECTLIT, index);
        }
    }

    private void visitDotQery(Node node, Node child)
    {
        updateLineNumber(node);
        visitExpression(child);
        addIcode(Icode_ENTERDQ);
        stackChange(-1);
        int queryPC = itsICodeTop;
        visitExpression(child.getNext());
        addBackwardGoto(Icode_LEAVEDQ, queryPC);
    }

    private int getLocalBlockRef(Node node)
    {
        Node localBlock = (Node)node.getProp(Node.LOCAL_BLOCK_PROP);
        return localBlock.getExistingIntProp(Node.LOCAL_PROP);
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

    private void markTargetLabel(Node.Target target)
    {
        int label = getTargetLabel(target);
        if (itsLabelTable[label] != -1) {
            // Can mark label only once
            Kit.codeBug();
        }
        itsLabelTable[label] = itsICodeTop;
    }

    private void addGoto(Node.Target target, int gotoOp)
    {
        int label = getTargetLabel(target);
        if (!(label < itsLabelTableTop)) Kit.codeBug();
        int targetPC = itsLabelTable[label];

        int gotoPC = itsICodeTop;
        if (validIcode(gotoOp)) {
            addIcode(gotoOp);
        } else {
            addToken(gotoOp);
        }

        if (targetPC != -1) {
            recordJump(gotoPC, targetPC);
            itsICodeTop += 2;
        } else {
            addUint16(0);
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
            itsFixupTable[top] = ((long)label << 32) | gotoPC;
        }
    }

    private void fixLabelGotos()
    {
        for (int i = 0; i < itsFixupTableTop; i++) {
            long fixup = itsFixupTable[i];
            int label = (int)(fixup >> 32);
            int jumpSource = (int)fixup;
            int pc = itsLabelTable[label];
            if (pc == -1) {
                // Unlocated label
                throw Kit.codeBug();
            }
            recordJump(jumpSource, pc);
        }
        itsFixupTableTop = 0;
    }

    private void addBackwardGoto(int gotoOp, int jumpPC)
    {
        if (jumpPC >= itsICodeTop) throw Kit.codeBug();
        int fromPC = itsICodeTop;
        addIcode(gotoOp);
        recordJump(fromPC, jumpPC);
        itsICodeTop += 2;
    }

    private void addForwardGoto(int gotoOp)
    {
        addToken(gotoOp);
        addUint16(0);
    }

    private void resolveForwardGoto(int fromPC)
    {
        if (fromPC + 3 > itsICodeTop) throw Kit.codeBug();
        recordJump(fromPC, itsICodeTop);
    }

    private void recordJump(int jumpSource, int jumpDestination)
    {
        if (jumpSource == jumpDestination) throw Kit.codeBug();
        int offsetSite = jumpSource + 1;
        int offset = jumpDestination - jumpSource;
        if (offset != (short)offset) {
            if (itsData.longJumps == null) {
                itsData.longJumps = new UintMap();
            }
            itsData.longJumps.put(offsetSite, jumpDestination);
            offset = 0;
        }
        itsData.itsICode[offsetSite] = (byte)(offset >> 8);
        itsData.itsICode[offsetSite + 1] = (byte)offset;
    }

    private void addToken(int token)
    {
        if (!validTokenCode(token)) throw Kit.codeBug();
        addUint8(token);
    }

    private void addIcode(int icode)
    {
        if (!validIcode(icode)) throw Kit.codeBug();
        // Write negative icode as uint8 bits
        addUint8(icode & 0xFF);
    }

    private void addUint8(int value)
    {
        if ((value & ~0xFF) != 0) throw Kit.codeBug();
        byte[] array = itsData.itsICode;
        int top = itsICodeTop;
        if (top == array.length) {
            array = increaseICodeCapasity(1);
        }
        array[top] = (byte)value;
        itsICodeTop = top + 1;
    }

    private void addUint16(int value)
    {
        if ((value & ~0xFFFF) != 0) throw Kit.codeBug();
        byte[] array = itsData.itsICode;
        int top = itsICodeTop;
        if (top + 2 > array.length) {
            array = increaseICodeCapasity(2);
        }
        array[top] = (byte)(value >>> 8);
        array[top + 1] = (byte)value;
        itsICodeTop = top + 2;
    }

    private void addInt(int i)
    {
        byte[] array = itsData.itsICode;
        int top = itsICodeTop;
        if (top + 4 > array.length) {
            array = increaseICodeCapasity(4);
        }
        array[top] = (byte)(i >>> 24);
        array[top + 1] = (byte)(i >>> 16);
        array[top + 2] = (byte)(i >>> 8);
        array[top + 3] = (byte)i;
        itsICodeTop = top + 4;
    }

    private int getDoubleIndex(double num)
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
        return index;
    }

    private void addVarOp(int op, int varIndex)
    {
        switch (op) {
          case Token.GETVAR:
          case Token.SETVAR:
            if (varIndex < 128) {
                addIcode(op == Token.GETVAR ? Icode_GETVAR1 : Icode_SETVAR1);
                addUint8(varIndex);
                return;
            }
            // fallthrough
          case Icode_VAR_INC_DEC:
            addIndexOp(op, varIndex);
            return;
        }
        throw Kit.codeBug();
    }

    private void addStringOp(int op, String str)
    {
        int index = itsStrings.get(str, -1);
        if (index == -1) {
            index = itsStrings.size();
            itsStrings.put(str, index);
        }
        itsLastStringIndex = index;
        if (index < 4) {
            addIcode(Icode_REG_STR_C0 - index);
        } else if (index <= 0xFF) {
            addIcode(Icode_REG_STR1);
            addUint8(index);
         } else if (index <= 0xFFFF) {
            addIcode(Icode_REG_STR2);
            addUint16(index);
         } else {
            addIcode(Icode_REG_STR4);
            addInt(index);
        }
        if (validIcode(op)) {
            addIcode(op);
        } else {
            addToken(op);
        }
    }

    private void addIndexOp(int op, int index)
    {
        if (index < 0) Kit.codeBug();
        if (index < 6) {
            addIcode(Icode_REG_IND_C0 - index);
        } else if (index <= 0xFF) {
            addIcode(Icode_REG_IND1);
            addUint8(index);
         } else if (index <= 0xFFFF) {
            addIcode(Icode_REG_IND2);
            addUint16(index);
         } else {
            addIcode(Icode_REG_IND4);
            addInt(index);
        }
        if (validIcode(op)) {
            addIcode(op);
        } else {
            addToken(op);
        }
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

    private byte[] increaseICodeCapasity(int extraSize)
    {
        int capacity = itsData.itsICode.length;
        int top = itsICodeTop;
        if (top + extraSize <= capacity) throw Kit.codeBug();
        capacity *= 2;
        if (top + extraSize > capacity) {
            capacity = top + extraSize;
        }
        byte[] array = new byte[capacity];
        System.arraycopy(itsData.itsICode, 0, array, 0, top);
        itsData.itsICode = array;
        return array;
    }

    private void stackChange(int change)
    {
        if (change <= 0) {
            itsStackDepth += change;
        } else {
            int newDepth = itsStackDepth + change;
            if (newDepth > itsData.itsMaxStack) {
                itsData.itsMaxStack = newDepth;
            }
            itsStackDepth = newDepth;
        }
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

    private static void dumpICode(InterpreterData idata)
    {
        if (!Token.printICode) {
            return;
        }

        byte iCode[] = idata.itsICode;
        int iCodeLength = iCode.length;
        String[] strings = idata.itsStringTable;
        PrintStream out = System.out;
        out.println("ICode dump, for " + idata.itsName
                    + ", length = " + iCodeLength);
        out.println("MaxStack = " + idata.itsMaxStack);

        int indexReg = 0;
        for (int pc = 0; pc < iCodeLength; ) {
            out.flush();
            out.print(" [" + pc + "] ");
            int token = iCode[pc];
            int icodeLength = bytecodeSpan(token);
            String tname = bytecodeName(token);
            int old_pc = pc;
            ++pc;
            switch (token) {
              default:
                if (icodeLength != 1) Kit.codeBug();
                out.println(tname);
                break;

              case Icode_GOSUB :
              case Token.GOTO :
              case Token.IFEQ :
              case Token.IFNE :
              case Icode_IFEQ_POP :
              case Icode_LEAVEDQ : {
                int newPC = pc + getShort(iCode, pc) - 1;
                out.println(tname + " " + newPC);
                pc += 2;
                break;
              }
              case Icode_VAR_INC_DEC :
              case Icode_NAME_INC_DEC :
              case Icode_PROP_INC_DEC :
              case Icode_ELEM_INC_DEC :
              case Icode_REF_INC_DEC: {
                int incrDecrType = iCode[pc];
                out.println(tname + " " + incrDecrType);
                ++pc;
                break;
              }

              case Icode_CALLSPECIAL : {
                int callType = iCode[pc] & 0xFF;
                boolean isNew =  (iCode[pc + 1] != 0);
                int line = getShort(iCode, pc+2);
                out.println(tname+" "+callType+" "+isNew+" "+indexReg+" "+line);
                pc += 4;
                break;
              }
              case Token.REGEXP :
                out.println(tname+" "+idata.itsRegExpLiterals[indexReg]);
                break;
              case Token.OBJECTLIT :
              case Icode_SPARE_ARRAYLIT :
                out.println(tname+" "+idata.literalIds[indexReg]);
                break;
              case Icode_CLOSURE_EXPR :
              case Icode_CLOSURE_STMT :
                out.println(tname+" "+idata.itsNestedFunctions[indexReg]);
                break;
              case Token.CALL :
              case Token.REF_CALL :
                out.println(tname+' '+indexReg);
                break;
              case Token.NEW :
                out.println(tname+' '+indexReg);
                pc += 2;
                break;
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
                double value = idata.itsDoubleTable[indexReg];
                out.println(tname + " " + value);
                pc += 2;
                break;
              }
              case Icode_LINE : {
                int line = getShort(iCode, pc);
                out.println(tname + " : " + line);
                pc += 2;
                break;
              }
              case Icode_REG_STR1: {
                String str = strings[0xFF & iCode[pc]];
                out.println(tname + " \"" + str + '"');
                ++pc;
                break;
              }
              case Icode_REG_STR2: {
                String str = strings[getIndex(iCode, pc)];
                out.println(tname + " \"" + str + '"');
                pc += 2;
                break;
              }
              case Icode_REG_STR4: {
                String str = strings[getInt(iCode, pc)];
                out.println(tname + " \"" + str + '"');
                pc += 4;
                break;
              }
              case Icode_REG_IND1: {
                indexReg = 0xFF & iCode[pc];
                out.println(tname+" "+indexReg);
                ++pc;
                break;
              }
              case Icode_REG_IND2: {
                indexReg = getIndex(iCode, pc);
                out.println(tname+" "+indexReg);
                pc += 2;
                break;
              }
              case Icode_REG_IND4: {
                indexReg = getInt(iCode, pc);
                out.println(tname+" "+indexReg);
                pc += 4;
                break;
              }
              case Icode_GETVAR1:
              case Icode_SETVAR1:
                indexReg = iCode[pc];
                out.println(tname+" "+indexReg);
                ++pc;
                break;
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

    private static int bytecodeSpan(int bytecode)
    {
        switch (bytecode) {
            case Token.THROW :
                // source line
                return 1 + 2;

            case Icode_GOSUB :
            case Token.GOTO :
            case Token.IFEQ :
            case Token.IFNE :
            case Icode_IFEQ_POP :
            case Icode_LEAVEDQ :
                // target pc offset
                return 1 + 2;

            case Icode_CALLSPECIAL :
                // call type
                // is new
                // line number
                return 1 + 1 + 1 + 2;

            case Token.NEW :
                // index of potential function name for debugging
                return 1 + 2;

            case Icode_VAR_INC_DEC:
            case Icode_NAME_INC_DEC:
            case Icode_PROP_INC_DEC:
            case Icode_ELEM_INC_DEC:
            case Icode_REF_INC_DEC:
                // type of ++/--
                return 1 + 1;

            case Icode_SHORTNUMBER :
                // short number
                return 1 + 2;

            case Icode_INTNUMBER :
                // int number
                return 1 + 4;

            case Icode_REG_IND1:
                // ubyte index
                return 1 + 1;

            case Icode_REG_IND2:
                // ushort index
                return 1 + 2;

            case Icode_REG_IND4:
                // int index
                return 1 + 4;

            case Icode_REG_STR1:
                // ubyte string index
                return 1 + 1;

            case Icode_REG_STR2:
                // ushort string index
                return 1 + 2;

            case Icode_REG_STR4:
                // int string index
                return 1 + 4;

            case Icode_GETVAR1:
            case Icode_SETVAR1:
                // byte var index
                return 1 + 1;

            case Icode_LINE :
                // line number
                return 1 + 2;
        }
        if (!validBytecode(bytecode)) throw Kit.codeBug();
        return 1;
    }

    static int[] getLineNumbers(InterpreterData data)
    {
        UintMap presentLines = new UintMap();

        byte[] iCode = data.itsICode;
        int iCodeLength = iCode.length;
        for (int pc = 0; pc != iCodeLength;) {
            int bytecode = iCode[pc];
            int span = bytecodeSpan(bytecode);
            if (bytecode == Icode_LINE) {
                if (span != 3) Kit.codeBug();
                int line = getShort(iCode, pc + 1);
                presentLines.put(line, 0);
            }
            pc += span;
        }

        return presentLines.getKeys();
    }

    static String getSourcePositionFromStack(Context cx, int[] linep)
    {
        InterpreterData idata = cx.interpreterData;
        if (cx.interpreterLineIndex >= 0) {
            linep[0] = getShort(idata.itsICode, cx.interpreterLineIndex);
        } else {
            linep[0] = 0;
        }
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

        final int LOCAL_SHFT = idata.itsMaxVars;
        final int STACK_SHFT = LOCAL_SHFT + idata.itsMaxLocals;

// stack[0 <= i < LOCAL_SHFT]: variables
// stack[LOCAL_SHFT <= i < TRY_STACK_SHFT]: used for local temporaries
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
            stack[i] = arg;
            if (arg == DBL_MRK) {
                sDbl[i] = argsDbl[argShift + i];
            }
        }
        for (int i = definedArgs; i != idata.itsMaxVars; ++i) {
            stack[i] = undefined;
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
                scope = ScriptRuntime.enterActivationFunction(cx, scope,
                                                              fnOrScript,
                                                              thisObj, args);
            }

        } else {
            scope = ScriptRuntime.enterScript(cx, scope, fnOrScript, thisObj);
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
                scope = ScriptRuntime.enterActivationFunction(cx, scope,
                                                              fnOrScript,
                                                              thisObj, args);
            }
            debuggerFrame.onEnter(cx, scope, thisObj, args);
        }

        InterpreterData savedData = cx.interpreterData;
        cx.interpreterData = idata;
        cx.interpreterLineIndex = idata.firstLinePC;

        Object result = undefined;
        // If javaException != null on exit, it will be throw instead of
        // normal return
        Throwable javaException = null;
        int exceptionPC = -1;

        byte[] iCode = idata.itsICode;
        int pc = 0;
        int pcPrevBranch = 0;
        final boolean instructionCounting = (cx.instructionThreshold != 0);
        // arbitrary number to add to instructionCount when calling
        // other functions
        final int INVOCATION_COST = 100;

        String[] strings = idata.itsStringTable;
        String stringReg = null;
        int indexReg = 0;

        Loop: for (;;) {
            try {
                int op = iCode[pc++];
              jumplessRun:
                {
    // Back indent to ease imlementation reading
switch (op) {

    case Icode_CATCH: {
        // The following code should be executed inside try/catch inside main
        // loop, not in the loop catch block itself to deal with exceptions
        // from observeInstructionCount. A special bytecode is used only to
        // simplify logic.
        if (javaException == null) Kit.codeBug();

        pc = -1;
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
                    pc = idata.itsExceptionTable[handlerOffset
                                                 + EXCEPTION_CATCH_SLOT];
                    if (pc >= 0) {
                        // Has catch block
                        doCatch = true;
                    }
                }
                if (pc < 0) {
                    pc = idata.itsExceptionTable[handlerOffset
                                                 + EXCEPTION_FINALLY_SLOT];
                }
            }
        }

        if (debuggerFrame != null && !(javaException instanceof Error)) {
            debuggerFrame.onExceptionThrown(cx, javaException);
        }

        if (pc < 0) {
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

        if (instructionCounting) {
            // 500: catch cost
            cx.addInstructionCount(500);
            pcPrevBranch = pc;
        }
        continue Loop;
    }
    case Token.THROW: {
        Object value = stack[stackTop];
        if (value == DBL_MRK) value = doubleWrap(sDbl[stackTop]);
        --stackTop;

        int sourceLine = getShort(iCode, pc);
        throw new JavaScriptException(value, idata.itsSourceFile, sourceLine);
    }
    case Token.GE :
    case Token.LE :
    case Token.GT :
    case Token.LT :
        stackTop = do_cmp(stack, sDbl, stackTop, op);
        continue Loop;
    case Token.IN : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        boolean valBln = ScriptRuntime.in(lhs, rhs, cx, scope);
        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
        continue Loop;
    }
    case Token.INSTANCEOF : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        boolean valBln = ScriptRuntime.instanceOf(lhs, rhs, cx, scope);
        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
        continue Loop;
    }
    case Token.EQ :
    case Token.NE :
        stackTop = do_eq(stack, sDbl, stackTop, op);
        continue Loop;
    case Token.SHEQ :
    case Token.SHNE :
        stackTop = do_sheq(stack, sDbl, stackTop, op);
        continue Loop;
    case Token.IFNE :
        if (stack_boolean(stack, sDbl, stackTop--)) {
            pc += 2;
            continue Loop;
        }
        break jumplessRun;
    case Token.IFEQ :
        if (!stack_boolean(stack, sDbl, stackTop--)) {
            pc += 2;
            continue Loop;
        }
        break jumplessRun;
    case Icode_IFEQ_POP :
        if (!stack_boolean(stack, sDbl, stackTop--)) {
            pc += 2;
            continue Loop;
        }
        stack[stackTop--] = null;
        break jumplessRun;
    case Token.GOTO :
        break jumplessRun;
    case Icode_GOSUB :
        ++stackTop;
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = pc + 2;
        break jumplessRun;
    case Icode_RETSUB : {
        // indexReg: local to store return address
        if (instructionCounting) {
            cx.addInstructionCount(pc - pcPrevBranch);
        }
        Object value = stack[LOCAL_SHFT + indexReg];
        if (value != DBL_MRK) {
            // Invocation from exception handler, restore object to rethrow
            javaException = (Throwable)value;
            exceptionPC = pc - 1;
            pc = getJavaCatchPC(iCode);
        } else {
            // Normal return from GOSUB
            pc = (int)sDbl[LOCAL_SHFT + indexReg];
            pcPrevBranch = pc;
        }
        continue Loop;
    }
    case Icode_POP :
        stack[stackTop] = null;
        stackTop--;
        continue Loop;
    case Icode_POP_RESULT :
        result = stack[stackTop];
        if (result == DBL_MRK) result = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = null;
        --stackTop;
        continue Loop;
    case Icode_DUP :
        stack[stackTop + 1] = stack[stackTop];
        sDbl[stackTop + 1] = sDbl[stackTop];
        stackTop++;
        continue Loop;
    case Icode_DUP2 :
        stack[stackTop + 1] = stack[stackTop - 1];
        sDbl[stackTop + 1] = sDbl[stackTop - 1];
        stack[stackTop + 2] = stack[stackTop];
        sDbl[stackTop + 2] = sDbl[stackTop];
        stackTop += 2;
        continue Loop;
    case Icode_SWAP : {
        Object o = stack[stackTop];
        stack[stackTop] = stack[stackTop - 1];
        stack[stackTop - 1] = o;
        double d = sDbl[stackTop];
        sDbl[stackTop] = sDbl[stackTop - 1];
        sDbl[stackTop - 1] = d;
        continue Loop;
    }
    case Token.RETURN :
        result = stack[stackTop];
        if (result == DBL_MRK) result = doubleWrap(sDbl[stackTop]);
        --stackTop;
        break Loop;
    case Token.RETURN_RESULT :
        break Loop;
    case Icode_RETUNDEF :
        result = undefined;
        break Loop;
    case Token.BITNOT : {
        int rIntValue = stack_int32(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = ~rIntValue;
        continue Loop;
    }
    case Token.BITAND : {
        int rIntValue = stack_int32(stack, sDbl, stackTop);
        --stackTop;
        int lIntValue = stack_int32(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lIntValue & rIntValue;
        continue Loop;
    }
    case Token.BITOR : {
        int rIntValue = stack_int32(stack, sDbl, stackTop);
        --stackTop;
        int lIntValue = stack_int32(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lIntValue | rIntValue;
        continue Loop;
    }
    case Token.BITXOR : {
        int rIntValue = stack_int32(stack, sDbl, stackTop);
        --stackTop;
        int lIntValue = stack_int32(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lIntValue ^ rIntValue;
        continue Loop;
    }
    case Token.LSH : {
        int rIntValue = stack_int32(stack, sDbl, stackTop);
        --stackTop;
        int lIntValue = stack_int32(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lIntValue << rIntValue;
        continue Loop;
    }
    case Token.RSH : {
        int rIntValue = stack_int32(stack, sDbl, stackTop);
        --stackTop;
        int lIntValue = stack_int32(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lIntValue >> rIntValue;
        continue Loop;
    }
    case Token.URSH : {
        int rIntValue = stack_int32(stack, sDbl, stackTop) & 0x1F;
        --stackTop;
        double lDbl = stack_double(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = ScriptRuntime.toUint32(lDbl) >>> rIntValue;
        continue Loop;
    }
    case Token.NEG : {
        double rDbl = stack_double(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = -rDbl;
        continue Loop;
    }
    case Token.POS : {
        double rDbl = stack_double(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = rDbl;
        continue Loop;
    }
    case Token.ADD :
        stackTop = do_add(stack, sDbl, stackTop, cx);
        continue Loop;
    case Token.SUB : {
        double rDbl = stack_double(stack, sDbl, stackTop);
        --stackTop;
        double lDbl = stack_double(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lDbl - rDbl;
        continue Loop;
    }
    case Token.MUL : {
        double rDbl = stack_double(stack, sDbl, stackTop);
        --stackTop;
        double lDbl = stack_double(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lDbl * rDbl;
        continue Loop;
    }
    case Token.DIV : {
        double rDbl = stack_double(stack, sDbl, stackTop);
        --stackTop;
        double lDbl = stack_double(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        // Detect the divide by zero or let Java do it ?
        sDbl[stackTop] = lDbl / rDbl;
        continue Loop;
    }
    case Token.MOD : {
        double rDbl = stack_double(stack, sDbl, stackTop);
        --stackTop;
        double lDbl = stack_double(stack, sDbl, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lDbl % rDbl;
        continue Loop;
    }
    case Token.NOT :
        stack[stackTop] = stack_boolean(stack, sDbl, stackTop)
                          ? Boolean.FALSE : Boolean.TRUE;
        continue Loop;
    case Token.BINDNAME :
        stack[++stackTop] = ScriptRuntime.bind(cx, scope, stringReg);
        continue Loop;
    case Token.SETNAME : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Scriptable lhs = (Scriptable)stack[stackTop];
        stack[stackTop] = ScriptRuntime.setName(lhs, rhs, cx, stringReg);
        continue Loop;
    }
    case Token.DELPROP : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.delete(cx, scope, lhs, rhs);
        continue Loop;
    }
    case Token.GETPROP : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.getObjectProp(lhs, stringReg,
                                                      cx, scope);
        continue Loop;
    }
    case Token.SETPROP : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.setObjectProp(lhs, stringReg, rhs,
                                                      cx, scope);
        continue Loop;
    }
    case Icode_PROP_INC_DEC : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.propIncrDecr(lhs, stringReg, scope,
                                                     iCode[pc]);
        ++pc;
        continue Loop;
    }
    case Token.GETELEM :
        stackTop = do_getElem(stack, sDbl, stackTop, cx, scope);
        continue Loop;
    case Token.SETELEM :
        stackTop = do_setElem(stack, sDbl, stackTop, cx, scope);
        continue Loop;
    case Icode_ELEM_INC_DEC: {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.elemIncrDecr(lhs, rhs, cx, scope,
                                                     iCode[pc]);
        ++pc;
        continue Loop;
    }
    case Token.GET_REF : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.getReference(lhs);
        continue Loop;
    }
    case Token.SET_REF : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.setReference(lhs, rhs);
        continue Loop;
    }
    case Token.DEL_REF : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.deleteReference(lhs);
        continue Loop;
    }
    case Icode_REF_INC_DEC : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.referenceIncrDecr(lhs, iCode[pc]);
        ++pc;
        continue Loop;
    }
    case Token.LOCAL_SAVE :
        stack[LOCAL_SHFT + indexReg] = stack[stackTop];
        sDbl[LOCAL_SHFT + indexReg] = sDbl[stackTop];
        --stackTop;
        continue Loop;
    case Token.LOCAL_LOAD :
        ++stackTop;
        stack[stackTop] = stack[LOCAL_SHFT + indexReg];
        sDbl[stackTop] = sDbl[LOCAL_SHFT + indexReg];
        continue Loop;
    case Icode_NAME_AND_THIS :
        // stringReg: name
        ++stackTop;
        stack[stackTop] = ScriptRuntime.getNameFunctionAndThis(stringReg,
                                                               cx, scope);
        ++stackTop;
        stack[stackTop] = ScriptRuntime.lastStoredScriptable(cx);
        continue Loop;
    case Icode_PROP_AND_THIS: {
        Object obj = stack[stackTop];
        if (obj == DBL_MRK) obj = doubleWrap(sDbl[stackTop]);
        // stringReg: property
        stack[stackTop] = ScriptRuntime.getPropFunctionAndThis(obj, stringReg,
                                                               cx, scope);
        ++stackTop;
        stack[stackTop] = ScriptRuntime.lastStoredScriptable(cx);
        continue Loop;
    }
    case Icode_ELEM_AND_THIS: {
        Object obj = stack[stackTop - 1];
        if (obj == DBL_MRK) obj = doubleWrap(sDbl[stackTop - 1]);
        Object id = stack[stackTop];
        if (id == DBL_MRK) id = doubleWrap(sDbl[stackTop]);
        stack[stackTop - 1] = ScriptRuntime.getElemFunctionAndThis(obj, id,
                                                                   cx, scope);
        stack[stackTop] = ScriptRuntime.lastStoredScriptable(cx);
        continue Loop;
    }
    case Icode_VALUE_AND_THIS : {
        Object value = stack[stackTop];
        if (value == DBL_MRK) value = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.getValueFunctionAndThis(value, cx);
        ++stackTop;
        stack[stackTop] = ScriptRuntime.lastStoredScriptable(cx);
        continue Loop;
    }
    case Icode_CALLSPECIAL : {
        if (instructionCounting) {
            cx.instructionCount += INVOCATION_COST;
        }
        // indexReg: number of arguments
        int callType = iCode[pc] & 0xFF;
        boolean isNew =  (iCode[pc + 1] != 0);
        int sourceLine = getShort(iCode, pc + 2);
        stackTop -= indexReg;
        Object[] outArgs = getArgsArray(stack, sDbl, stackTop + 1, indexReg);
        if (isNew) {
            Object function = stack[stackTop];
            if (function == DBL_MRK) function = doubleWrap(sDbl[stackTop]);
            stack[stackTop] = ScriptRuntime.newSpecial(
                                  cx, function, outArgs, scope, callType);
        } else {
            // Call code generation ensure that stack here
            // is ... Function Scriptable
            Scriptable functionThis = (Scriptable)stack[stackTop];
            --stackTop;
            Function function = (Function)stack[stackTop];
            stack[stackTop] = ScriptRuntime.callSpecial(
                                  cx, function, functionThis, outArgs,
                                  scope, thisObj, callType,
                                  idata.itsSourceFile, sourceLine);
        }
        pc += 4;
        continue Loop;
    }
    case Token.CALL : {
        if (instructionCounting) {
            cx.instructionCount += INVOCATION_COST;
        }
        // indexReg: number of arguments
        stackTop -= indexReg;
        int calleeArgShft = stackTop + 1;

        // CALL generation ensures that funThisObj and fun
        // are already Scriptable and Function objects respectively
        Scriptable funThisObj = (Scriptable)stack[stackTop];
        --stackTop;
        Function fun = (Function)stack[stackTop];

        Scriptable funScope = scope;
        if (idata.itsNeedsActivation) {
            funScope = ScriptableObject.getTopLevelScope(scope);
        }

        if (fun instanceof InterpretedFunction) {
            // Inlining of InterpretedFunction.call not to create
            // argument array
            InterpretedFunction ifun = (InterpretedFunction)fun;
            stack[stackTop] = interpret(cx, funScope, funThisObj,
                                        stack, sDbl, calleeArgShft, indexReg,
                                        ifun, ifun.itsData);
        } else {
            Object[] outArgs = getArgsArray(stack, sDbl, calleeArgShft,
                                            indexReg);
            stack[stackTop] = fun.call(cx, funScope, funThisObj, outArgs);
        }
        continue Loop;
    }
    case Token.REF_CALL : {
        if (instructionCounting) {
            cx.instructionCount += INVOCATION_COST;
        }
        // indexReg: number of arguments
        stackTop -= indexReg;
        int calleeArgShft = stackTop + 1;

        // REF_CALL generation ensures that funThisObj and fun
        // are already Scriptable and Function objects respectively
        Scriptable funThisObj = (Scriptable)stack[stackTop];
        --stackTop;
        Function fun = (Function)stack[stackTop];

        Scriptable funScope = scope;
        if (idata.itsNeedsActivation) {
            funScope = ScriptableObject.getTopLevelScope(scope);
        }
        Object[] outArgs = getArgsArray(stack, sDbl, calleeArgShft, indexReg);
        stack[stackTop] = ScriptRuntime.referenceCall(fun, funThisObj, outArgs,
                                                      cx, funScope);
        continue Loop;
    }
    case Token.NEW : {
        if (instructionCounting) {
            cx.instructionCount += INVOCATION_COST;
        }
        // indexReg: number of arguments
        stackTop -= indexReg;
        int calleeArgShft = stackTop + 1;
        Object lhs = stack[stackTop];

        if (lhs instanceof InterpretedFunction) {
            // Inlining of InterpretedFunction.construct not to create
            // argument array
            InterpretedFunction f = (InterpretedFunction)lhs;
            Scriptable newInstance = f.createObject(cx, scope);
            Object callResult = interpret(cx, scope, newInstance,
                                          stack, sDbl, calleeArgShft, indexReg,
                                          f, f.itsData);
            if (callResult instanceof Scriptable && callResult != undefined) {
                stack[stackTop] = callResult;
            } else {
                stack[stackTop] = newInstance;
            }
        } else if (lhs instanceof Function) {
            Function f = (Function)lhs;
            Object[] outArgs = getArgsArray(stack, sDbl, calleeArgShft,
                                            indexReg);
            stack[stackTop] = f.construct(cx, scope, outArgs);
        } else {
            if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
            throw notAFunction(lhs, idata, pc);
        }
        pc += 2; // skip debug name index
        continue Loop;
    }
    case Token.TYPEOF : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.typeof(lhs);
        continue Loop;
    }
    case Icode_TYPEOFNAME :
        stack[++stackTop] = ScriptRuntime.typeofName(scope, stringReg);
        continue Loop;
    case Token.STRING :
        stack[++stackTop] = stringReg;
        continue Loop;
    case Icode_SHORTNUMBER :
        ++stackTop;
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = getShort(iCode, pc);
        pc += 2;
        continue Loop;
    case Icode_INTNUMBER :
        ++stackTop;
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = getInt(iCode, pc);
        pc += 4;
        continue Loop;
    case Token.NUMBER :
        ++stackTop;
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = idata.itsDoubleTable[indexReg];
        continue Loop;
    case Token.NAME :
        stack[++stackTop] = ScriptRuntime.name(cx, scope, stringReg);
        continue Loop;
    case Icode_NAME_INC_DEC :
        stack[++stackTop] = ScriptRuntime.nameIncrDecr(scope, stringReg,
                                                       iCode[pc]);
        ++pc;
        continue Loop;
    case Icode_SETVAR1:
        indexReg = iCode[pc++];
        // fallthrough
    case Token.SETVAR :
        if (!useActivationVars) {
            stack[indexReg] = stack[stackTop];
            sDbl[indexReg] = sDbl[stackTop];
        } else {
            Object val = stack[stackTop];
            if (val == DBL_MRK) val = doubleWrap(sDbl[stackTop]);
            activationPut(fnOrScript, scope, indexReg, val);
        }
        continue Loop;
    case Icode_GETVAR1:
        indexReg = iCode[pc++];
        // fallthrough
    case Token.GETVAR :
        ++stackTop;
        if (!useActivationVars) {
            stack[stackTop] = stack[indexReg];
            sDbl[stackTop] = sDbl[indexReg];
        } else {
            stack[stackTop] = activationGet(fnOrScript, scope, indexReg);
        }
        continue Loop;
    case Icode_VAR_INC_DEC : {
        // indexReg : varindex
        ++stackTop;
        int incrDecrMask = iCode[pc];
        if (!useActivationVars) {
            stack[stackTop] = DBL_MRK;
            Object varValue = stack[indexReg];
            double d;
            if (varValue == DBL_MRK) {
                d = sDbl[indexReg];
            } else {
                d = ScriptRuntime.toNumber(varValue);
                stack[indexReg] = DBL_MRK;
            }
            double d2 = ((incrDecrMask & Node.DECR_FLAG) == 0)
                        ? d + 1.0 : d - 1.0;
            sDbl[indexReg] = d2;
            sDbl[stackTop] = ((incrDecrMask & Node.POST_FLAG) == 0) ? d2 : d;
        } else {
            String varName = fnOrScript.argNames[indexReg];
            stack[stackTop] = ScriptRuntime.nameIncrDecr(scope, varName,
                                                         incrDecrMask);
        }
        ++pc;
        continue Loop;
    }
    case Icode_ZERO :
        ++stackTop;
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = 0;
        continue Loop;
    case Icode_ONE :
        ++stackTop;
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = 1;
        continue Loop;
    case Token.NULL :
        stack[++stackTop] = null;
        continue Loop;
    case Token.THIS :
        stack[++stackTop] = thisObj;
        continue Loop;
    case Token.THISFN :
        stack[++stackTop] = fnOrScript;
        continue Loop;
    case Token.FALSE :
        stack[++stackTop] = Boolean.FALSE;
        continue Loop;
    case Token.TRUE :
        stack[++stackTop] = Boolean.TRUE;
        continue Loop;
    case Icode_UNDEF :
        stack[++stackTop] = undefined;
        continue Loop;
    case Token.ENTERWITH : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        scope = ScriptRuntime.enterWith(lhs, scope);
        ++withDepth;
        continue Loop;
    }
    case Token.LEAVEWITH :
        scope = ScriptRuntime.leaveWith(scope);
        --withDepth;
        continue Loop;
    case Token.CATCH_SCOPE :
        stack[stackTop] = ScriptRuntime.newCatchScope(stringReg,
                                                      stack[stackTop]);
        continue Loop;
    case Token.ENUM_INIT_KEYS : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        stack[LOCAL_SHFT + indexReg] = ScriptRuntime.enumInit(lhs, scope);
        continue Loop;
    }
    case Token.ENUM_INIT_VALUES : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        stack[LOCAL_SHFT + indexReg] = ScriptRuntime.enumValuesInit(lhs, scope);
        continue Loop;
    }
    case Token.ENUM_NEXT :
    case Token.ENUM_ID : {
        Object val = stack[LOCAL_SHFT + indexReg];
        ++stackTop;
        stack[stackTop] = (op == Token.ENUM_NEXT)
                          ? (Object)ScriptRuntime.enumNext(val)
                          : (Object)ScriptRuntime.enumId(val, cx);
        continue Loop;
    }
    case Token.SPECIAL_REF : {
        //stringReg: name of special property
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.specialReference(lhs, stringReg,
                                                         cx, scope);
        continue Loop;
    }
    case Token.XML_REF : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.xmlReference(lhs, cx, scope);
        continue Loop;
    }
    case Token.GENERIC_REF : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.genericReference(lhs, cx, scope);
        continue Loop;
    }
    case Icode_SCOPE :
        stack[++stackTop] = scope;
        continue Loop;
    case Icode_CLOSURE_EXPR : {
        InterpreterData closureData = idata.itsNestedFunctions[indexReg];
        stack[++stackTop] = createFunction(cx, scope, closureData,
                                           idata.itsFromEvalCode);
        continue Loop;
    }
    case Icode_CLOSURE_STMT : {
        InterpreterData closureData = idata.itsNestedFunctions[indexReg];
        createFunction(cx, scope, closureData, idata.itsFromEvalCode);
        continue Loop;
    }
    case Token.REGEXP : {
        Scriptable regexp;
        if (idata.itsFunctionType != 0) {
            regexp = ((InterpretedFunction)fnOrScript).itsRegExps[indexReg];
        } else {
            if (scriptRegExps == null) {
                scriptRegExps = wrapRegExps(cx, scope, idata);
            }
            regexp = scriptRegExps[indexReg];
        }
        stack[++stackTop] = regexp;
        continue Loop;
    }
    case Icode_LITERAL_NEW :
        // indexReg: number of values in the literal
        ++stackTop;
        stack[stackTop] = new Object[indexReg];
        sDbl[stackTop] = 0;
        continue Loop;
    case Icode_LITERAL_SET : {
        Object value = stack[stackTop];
        if (value == DBL_MRK) value = doubleWrap(sDbl[stackTop]);
        --stackTop;
        int i = (int)sDbl[stackTop];
        ((Object[])stack[stackTop])[i] = value;
        sDbl[stackTop] = i + 1;
        continue Loop;
    }
    case Token.ARRAYLIT :
    case Icode_SPARE_ARRAYLIT :
    case Token.OBJECTLIT : {
        Object[] data = (Object[])stack[stackTop];
        Object val;
        if (op == Token.OBJECTLIT) {
            Object[] ids = (Object[])idata.literalIds[indexReg];
            val = ScriptRuntime.newObjectLiteral(ids, data, cx, scope);
        } else {
            int[] skipIndexces = null;
            if (op == Icode_SPARE_ARRAYLIT) {
                skipIndexces = (int[])idata.literalIds[indexReg];
            }
            val = ScriptRuntime.newArrayLiteral(data, skipIndexces, cx, scope);
        }
        stack[stackTop] = val;
        continue Loop;
    }
    case Icode_ENTERDQ : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        scope = ScriptRuntime.enterDotQuery(lhs, scope);
        ++withDepth;
        continue Loop;
    }
    case Icode_LEAVEDQ : {
        boolean valBln = stack_boolean(stack, sDbl, stackTop);
        Object x = ScriptRuntime.updateDotQuery(valBln, scope);
        if (x != null) {
            stack[stackTop] = x;
            scope = ScriptRuntime.leaveDotQuery(scope);
            --withDepth;
            pc += 2;
            continue Loop;
        }
        // reset stack and PC to code after ENTERDQ
        --stackTop;
        break jumplessRun;
    }
    case Token.DEFAULTNAMESPACE : {
        Object value = stack[stackTop];
        if (value == DBL_MRK) value = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.setDefaultNamespace(value, cx);
        continue Loop;
    }
    case Token.ESCXMLATTR : {
        Object value = stack[stackTop];
        if (value != DBL_MRK) {
            stack[stackTop] = ScriptRuntime.escapeAttributeValue(value, cx);
        }
        continue Loop;
    }
    case Token.ESCXMLTEXT : {
        Object value = stack[stackTop];
        if (value != DBL_MRK) {
            stack[stackTop] = ScriptRuntime.escapeTextValue(value, cx);
        }
        continue Loop;
    }
    case Token.TOATTRNAME : {
        Object value = stack[stackTop];
        if (value == DBL_MRK) value = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.toAttributeName(value, cx);
        continue Loop;
    }
    case Token.DESCENDANTS : {
        Object value = stack[stackTop];
        if(value == DBL_MRK) value = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.toDescendantsName(value, cx);
        continue Loop;
    }
    case Token.COLONCOLON : {
        Object value = stack[stackTop];
        if (value == DBL_MRK) value = doubleWrap(sDbl[stackTop]);
        // stringReg contains namespace
        stack[stackTop] = ScriptRuntime.toQualifiedName(stringReg, value,
                                                        cx, scope);
        continue Loop;
    }
    case Icode_LINE :
        cx.interpreterLineIndex = pc;
        if (debuggerFrame != null) {
            int line = getShort(iCode, pc);
            debuggerFrame.onLineChange(cx, line);
        }
        pc += 2;
        continue Loop;
    case Icode_REG_IND_C0:
        indexReg = 0;
        continue Loop;
    case Icode_REG_IND_C1:
        indexReg = 1;
        continue Loop;
    case Icode_REG_IND_C2:
        indexReg = 2;
        continue Loop;
    case Icode_REG_IND_C3:
        indexReg = 3;
        continue Loop;
    case Icode_REG_IND_C4:
        indexReg = 4;
        continue Loop;
    case Icode_REG_IND_C5:
        indexReg = 5;
        continue Loop;
    case Icode_REG_IND1:
        indexReg = 0xFF & iCode[pc];
        ++pc;
        continue Loop;
    case Icode_REG_IND2:
        indexReg = getIndex(iCode, pc);
        pc += 2;
        continue Loop;
    case Icode_REG_IND4:
        indexReg = getInt(iCode, pc);
        pc += 4;
        continue Loop;
    case Icode_REG_STR_C0:
        stringReg = strings[0];
        continue Loop;
    case Icode_REG_STR_C1:
        stringReg = strings[1];
        continue Loop;
    case Icode_REG_STR_C2:
        stringReg = strings[2];
        continue Loop;
    case Icode_REG_STR_C3:
        stringReg = strings[3];
        continue Loop;
    case Icode_REG_STR1:
        stringReg = strings[0xFF & iCode[pc]];
        ++pc;
        continue Loop;
    case Icode_REG_STR2:
        stringReg = strings[getIndex(iCode, pc)];
        pc += 2;
        continue Loop;
    case Icode_REG_STR4:
        stringReg = strings[getInt(iCode, pc)];
        pc += 4;
        continue Loop;
    default :
        dumpICode(idata);
        throw new RuntimeException("Unknown icode : "+op+" @ pc : "+(pc-1));
}  // end of interpreter switch

                } // end of jumplessRun label block

                // This should be reachable only for jump implementation
                // when pc points to encoded target offset
                if (instructionCounting) {
                    cx.addInstructionCount(pc + 2 - pcPrevBranch);
                }
                int offset = getShort(iCode, pc);
                if (offset != 0) {
                    // -1 accounts for pc pointing to jump opcode + 1
                    pc += offset - 1;
                } else {
                    pc = idata.longJumps.getExistingInt(pc);
                }
                pcPrevBranch = pc;
                continue Loop;

            }  // end of interpreter try
            catch (Throwable ex) {
                if (instructionCounting) {
                    // Can not call addInstructionCount as it may trigger
                    // exception
                    cx.instructionCount += pc - pcPrevBranch;
                }
                javaException = ex;
                exceptionPC = pc - 1;
                pc = getJavaCatchPC(iCode);
                continue Loop;
            }
        } // end of interpreter loop

        cx.interpreterData = savedData;

        if (debuggerFrame != null) {
            if (javaException != null) {
                debuggerFrame.onExit(cx, true, javaException);
            } else {
                debuggerFrame.onExit(cx, false, result);
            }
        }

        if (idata.itsFunctionType != 0) {
            if (idata.itsNeedsActivation || debuggerFrame != null) {
                ScriptRuntime.exitActivationFunction(cx);
            }
        } else {
            ScriptRuntime.exitScript(cx);
        }

        if (javaException != null) {
            if (javaException instanceof RuntimeException) {
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
        if (x == Boolean.TRUE) {
            return true;
        } else if (x == Boolean.FALSE) {
            return false;
        } else if (x == DBL_MRK) {
            double d = stackDbl[i];
            return d == d && d != 0.0;
        } else if (x == null || x == Undefined.instance) {
            return false;
        } else if (x instanceof Number) {
            double d = ((Number)x).doubleValue();
            return (d == d && d != 0.0);
        } else if (x instanceof Boolean) {
            return ((Boolean)x).booleanValue();
        } else {
            return ScriptRuntime.toBoolean(x);
        }
    }

    private static int do_add(Object[] stack, double[] sDbl, int stackTop,
                              Context cx)
    {
        --stackTop;
        Object rhs = stack[stackTop + 1];
        Object lhs = stack[stackTop];
        if (rhs == DBL_MRK) {
            double rDbl = sDbl[stackTop + 1];
            if (lhs == DBL_MRK) {
                sDbl[stackTop] += rDbl;
            } else {
                do_add(lhs, rDbl, stack, sDbl, stackTop, true, cx);
            }
        } else if (lhs == DBL_MRK) {
            do_add(rhs, sDbl[stackTop], stack, sDbl, stackTop, false, cx);
        } else {
            if (lhs instanceof Scriptable || rhs instanceof Scriptable) {
                stack[stackTop] = ScriptRuntime.add(lhs, rhs, cx);
                return stackTop;
            }
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
                sDbl[stackTop] = lDbl + rDbl;
            }
        }
        return stackTop;
    }

    // x + y when x is Number
    private static void do_add
        (Object lhs, double rDbl,
         Object[] stack, double[] stackDbl, int stackTop,
         boolean left_right_order, Context cx)
    {
        if (lhs instanceof Scriptable) {
            Object rhs = doubleWrap(rDbl);
            if (!left_right_order) {
                Object tmp = lhs;
                lhs = rhs;
                rhs = tmp;
            }
            stack[stackTop] = ScriptRuntime.add(lhs, rhs, cx);
            return;
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

    private static int do_cmp(Object[] stack, double[] sDbl, int stackTop,
                              int op)
    {
        --stackTop;
        Object rhs = stack[stackTop + 1];
        Object lhs = stack[stackTop];
        boolean result;
      object_compare:
        {
          number_compare:
            {
                double rDbl, lDbl;
                if (rhs == DBL_MRK) {
                    rDbl = sDbl[stackTop + 1];
                    lDbl = stack_double(stack, sDbl, stackTop);
                } else if (lhs == DBL_MRK) {
                    rDbl = ScriptRuntime.toNumber(rhs);
                    lDbl = sDbl[stackTop];
                } else {
                    break number_compare;
                }
                switch (op) {
                  case Token.GE:
                    result = (lDbl >= rDbl);
                    break object_compare;
                  case Token.LE:
                    result = (lDbl <= rDbl);
                    break object_compare;
                  case Token.GT:
                    result = (lDbl > rDbl);
                    break object_compare;
                  case Token.LT:
                    result = (lDbl < rDbl);
                    break object_compare;
                  default:
                    throw Kit.codeBug();
                }
            }
            switch (op) {
              case Token.GE:
                result = ScriptRuntime.cmp_LE(rhs, lhs);
                break;
              case Token.LE:
                result = ScriptRuntime.cmp_LE(lhs, rhs);
                break;
              case Token.GT:
                result = ScriptRuntime.cmp_LT(rhs, lhs);
                break;
              case Token.LT:
                result = ScriptRuntime.cmp_LT(lhs, rhs);
                break;
              default:
                throw Kit.codeBug();
            }
        }
        stack[stackTop] = result ? Boolean.TRUE : Boolean.FALSE;
        return stackTop;
    }

    private static int do_eq(Object[] stack, double[] sDbl, int stackTop,
                             int op)
    {
        --stackTop;
        boolean result;
        Object rhs = stack[stackTop + 1];
        Object lhs = stack[stackTop];
        if (rhs == DBL_MRK) {
            if (lhs == DBL_MRK) {
                result = (sDbl[stackTop] == sDbl[stackTop + 1]);
            } else {
                result = ScriptRuntime.eqNumber(sDbl[stackTop + 1], lhs);
            }
        } else {
            if (lhs == DBL_MRK) {
                result = ScriptRuntime.eqNumber(sDbl[stackTop], rhs);
            } else {
                result = ScriptRuntime.eq(lhs, rhs);
            }
        }
        result ^= (op == Token.NE);
        stack[stackTop] = (result) ? Boolean.TRUE : Boolean.FALSE;
        return stackTop;
    }

    private static int do_sheq(Object[] stack, double[] sDbl, int stackTop,
                               int op)
    {
        --stackTop;
        Object rhs = stack[stackTop + 1];
        Object lhs = stack[stackTop];
        boolean result;
      double_compare: {
            double rdbl, ldbl;
            if (rhs == DBL_MRK) {
                rdbl = sDbl[stackTop + 1];
                if (lhs == DBL_MRK) {
                    ldbl = sDbl[stackTop];
                } else if (lhs instanceof Number) {
                    ldbl = ((Number)lhs).doubleValue();
                } else {
                    result = false;
                    break double_compare;
                }
            } else if (lhs == DBL_MRK) {
                ldbl = sDbl[stackTop];
                if (rhs == DBL_MRK) {
                    rdbl = sDbl[stackTop + 1];
                } else if (rhs instanceof Number) {
                    rdbl = ((Number)rhs).doubleValue();
                } else {
                    result = false;
                    break double_compare;
                }
            } else {
                result = ScriptRuntime.shallowEq(lhs, rhs);
                break double_compare;
            }
            result = ldbl == rdbl;
        }
        result ^= (op == Token.SHNE);
        stack[stackTop] = (result) ? Boolean.TRUE : Boolean.FALSE;
        return stackTop;
    }

    private static int do_getElem(Object[] stack, double[] sDbl, int stackTop,
                                  Context cx, Scriptable scope)
    {
        Object lhs = stack[stackTop - 1];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop - 1]);

        Object result;
        Object id = stack[stackTop];
        if (id != DBL_MRK) {
            result = ScriptRuntime.getObjectElem(lhs, id, cx, scope);
        } else {
            double val = sDbl[stackTop];
            if (lhs == null || lhs == Undefined.instance) {
                throw ScriptRuntime.undefReadError(
                          lhs, ScriptRuntime.toString(val));
            }
            Scriptable obj = (lhs instanceof Scriptable)
                             ? (Scriptable)lhs
                             : ScriptRuntime.toObject(cx, scope, lhs);
            int index = (int)val;
            if (index == val) {
                result = ScriptRuntime.getObjectIndex(obj, index, cx);
            } else {
                String s = ScriptRuntime.toString(val);
                result = ScriptRuntime.getObjectProp(obj, s, cx);
            }
        }
        --stackTop;
        stack[stackTop] = result;
        return stackTop;
    }

    private static int do_setElem(Object[] stack, double[] sDbl, int stackTop,
                                  Context cx, Scriptable scope)
    {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        Object lhs = stack[stackTop - 2];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop - 2]);

        Object result;
        Object id = stack[stackTop - 1];
        if (id != DBL_MRK) {
            result = ScriptRuntime.setObjectElem(lhs, id, rhs, cx, scope);
        } else {
            double val = sDbl[stackTop - 1];
            if (lhs == null || lhs == Undefined.instance) {
                throw ScriptRuntime.undefWriteError(
                          lhs, ScriptRuntime.toString(val), rhs);
            }
            Scriptable obj = (lhs instanceof Scriptable)
                             ? (Scriptable)lhs
                             : ScriptRuntime.toObject(cx, scope, lhs);
            int index = (int)val;
            if (index == val) {
                result = ScriptRuntime.setObjectIndex(obj, index, rhs, cx);
            } else {
                String s = ScriptRuntime.toString(val);
                result = ScriptRuntime.setObjectProp(obj, s, rhs, cx);
            }
        }
        stackTop -= 2;
        stack[stackTop] = result;
        return stackTop;
    }

    static RuntimeException notAFunction(Object notAFunction,
                                         InterpreterData idata, int namePC)
    {
        String debugName;
        if (notAFunction == Undefined.instance || notAFunction == null) {
            // special code for better error message for call
            // to undefined or null
            int index = getIndex(idata.itsICode, namePC);
            if (index == 0xFFFF) {
                debugName = "<unknown>";
            } else {
                debugName = idata.itsStringTable[index];
            }
        } else {
            debugName = ScriptRuntime.toString(notAFunction);
        }
        throw ScriptRuntime.notFunctionError(notAFunction, debugName);
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
        if (iCode[pc] != Icode_CATCH) Kit.codeBug();
        return pc;
    }

    private CompilerEnvirons compilerEnv;

    private boolean itsInFunctionFlag;

    private InterpreterData itsData;
    private ScriptOrFnNode scriptOrFn;
    private int itsICodeTop;
    private int itsStackDepth;
    private int itsWithDepth;
    private int itsLineNumber;
    private int itsDoubleTableTop;
    private ObjToIntMap itsStrings = new ObjToIntMap(20);
    private int itsLastStringIndex;
    private int itsLocalTop;

    private static final int MIN_LABEL_TABLE_SIZE = 32;
    private static final int MIN_FIXUP_TABLE_SIZE = 40;
    private int[] itsLabelTable;
    private int itsLabelTableTop;
// itsFixupTable[i] = (label_index << 32) | fixup_site
    private long[] itsFixupTable;
    private int itsFixupTableTop;
    private ObjArray itsLiteralIds = new ObjArray();

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
