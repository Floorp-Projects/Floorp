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
        Icode_GOSUB                     = -22,
        Icode_RETSUB                    = -23,

    // To indicating a line number change in icodes.
        Icode_LINE                      = -24,

    // To store shorts and ints inline
        Icode_SHORTNUMBER               = -25,
        Icode_INTNUMBER                 = -26,

    // To create and populate array to hold values for [] and {} literals
        Icode_LITERAL_NEW               = -27,
        Icode_LITERAL_SET               = -28,

    // Array literal with skipped index like [1,,2]
        Icode_SPARE_ARRAYLIT            = -29,

    // Load index register to prepare for the following index operation
        Icode_REG_IND_C0                = -30,
        Icode_REG_IND_C1                = -31,
        Icode_REG_IND_C2                = -32,
        Icode_REG_IND_C3                = -33,
        Icode_REG_IND_C4                = -34,
        Icode_REG_IND_C5                = -35,
        Icode_REG_IND1                  = -36,
        Icode_REG_IND2                  = -37,
        Icode_REG_IND4                  = -38,

    // Load string register to prepare for the following string operation
        Icode_REG_STR_C0                = -39,
        Icode_REG_STR_C1                = -40,
        Icode_REG_STR_C2                = -41,
        Icode_REG_STR_C3                = -42,
        Icode_REG_STR1                  = -43,
        Icode_REG_STR2                  = -44,
        Icode_REG_STR4                  = -45,

    // Version of getvar/setvar that read var index directly from bytecode
        Icode_GETVAR1                   = -46,
        Icode_SETVAR1                   = -47,

    // Load unefined
        Icode_UNDEF                     = -48,
        Icode_ZERO                      = -49,
        Icode_ONE                       = -50,

    // entrance and exit from .()
       Icode_ENTERDQ                    = -51,
       Icode_LEAVEDQ                    = -52,

       Icode_TAIL_CALL                  = -53,

    // Last icode
        MIN_ICODE                       = -53;

    // data for parsing

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

// ECF_ or Expression Context Flags constants: for now only TAIL is available
    private static final int ECF_TAIL = 1 << 0;

    private static final Object DBL_MRK = new Object();


    private static class State
    {
        State callerState;

        InterpretedFunction fnOrScript;
        InterpreterData idata;

// Stack structure
// stack[0 <= i < localShift]: arguments and local variables
// stack[localShift <= i <= emptyStackTop]: used for local temporaries
// stack[emptyStackTop < i < stack.length]: stack data
// sDbl[i]: if stack[i] is DBL_MRK, sDbl[i] holds the number value

        Object[] stack;
        double[] sDbl;
        int localShift;
        int emptyStackTop;

        DebugFrame debuggerFrame;
        boolean useActivation;

        Scriptable thisObj;
        Scriptable[] scriptRegExps;

// The values that change during interpretation

        Object result;
        double resultDbl;
        int pc;
        int pcPrevBranch;
        int pcSourceLineStart;
        int withDepth;
        Scriptable scope;

        int savedStackTop;
        int savedCallOp;
    }

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
          case Icode_TAIL_CALL:        return "TAIL_CALL";
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

    public Object compile(CompilerEnvirons compilerEnv,
                          ScriptOrFnNode tree,
                          String encodedSource,
                          boolean returnFunction)
    {
        this.compilerEnv = compilerEnv;
        new NodeTransformer().transform(tree);

        if (Token.printTrees) {
            System.out.println(tree.toStringTree(tree));
        }

        if (returnFunction) {
            tree = tree.getFunctionNode(0);
        }

        scriptOrFn = tree;
        itsData = new InterpreterData(compilerEnv.getLanguageVersion(),
                                      scriptOrFn.getSourceName(),
                                      encodedSource);
        itsData.topLevel = true;

        if (returnFunction) {
            generateFunctionICode();
        } else {
            generateICodeFromTree(scriptOrFn);
        }

        return itsData;
    }

    public Script createScriptObject(Object bytecode,
                                     Object staticSecurityDomain)
    {
        InterpreterData idata = (InterpreterData)bytecode;
        return InterpretedFunction.createScript(itsData,
                                                staticSecurityDomain);
    }

    public Function createFunctionObject(Context cx, Scriptable scope,
                                         Object bytecode,
                                         Object staticSecurityDomain)
    {
        InterpreterData idata = (InterpreterData)bytecode;
        return InterpretedFunction.createFunction(cx, scope, itsData,
                                                  staticSecurityDomain);
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
            visitExpression(child, 0);
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
            visitExpression(child, 0);
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
            visitExpression(child, 0);
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
            visitExpression(child, 0);
            addToken(Token.THROW);
            addUint16(itsLineNumber);
            stackChange(-1);
            break;

          case Token.RETURN:
            updateLineNumber(node);
            if (child != null) {
                visitExpression(child, ECF_TAIL);
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
            visitExpression(child, 0);
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

    private void visitExpression(Node node, int contextFlags)
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

          case Token.COMMA: {
            Node lastChild = node.getLastChild();
            while (child != lastChild) {
                visitExpression(child, 0);
                addIcode(Icode_POP);
                stackChange(-1);
                child = child.getNext();
            }
            // Preserve tail context flag if any
            visitExpression(child, contextFlags & ECF_TAIL);
            break;
          }

          case Token.USE_STACK:
            // Indicates that stack was modified externally,
            // like placed catch object
            stackChange(1);
            break;

          case Token.CALL:
          case Token.NEW:
          case Token.REF_CALL:
            visitCall(node, contextFlags);
            break;

          case Token.AND:
          case Token.OR: {
            visitExpression(child, 0);
            addIcode(Icode_DUP);
            stackChange(1);
            int afterSecondJumpStart = itsICodeTop;
            int jump = (type == Token.AND) ? Token.IFNE : Token.IFEQ;
            addForwardGoto(jump);
            stackChange(-1);
            addIcode(Icode_POP);
            stackChange(-1);
            child = child.getNext();
            // Preserve tail context flag if any
            visitExpression(child, contextFlags & ECF_TAIL);
            resolveForwardGoto(afterSecondJumpStart);
            break;
          }

          case Token.HOOK: {
            Node ifThen = child.getNext();
            Node ifElse = ifThen.getNext();
            visitExpression(child, 0);
            int elseJumpStart = itsICodeTop;
            addForwardGoto(Token.IFNE);
            stackChange(-1);
            // Preserve tail context flag if any
            visitExpression(ifThen, contextFlags & ECF_TAIL);
            int afterElseJumpStart = itsICodeTop;
            addForwardGoto(Token.GOTO);
            resolveForwardGoto(elseJumpStart);
            itsStackDepth = savedStackDepth;
            // Preserve tail context flag if any
            visitExpression(ifElse, contextFlags & ECF_TAIL);
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
            visitExpression(child, 0);
            child = child.getNext();
            visitExpression(child, 0);
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
            visitExpression(child, 0);
            if (type == Token.VOID) {
                addIcode(Icode_POP);
                addIcode(Icode_UNDEF);
            } else {
                addToken(type);
            }
            break;

        case Token.SETPROP:
          case Token.SETPROP_OP: {
            visitExpression(child, 0);
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
            visitExpression(child, 0);
            addStringOp(Token.SETPROP, property);
            stackChange(-1);
            break;
          }

          case Token.SETELEM:
          case Token.SETELEM_OP:
            visitExpression(child, 0);
            child = child.getNext();
            visitExpression(child, 0);
            child = child.getNext();
            if (type == Token.SETELEM_OP) {
                addIcode(Icode_DUP2);
                stackChange(2);
                addToken(Token.GETELEM);
                stackChange(-1);
                // Compensate for the following USE_STACK
                stackChange(-1);
            }
            visitExpression(child, 0);
            addToken(Token.SETELEM);
            stackChange(-2);
            break;

          case Token.SET_REF:
          case Token.SET_REF_OP:
            visitExpression(child, 0);
            child = child.getNext();
            if (type == Token.SET_REF_OP) {
                addIcode(Icode_DUP);
                stackChange(1);
                addToken(Token.GET_REF);
                // Compensate for the following USE_STACK
                stackChange(-1);
            }
            visitExpression(child, 0);
            addToken(Token.SET_REF);
            stackChange(-1);
            break;

          case Token.SETNAME: {
            String name = child.getString();
            visitExpression(child, 0);
            child = child.getNext();
            visitExpression(child, 0);
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
            visitExpression(child, 0);
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
                visitExpression(node, 0);
            } else {
                String name = child.getString();
                child = child.getNext();
                visitExpression(child, 0);
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
            visitExpression(child, 0);
            String special = (String)node.getProp(Node.SPECIAL_PROP_PROP);
            addStringOp(Token.SPECIAL_REF, special);
            break;
          }

          case Token.XML_REF:
            visitExpression(child, 0);
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
            visitExpression(child, 0);
            addToken(type);
            break;

          case Token.COLONCOLON : {
            if (child.getType() != Token.STRING)
                throw badTree(child);
            String namespace = child.getString();
            child = child.getNext();
            visitExpression(child, 0);
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
        // See comments in IRFactory.createSwitch() for description
        // of SWITCH node

        updateLineNumber(switchNode);

        Node child = switchNode.getFirstChild();
        visitExpression(child, 0);
        for (Node.Jump caseNode = (Node.Jump)child.getNext();
             caseNode != null;
             caseNode = (Node.Jump)caseNode.getNext())
        {
            if (caseNode.getType() != Token.CASE)
                throw badTree(caseNode);
            Node test = caseNode.getFirstChild();
            addIcode(Icode_DUP);
            stackChange(1);
            visitExpression(test, 0);
            addToken(Token.SHEQ);
            stackChange(-1);
            // If true, Icode_IFEQ_POP will jump and remove case value
            // from stack
            addGoto(caseNode.target, Icode_IFEQ_POP);
            stackChange(-1);
        }
        addIcode(Icode_POP);
        stackChange(-1);
    }

    private void visitCall(Node node, int contextFlags)
    {
        int type = node.getType();
        Node child = node.getFirstChild();
        if (type == Token.NEW) {
            visitExpression(child, 0);
        } else {
            generateCallFunAndThis(child);
        }
        int argCount = 0;
        while ((child = child.getNext()) != null) {
            visitExpression(child, 0);
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
            if (type == Token.CALL) {
                if ((contextFlags & ECF_TAIL) != 0) {
                    type = Icode_TAIL_CALL;
                }
            }
            addIndexOp(type, argCount);
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
            visitExpression(target, 0);
            Node id = target.getNext();
            if (type == Token.GETPROP) {
                String property = id.getString();
                // stack: ... target -> ... function thisObj
                addStringOp(Icode_PROP_AND_THIS, property);
                stackChange(1);
            } else {
                visitExpression(id, 0);
                // stack: ... target id -> ... function thisObj
                addIcode(Icode_ELEM_AND_THIS);
            }
            break;
          }
          default:
            // Including Token.GETVAR
            visitExpression(left, 0);
            // stack: ... value -> ... function thisObj
            addIcode(Icode_VALUE_AND_THIS);
            stackChange(1);
            break;
        }
    }

    private void visitGetProp(Node node, Node child)
    {
        visitExpression(child, 0);
        child = child.getNext();
        String property = child.getString();
        addStringOp(Token.GETPROP, property);
    }

    private void visitGetElem(Node node, Node child)
    {
        visitExpression(child, 0);
        child = child.getNext();
        visitExpression(child, 0);
        addToken(Token.GETELEM);
        stackChange(-1);
    }

    private void visitGetRef(Node node, Node child)
    {
        visitExpression(child, 0);
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
            visitExpression(object, 0);
            String property = object.getNext().getString();
            addStringOp(Icode_PROP_INC_DEC, property);
            addUint8(incrDecrMask);
            break;
          }
          case Token.GETELEM : {
            Node object = child.getFirstChild();
            visitExpression(object, 0);
            Node index = object.getNext();
            visitExpression(index, 0);
            addIcode(Icode_ELEM_INC_DEC);
            addUint8(incrDecrMask);
            stackChange(-1);
            break;
          }
          case Token.GET_REF : {
            Node ref = child.getFirstChild();
            visitExpression(ref, 0);
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
            visitExpression(child, 0);
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
        visitExpression(child, 0);
        addIcode(Icode_ENTERDQ);
        stackChange(-1);
        int queryPC = itsICodeTop;
        visitExpression(child.getNext(), 0);
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
              case Icode_TAIL_CALL :
              case Token.REF_CALL :
              case Token.NEW :
                out.println(tname+' '+indexReg);
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
        State state = (State)cx.interpreterLineCounting;
        InterpreterData idata = state.idata;
        if (state.pcSourceLineStart >= 0) {
            linep[0] = getShort(idata.itsICode, state.pcSourceLineStart);
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

    private static void initFunction(Context cx, Scriptable scope,
                                     InterpretedFunction parent, int index)
    {
        InterpretedFunction fn;
        fn = InterpretedFunction.createFunction(cx, scope, parent, index);
        ScriptRuntime.initFunction(cx, scope, fn, fn.idata.itsFunctionType,
                                   parent.evalScriptFlag);
    }

    static Object interpret(InterpretedFunction ifun,
                            Context cx, Scriptable scope,
                            Scriptable thisObj, Object[] args)
    {
        if (!ScriptRuntime.hasTopCall(cx)) {
            return ScriptRuntime.doTopCall(ifun, cx, scope, thisObj, args);
        }
        if (cx.interpreterSecurityDomain != ifun.securityDomain) {
            Object savedDomain = cx.interpreterSecurityDomain;
            cx.interpreterSecurityDomain = ifun.securityDomain;
            try {
                return ifun.securityController.callWithDomain(
                    ifun.securityDomain, cx, ifun, scope, thisObj, args);
            } finally {
                cx.interpreterSecurityDomain = savedDomain;
            }
        }

        State state = new State();
        initState(cx, scope, thisObj, args, null, 0, args.length,
                  ifun, null, state);
        try {
            return interpret(cx, state);
        } finally {
            // Always clenup interpreterLineCounting to avoid memory leaks
            // throgh stored in Context state
            cx.interpreterLineCounting = null;
        }
    }

    private static Object interpret(Context cx, State state)
    {
        final Object DBL_MRK = Interpreter.DBL_MRK;
        final Scriptable undefined = Undefined.instance;
        // arbitrary number to add to instructionCount when calling
        // other functions
        final int INVOCATION_COST = 100;

        String stringReg = null;
        int indexReg = 0;
        final boolean instructionCounting = (cx.instructionThreshold != 0);

        StateLoop: for (;;) {
            // Use local variables for constant values in state
            // for faster access
            Object[] stack = state.stack;
            double[] sDbl = state.sDbl;
            byte[] iCode = state.idata.itsICode;
            String[] strings = state.idata.itsStringTable;
            boolean useActivation = state.useActivation;

            // Use local for stackTop as well. Since execption handlers
            // can only exist at statement level where stack is empty,
            // it is necessary to save/restore stackTop only accross
            // function calls and normal returns.
            int stackTop = state.savedStackTop;

            // Point line counting to the new state
            cx.interpreterLineCounting = state;

            Loop: for (;;) {

                // Exception object to rethrow or catch
                Throwable throwable;

                withoutExceptions: try {
                    int op = iCode[state.pc++];
                    jumplessRun: {

    // Back indent to ease imlementation reading
switch (op) {
    case Token.THROW: {
        Object value = stack[stackTop];
        if (value == DBL_MRK) value = doubleWrap(sDbl[stackTop]);
        --stackTop;

        int sourceLine = getShort(iCode, state.pc);
        throwable = new JavaScriptException(value,
                                            state.idata.itsSourceFile,
                                            sourceLine);
        break withoutExceptions;
    }
    case Token.GE :
    case Token.LE :
    case Token.GT :
    case Token.LT :
        --stackTop;
        do_cmp(state, stackTop, op);
        continue Loop;
    case Token.IN : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        boolean valBln = ScriptRuntime.in(lhs, rhs, cx, state.scope);
        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
        continue Loop;
    }
    case Token.INSTANCEOF : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        boolean valBln = ScriptRuntime.instanceOf(lhs, rhs, cx, state.scope);
        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
        continue Loop;
    }
    case Token.EQ :
    case Token.NE :
        --stackTop;
        do_eq(state, stackTop, op);
        continue Loop;
    case Token.SHEQ :
    case Token.SHNE :
        --stackTop;
        do_sheq(state, stackTop, op);
        continue Loop;
    case Token.IFNE :
        if (stack_boolean(state, stackTop--)) {
            state.pc += 2;
            continue Loop;
        }
        break jumplessRun;
    case Token.IFEQ :
        if (!stack_boolean(state, stackTop--)) {
            state.pc += 2;
            continue Loop;
        }
        break jumplessRun;
    case Icode_IFEQ_POP :
        if (!stack_boolean(state, stackTop--)) {
            state.pc += 2;
            continue Loop;
        }
        stack[stackTop--] = null;
        break jumplessRun;
    case Token.GOTO :
        break jumplessRun;
    case Icode_GOSUB :
        ++stackTop;
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = state.pc + 2;
        break jumplessRun;
    case Icode_RETSUB : {
        // indexReg: local to store return address
        if (instructionCounting) {
            addInstructionCount(cx, state, 0);
        }
        indexReg += state.localShift;
        Object value = stack[indexReg];
        if (value != DBL_MRK) {
            // Invocation from exception handler, restore object to rethrow
            throwable = (Throwable)value;
            break withoutExceptions;
        }
        // Normal return from GOSUB
        state.pc = (int)sDbl[indexReg];
        if (instructionCounting) {
            state.pcPrevBranch = state.pc;
        }
        continue Loop;
    }
    case Icode_POP :
        stack[stackTop] = null;
        stackTop--;
        continue Loop;
    case Icode_POP_RESULT :
        state.result = stack[stackTop];
        state.resultDbl = sDbl[stackTop];
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
        state.result = stack[stackTop];
        state.resultDbl = sDbl[stackTop];
        --stackTop;
        break Loop;
    case Token.RETURN_RESULT :
        break Loop;
    case Icode_RETUNDEF :
        state.result = undefined;
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
        double lDbl = stack_double(state, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = ScriptRuntime.toUint32(lDbl) >>> rIntValue;
        continue Loop;
    }
    case Token.NEG : {
        double rDbl = stack_double(state, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = -rDbl;
        continue Loop;
    }
    case Token.POS : {
        double rDbl = stack_double(state, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = rDbl;
        continue Loop;
    }
    case Token.ADD :
        stackTop = do_add(stack, sDbl, stackTop, cx);
        continue Loop;
    case Token.SUB : {
        double rDbl = stack_double(state, stackTop);
        --stackTop;
        double lDbl = stack_double(state, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lDbl - rDbl;
        continue Loop;
    }
    case Token.MUL : {
        double rDbl = stack_double(state, stackTop);
        --stackTop;
        double lDbl = stack_double(state, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lDbl * rDbl;
        continue Loop;
    }
    case Token.DIV : {
        double rDbl = stack_double(state, stackTop);
        --stackTop;
        double lDbl = stack_double(state, stackTop);
        stack[stackTop] = DBL_MRK;
        // Detect the divide by zero or let Java do it ?
        sDbl[stackTop] = lDbl / rDbl;
        continue Loop;
    }
    case Token.MOD : {
        double rDbl = stack_double(state, stackTop);
        --stackTop;
        double lDbl = stack_double(state, stackTop);
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = lDbl % rDbl;
        continue Loop;
    }
    case Token.NOT :
        stack[stackTop] = stack_boolean(state, stackTop)
                          ? Boolean.FALSE : Boolean.TRUE;
        continue Loop;
    case Token.BINDNAME :
        stack[++stackTop] = ScriptRuntime.bind(cx, state.scope, stringReg);
        continue Loop;
    case Token.SETNAME : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Scriptable lhs = (Scriptable)stack[stackTop];
        stack[stackTop] = ScriptRuntime.setName(lhs, rhs, cx,
                                                state.scope, stringReg);
        continue Loop;
    }
    case Token.DELPROP : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.delete(cx, state.scope, lhs, rhs);
        continue Loop;
    }
    case Token.GETPROP : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.getObjectProp(lhs, stringReg,
                                                      cx, state.scope);
        continue Loop;
    }
    case Token.SETPROP : {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.setObjectProp(lhs, stringReg, rhs,
                                                      cx, state.scope);
        continue Loop;
    }
    case Icode_PROP_INC_DEC : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.propIncrDecr(lhs, stringReg,
                                                     state.scope,
                                                     iCode[state.pc]);
        ++state.pc;
        continue Loop;
    }
    case Token.GETELEM :
        --stackTop;
        do_getElem(state, stackTop, cx);
        continue Loop;
    case Token.SETELEM :
        stackTop -= 2;
        do_setElem(state, stackTop, cx);
        continue Loop;
    case Icode_ELEM_INC_DEC: {
        Object rhs = stack[stackTop];
        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.elemIncrDecr(lhs, rhs, cx, state.scope,
                                                     iCode[state.pc]);
        ++state.pc;
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
        stack[stackTop] = ScriptRuntime.referenceIncrDecr(lhs, iCode[state.pc]);
        ++state.pc;
        continue Loop;
    }
    case Token.LOCAL_SAVE :
        indexReg += state.localShift;
        stack[indexReg] = stack[stackTop];
        sDbl[indexReg] = sDbl[stackTop];
        --stackTop;
        continue Loop;
    case Token.LOCAL_LOAD :
        ++stackTop;
        indexReg += state.localShift;
        stack[stackTop] = stack[indexReg];
        sDbl[stackTop] = sDbl[indexReg];
        continue Loop;
    case Icode_NAME_AND_THIS :
        // stringReg: name
        ++stackTop;
        stack[stackTop] = ScriptRuntime.getNameFunctionAndThis(stringReg,
                                                               cx, state.scope);
        ++stackTop;
        stack[stackTop] = ScriptRuntime.lastStoredScriptable(cx);
        continue Loop;
    case Icode_PROP_AND_THIS: {
        Object obj = stack[stackTop];
        if (obj == DBL_MRK) obj = doubleWrap(sDbl[stackTop]);
        // stringReg: property
        stack[stackTop] = ScriptRuntime.getPropFunctionAndThis(obj, stringReg,
                                                               cx, state.scope);
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
                                                                   cx,
                                                                   state.scope);
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
        int callType = iCode[state.pc] & 0xFF;
        boolean isNew =  (iCode[state.pc + 1] != 0);
        int sourceLine = getShort(iCode, state.pc + 2);

        // indexReg: number of arguments
        if (isNew) {
            // stack change: function arg0 .. argN -> newResult
            stackTop -= indexReg;

            Object function = stack[stackTop];
            if (function == DBL_MRK) function = doubleWrap(sDbl[stackTop]);
            Object[] outArgs = getArgsArray(
                                   stack, sDbl, stackTop + 1, indexReg);
            stack[stackTop] = ScriptRuntime.newSpecial(
                                  cx, function, outArgs, state.scope, callType);
        } else {
            // stack change: function thisObj arg0 .. argN -> result
            stackTop -= 1 + indexReg;

            // Call code generation ensure that stack here
            // is ... Function Scriptable
            Scriptable functionThis = (Scriptable)stack[stackTop + 1];
            Function function = (Function)stack[stackTop];
            Object[] outArgs = getArgsArray(
                                   stack, sDbl, stackTop + 2, indexReg);
            stack[stackTop] = ScriptRuntime.callSpecial(
                                  cx, function, functionThis, outArgs,
                                  state.scope, state.thisObj, callType,
                                  state.idata.itsSourceFile, sourceLine);
        }
        state.pc += 4;
        continue Loop;
    }
    case Token.CALL :
    case Icode_TAIL_CALL : {
        if (instructionCounting) {
            cx.instructionCount += INVOCATION_COST;
        }
        // stack change: function thisObj arg0 .. argN -> result
        // indexReg: number of arguments
        stackTop -= 1 + indexReg;

        // CALL generation ensures that funThisObj and fun
        // are already Scriptable and Function objects respectively
        Scriptable funThisObj = (Scriptable)stack[stackTop + 1];
        Function fun = (Function)stack[stackTop];

        Scriptable funScope = state.scope;
        if (useActivation) {
            funScope = ScriptableObject.getTopLevelScope(state.scope);
        }

        if (fun instanceof InterpretedFunction) {
            // Inlining of InterpretedFunction.call not to create
            // argument array
            InterpretedFunction ifun = (InterpretedFunction)fun;
            if (state.fnOrScript.securityDomain == ifun.securityDomain) {
                State callParentState = state;
                State calleeState = new State();
                if (op == Icode_TAIL_CALL) {
                    // In principle tail call can re-use the current
                    // state and its stack arrays but it is hard to
                    // do properly. Any exceptions that can legally
                    // happen during state re-initialization including
                    // StackOverflowException during innocent looking
                    // System.arraycopy may leave the current state
                    // data corrupted leading to undefined behaviour
                    // in the catch code bellow that unwinds JS stack
                    // on exceptions. Then there is issue about state release
                    // end exceptions there.
                    // To avoid state allocation a released state
                    // can be cached for re-use which would also benefit
                    // non-tail calls but it is not clear that this caching
                    // would gain in performance due to potentially
                    // bad iteraction with GC.
                    callParentState = state.callerState;
                }
                initState(cx, funScope, funThisObj, stack, sDbl,
                          stackTop + 2, indexReg, ifun, callParentState,
                          calleeState);
                if (op == Icode_TAIL_CALL) {
                    // Release the parent
                    releaseState(cx, state, null);
                } else {
                    state.savedStackTop = stackTop;
                    state.savedCallOp = op;
                }
                state = calleeState;
                continue StateLoop;
            }
        }
        Object[] outArgs = getArgsArray(stack, sDbl, stackTop + 2, indexReg);
        stack[stackTop] = fun.call(cx, funScope, funThisObj, outArgs);
        continue Loop;
    }
    case Token.REF_CALL : {
        if (instructionCounting) {
            cx.instructionCount += INVOCATION_COST;
        }
        // stack change: function thisObj arg0 .. argN -> reference
        // indexReg: number of arguments
        stackTop -= indexReg + 1;

        // REF_CALL generation ensures that funThisObj and fun
        // are already Scriptable and Function objects respectively
        Scriptable funThisObj = (Scriptable)stack[stackTop + 1];
        Function fun = (Function)stack[stackTop];

        Scriptable funScope = state.scope;
        if (useActivation) {
            funScope = ScriptableObject.getTopLevelScope(state.scope);
        }
        Object[] outArgs = getArgsArray(stack, sDbl, stackTop + 2, indexReg);
        stack[stackTop] = ScriptRuntime.referenceCall(fun, funThisObj, outArgs,
                                                      cx, funScope);
        continue Loop;
    }
    case Token.NEW : {
        if (instructionCounting) {
            cx.instructionCount += INVOCATION_COST;
        }
        // stack change: function arg0 .. argN -> newResult
        // indexReg: number of arguments
        stackTop -= indexReg;
        Object lhs = stack[stackTop];

        if (lhs instanceof InterpretedFunction) {
            // Inlining of InterpretedFunction.construct not to create
            // argument array
            InterpretedFunction f = (InterpretedFunction)lhs;
            if (state.fnOrScript.securityDomain == f.securityDomain) {
                Scriptable newInstance = f.createObject(cx, state.scope);
                State calleeState = new State();
                initState(cx, state.scope, newInstance, stack, sDbl,
                          stackTop + 1, indexReg, f, state,
                          calleeState);

                stack[stackTop] = newInstance;
                state.savedStackTop = stackTop;
                state.savedCallOp = op;
                state = calleeState;
                continue StateLoop;
            }
        }
        if (!(lhs instanceof Function)) {
            if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
            throw ScriptRuntime.notFunctionError(lhs);
        }
        Function f = (Function)lhs;
        Object[] outArgs = getArgsArray(stack, sDbl, stackTop + 1, indexReg);
        stack[stackTop] = f.construct(cx, state.scope, outArgs);
        continue Loop;
    }
    case Token.TYPEOF : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.typeof(lhs);
        continue Loop;
    }
    case Icode_TYPEOFNAME :
        stack[++stackTop] = ScriptRuntime.typeofName(state.scope, stringReg);
        continue Loop;
    case Token.STRING :
        stack[++stackTop] = stringReg;
        continue Loop;
    case Icode_SHORTNUMBER :
        ++stackTop;
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = getShort(iCode, state.pc);
        state.pc += 2;
        continue Loop;
    case Icode_INTNUMBER :
        ++stackTop;
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = getInt(iCode, state.pc);
        state.pc += 4;
        continue Loop;
    case Token.NUMBER :
        ++stackTop;
        stack[stackTop] = DBL_MRK;
        sDbl[stackTop] = state.idata.itsDoubleTable[indexReg];
        continue Loop;
    case Token.NAME :
        stack[++stackTop] = ScriptRuntime.name(cx, state.scope, stringReg);
        continue Loop;
    case Icode_NAME_INC_DEC :
        stack[++stackTop] = ScriptRuntime.nameIncrDecr(state.scope, stringReg,
                                                       iCode[state.pc]);
        ++state.pc;
        continue Loop;
    case Icode_SETVAR1:
        indexReg = iCode[state.pc++];
        // fallthrough
    case Token.SETVAR :
        if (!useActivation) {
            stack[indexReg] = stack[stackTop];
            sDbl[indexReg] = sDbl[stackTop];
        } else {
            Object val = stack[stackTop];
            if (val == DBL_MRK) val = doubleWrap(sDbl[stackTop]);
            activationPut(state, indexReg, val);
        }
        continue Loop;
    case Icode_GETVAR1:
        indexReg = iCode[state.pc++];
        // fallthrough
    case Token.GETVAR :
        ++stackTop;
        if (!useActivation) {
            stack[stackTop] = stack[indexReg];
            sDbl[stackTop] = sDbl[indexReg];
        } else {
            stack[stackTop] = activationGet(state, indexReg);
        }
        continue Loop;
    case Icode_VAR_INC_DEC : {
        // indexReg : varindex
        ++stackTop;
        int incrDecrMask = iCode[state.pc];
        if (!useActivation) {
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
            String varName = state.fnOrScript.argNames[indexReg];
            stack[stackTop] = ScriptRuntime.nameIncrDecr(state.scope, varName,
                                                         incrDecrMask);
        }
        ++state.pc;
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
        stack[++stackTop] = state.thisObj;
        continue Loop;
    case Token.THISFN :
        stack[++stackTop] = state.fnOrScript;
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
        state.scope = ScriptRuntime.enterWith(lhs, state.scope);
        ++state.withDepth;
        continue Loop;
    }
    case Token.LEAVEWITH :
        state.scope = ScriptRuntime.leaveWith(state.scope);
        --state.withDepth;
        continue Loop;
    case Token.CATCH_SCOPE :
        stack[stackTop] = ScriptRuntime.newCatchScope(stringReg,
                                                      stack[stackTop]);
        continue Loop;
    case Token.ENUM_INIT_KEYS : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        indexReg += state.localShift;
        stack[indexReg] = ScriptRuntime.enumInit(lhs, state.scope);
        continue Loop;
    }
    case Token.ENUM_INIT_VALUES : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        indexReg += state.localShift;
        stack[indexReg] = ScriptRuntime.enumValuesInit(lhs, state.scope);
        continue Loop;
    }
    case Token.ENUM_NEXT :
    case Token.ENUM_ID : {
        indexReg += state.localShift;
        Object val = stack[indexReg];
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
                                                         cx, state.scope);
        continue Loop;
    }
    case Token.XML_REF : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        stack[stackTop] = ScriptRuntime.xmlReference(lhs, cx, state.scope);
        continue Loop;
    }
    case Icode_SCOPE :
        stack[++stackTop] = state.scope;
        continue Loop;
    case Icode_CLOSURE_EXPR :
        stack[++stackTop] = InterpretedFunction.createFunction(cx, state.scope,
                                                               state.fnOrScript,
                                                               indexReg);
        continue Loop;
    case Icode_CLOSURE_STMT :
        initFunction(cx, state.scope, state.fnOrScript, indexReg);
        continue Loop;
    case Token.REGEXP :
        stack[++stackTop] = state.scriptRegExps[indexReg];
        continue Loop;
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
            Object[] ids = (Object[])state.idata.literalIds[indexReg];
            val = ScriptRuntime.newObjectLiteral(ids, data, cx, state.scope);
        } else {
            int[] skipIndexces = null;
            if (op == Icode_SPARE_ARRAYLIT) {
                skipIndexces = (int[])state.idata.literalIds[indexReg];
            }
            val = ScriptRuntime.newArrayLiteral(data, skipIndexces, cx,
                                                state.scope);
        }
        stack[stackTop] = val;
        continue Loop;
    }
    case Icode_ENTERDQ : {
        Object lhs = stack[stackTop];
        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
        --stackTop;
        state.scope = ScriptRuntime.enterDotQuery(lhs, state.scope);
        ++state.withDepth;
        continue Loop;
    }
    case Icode_LEAVEDQ : {
        boolean valBln = stack_boolean(state, stackTop);
        Object x = ScriptRuntime.updateDotQuery(valBln, state.scope);
        if (x != null) {
            stack[stackTop] = x;
            state.scope = ScriptRuntime.leaveDotQuery(state.scope);
            --state.withDepth;
            state.pc += 2;
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
                                                        cx, state.scope);
        continue Loop;
    }
    case Icode_LINE :
        state.pcSourceLineStart = state.pc;
        if (state.debuggerFrame != null) {
            int line = getShort(iCode, state.pc);
            state.debuggerFrame.onLineChange(cx, line);
        }
        state.pc += 2;
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
        indexReg = 0xFF & iCode[state.pc];
        ++state.pc;
        continue Loop;
    case Icode_REG_IND2:
        indexReg = getIndex(iCode, state.pc);
        state.pc += 2;
        continue Loop;
    case Icode_REG_IND4:
        indexReg = getInt(iCode, state.pc);
        state.pc += 4;
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
        stringReg = strings[0xFF & iCode[state.pc]];
        ++state.pc;
        continue Loop;
    case Icode_REG_STR2:
        stringReg = strings[getIndex(iCode, state.pc)];
        state.pc += 2;
        continue Loop;
    case Icode_REG_STR4:
        stringReg = strings[getInt(iCode, state.pc)];
        state.pc += 4;
        continue Loop;
    default :
        dumpICode(state.idata);
        throw new RuntimeException(
            "Unknown icode : "+op+" @ pc : "+(state.pc-1));
}  // end of interpreter switch

                    } // end of jumplessRun label block

                    // This should be reachable only for jump implementation
                    // when pc points to encoded target offset
                    if (instructionCounting) {
                        addInstructionCount(cx, state, 2);
                    }
                    int offset = getShort(iCode, state.pc);
                    if (offset != 0) {
                        // -1 accounts for pc pointing to jump opcode + 1
                        state.pc += offset - 1;
                    } else {
                        state.pc = state.idata.longJumps.
                                       getExistingInt(state.pc);
                    }
                    if (instructionCounting) {
                        state.pcPrevBranch = state.pc;
                    }
                    continue Loop;

                }  // end of interpreter withoutExceptions: try
                catch (Throwable ex) {
                    throwable = ex;
                }

                // This should be reachable only after above catch or from
                // finally when it needs to propagate exception or from
                // explicit throw
                if (throwable == null) Kit.codeBug();

                state = handleException(cx, state, throwable);
                continue StateLoop;

            } // end of Loop: for

            releaseState(cx, state, null);
            if (state.callerState != null) {
                Object calleeResult = state.result;
                double calleeResultDbl = state.resultDbl;
                state = state.callerState;

                if (state.savedCallOp == Token.CALL) {
                    state.stack[state.savedStackTop] = calleeResult;
                    state.sDbl[state.savedStackTop] = calleeResultDbl;
                } else if (state.savedCallOp == Token.NEW) {
                    // If construct returns scriptable,
                    // then it replaces on stack top saved original instance
                    // of the object.
                    if (calleeResult instanceof Scriptable
                        && calleeResult != undefined)
                    {
                        state.stack[state.savedStackTop] = calleeResult;
                    }
                } else {
                    Kit.codeBug();
                }
                state.savedCallOp = 0;
                continue StateLoop;
            }

            return (state.result != DBL_MRK)
                ? state.result : doubleWrap(state.resultDbl);

        } // end of StateLoop: for(;;)
    }

    private static void initState(Context cx, Scriptable callerScope,
                                  Scriptable thisObj,
                                  Object[] args, double[] argsDbl,
                                  int argShift, int argCount,
                                  InterpretedFunction fnOrScript,
                                  State callerState, State state)
    {
        InterpreterData idata = fnOrScript.idata;

        boolean useActivation = idata.itsNeedsActivation;
        DebugFrame debuggerFrame = null;
        if (cx.debugger != null) {
            debuggerFrame = cx.debugger.getFrame(cx, idata);
            if (debuggerFrame != null) {
                useActivation = true;
            }
        }

        if (useActivation) {
            // Copy args to new array to pass to enterActivationFunction
            // or debuggerFrame.onEnter
            if (argsDbl != null) {
                args = getArgsArray(args, argsDbl, argShift, argCount);
            }
            argShift = 0;
            argsDbl = null;
        }

        Scriptable scope;
        if (idata.itsFunctionType != 0) {
            if (!idata.useDynamicScope) {
                scope = fnOrScript.getParentScope();
            } else {
                scope = callerScope;
            }

            if (useActivation) {
                scope = ScriptRuntime.enterActivationFunction(cx, scope,
                                                              fnOrScript,
                                                              thisObj, args);
            }
        } else {
            scope = callerScope;
            ScriptRuntime.initScript(fnOrScript, thisObj, cx, scope,
                                     fnOrScript.evalScriptFlag);
        }

        if (idata.itsNestedFunctions != null) {
            if (idata.itsFunctionType != 0 && !idata.itsNeedsActivation)
                Kit.codeBug();
            for (int i = 0; i < idata.itsNestedFunctions.length; i++) {
                InterpreterData fdata = idata.itsNestedFunctions[i];
                if (fdata.itsFunctionType == FunctionNode.FUNCTION_STATEMENT) {
                    initFunction(cx, scope, fnOrScript, i);
                }
            }
        }

        Scriptable[] scriptRegExps = null;
        if (idata.itsRegExpLiterals != null) {
            // Wrapped regexps for functions are stored in InterpretedFunction
            // but for script which should not contain references to scope
            // the regexps re-wrapped during each script execution
            if (idata.itsFunctionType != 0) {
                scriptRegExps = fnOrScript.functionRegExps;
            } else {
                scriptRegExps = fnOrScript.createRegExpWraps(cx, scope);
            }
        }

        if (debuggerFrame != null) {
            debuggerFrame.onEnter(cx, scope, thisObj, args);
        }

        // Initialize args, vars, locals and stack

        int emptyStackTop = idata.itsMaxVars + idata.itsMaxLocals - 1;
        int maxFrameArray = idata.itsMaxFrameArray;
        if (maxFrameArray != emptyStackTop + idata.itsMaxStack + 1)
            Kit.codeBug();

        Object[] stack;
        double[] sDbl;
        boolean stackReuse;
        if (state.stack != null && maxFrameArray <= state.stack.length) {
            // Reuse stacks from old state
            stackReuse = true;
            stack = state.stack;
            sDbl = state.sDbl;
        } else {
            stackReuse = false;
            stack = new Object[maxFrameArray];
            sDbl = new double[maxFrameArray];
        }

        int definedArgs = idata.argCount;
        if (definedArgs > argCount) { definedArgs = argCount; }

        // Fill the state structure

        state.callerState = callerState;
        state.fnOrScript = fnOrScript;
        state.idata = idata;

        state.stack = stack;
        state.sDbl = sDbl;
        state.localShift = idata.itsMaxVars;
        state.emptyStackTop = emptyStackTop;

        state.debuggerFrame = debuggerFrame;
        state.useActivation = useActivation;

        state.thisObj = thisObj;
        state.scriptRegExps = scriptRegExps;

        // Initialize initial values of variables that change during
        // interpretation.
        state.result = Undefined.instance;
        state.pc = 0;
        state.pcPrevBranch = 0;
        state.pcSourceLineStart = idata.firstLinePC;
        state.withDepth = 0;
        state.scope = scope;

        state.savedStackTop = emptyStackTop;
        state.savedCallOp = 0;

        System.arraycopy(args, argShift, stack, 0, definedArgs);
        if (argsDbl != null) {
            System.arraycopy(argsDbl, argShift, sDbl, 0, definedArgs);
        }
        for (int i = definedArgs; i != idata.itsMaxVars; ++i) {
            stack[i] = Undefined.instance;
        }
        if (stackReuse) {
            // Clean the stack part and space beyond stack if any
            // of the old array to allow to GC objects there
            for (int i = emptyStackTop + 1; i != stack.length; ++i) {
                stack[i] = null;
            }
        }
    }

    private static void releaseState(Context cx, State state,
                                     Throwable throwable)
    {
        if (state.debuggerFrame != null) {
            try {
                if (throwable != null) {
                    state.debuggerFrame.onExit(cx, true, throwable);
                } else {
                    state.debuggerFrame.onExit(cx, false, state.result);
                }
            } catch (Throwable ex) {
                System.err.println(
"RHINO USAGE WARNING: onExit terminated with exception");
                ex.printStackTrace(System.err);
            }
        }

        if (state.idata.itsFunctionType != 0) {
            if (state.useActivation) {
                ScriptRuntime.exitActivationFunction(cx);
            }
        }
    }

    private static State handleException(Context cx, State state,
                                         Throwable throwable)
    {
        // arbitrary exception cost for instruction counting
        final int EXCEPTION_COST = 100;
        final boolean instructionCounting = (cx.instructionThreshold != 0);

        // Exception type
        final int EX_CATCH_STATE = 2; // Can execute JS catch
        final int EX_FINALLY_STATE = 1; // Can execute JS finally
        final int EX_NO_JS_STATE = 0; // Terminate JS execution

        int exState;

        if (throwable instanceof JavaScriptException) {
            exState = EX_CATCH_STATE;
        } else if (throwable instanceof EcmaError) {
            // an offical ECMA error object,
            exState = EX_CATCH_STATE;
        } else if (throwable instanceof EvaluatorException) {
            exState = EX_CATCH_STATE;
        } else if (throwable instanceof RuntimeException) {
            exState = EX_FINALLY_STATE;
        } else {
            // Error instance
            exState = EX_NO_JS_STATE;
        }

        if (instructionCounting) {
            try {
                addInstructionCount(cx, state, EXCEPTION_COST);
            } catch (RuntimeException ex) {
                throwable = ex;
                exState = EX_FINALLY_STATE;
            } catch (Error ex) {
                // Error from instruction counting
                //     => unconditionally terminate JS
                throwable = ex;
                exState = EX_NO_JS_STATE;
            }
        }
        if (state.debuggerFrame != null
            && !(throwable instanceof Error))
        {
            try {
                state.debuggerFrame.onExceptionThrown(
                    cx, throwable);
            } catch (Throwable ex) {
                // Any exception from debugger
                //     => unconditionally terminate JS
                throwable = ex;
                exState = EX_NO_JS_STATE;
            }
        }

        int[] table;
        int handler;
        for (;;) {
            if (exState != EX_NO_JS_STATE) {
                table = state.idata.itsExceptionTable;
                handler = getExceptionHandler(table, state.pc);
                if (handler >= 0) {
                    if (table[handler + EXCEPTION_CATCH_SLOT] >= 0) {
                        // Found catch handler, check if it is allowed
                        // to run
                        if (exState == EX_CATCH_STATE) {
                            break;
                        }
                    }
                    if (table[handler + EXCEPTION_FINALLY_SLOT] >= 0) {
                        // Found finally handler, make state
                        // to match always handle type
                        exState = EX_FINALLY_STATE;
                        break;
                    }
                }
            }
            // No allowed execption handlers in this frame, unwind
            // to parent and try to look there

            while (state.withDepth != 0) {
                state.scope = ScriptRuntime.leaveWith(state.scope);
                --state.withDepth;
            }
            releaseState(cx, state, throwable);

            if (state.callerState == null) {
                // No more parent state frames, rethrow the exception.
                if (throwable instanceof RuntimeException) {
                    throw (RuntimeException)throwable;
                } else {
                    // Must be instance of Error or code bug
                    throw (Error)throwable;
                }
            }

            state = state.callerState;
        }

        // We caught an exception, since the loop above
        // can unwind accross frame boundary, always use
        // stack.something instead of something that were cached
        // at the beginning of StateLoop and refer to values in the
        // original frame

        int tryWithDepth = table[handler + EXCEPTION_WITH_DEPTH_SLOT];
        while (state.withDepth != tryWithDepth) {
            if (state.scope == null) Kit.codeBug();
            state.scope = ScriptRuntime.leaveWith(state.scope);
            --state.withDepth;
        }

        state.savedStackTop = state.emptyStackTop;
        if (exState == EX_CATCH_STATE) {
            int exLocal = table[handler + EXCEPTION_LOCAL_SLOT];
            state.stack[state.localShift + exLocal]
                = ScriptRuntime.getCatchObject(cx, state.scope,
                                               throwable);
            state.pc = table[handler + EXCEPTION_CATCH_SLOT];
        } else {
            ++state.savedStackTop;
            // Call finally handler with throwable on stack top to
            // distinguish from normal invocation through GOSUB
            // which would contain DBL_MRK on the stack
            state.stack[state.savedStackTop] = throwable;
            state.pc = table[handler + EXCEPTION_FINALLY_SLOT];
        }

        if (instructionCounting) {
            state.pcPrevBranch = state.pc;
        }

        return state;
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

    private static double stack_double(State state, int i)
    {
        Object x = state.stack[i];
        return (x != DBL_MRK) ? ScriptRuntime.toNumber(x) : state.sDbl[i];
    }

    private static boolean stack_boolean(State state, int i)
    {
        Object x = state.stack[i];
        if (x == Boolean.TRUE) {
            return true;
        } else if (x == Boolean.FALSE) {
            return false;
        } else if (x == DBL_MRK) {
            double d = state.sDbl[i];
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

    private static void do_cmp(State state, int i, int op)
    {
        Object rhs = state.stack[i + 1];
        Object lhs = state.stack[i];
        boolean result;
      object_compare:
        {
          number_compare:
            {
                double rDbl, lDbl;
                if (rhs == DBL_MRK) {
                    rDbl = state.sDbl[i + 1];
                    lDbl = stack_double(state, i);
                } else if (lhs == DBL_MRK) {
                    rDbl = ScriptRuntime.toNumber(rhs);
                    lDbl = state.sDbl[i];
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
        state.stack[i] = result ? Boolean.TRUE : Boolean.FALSE;
    }

    private static void do_eq(State state, int i, int op)
    {
        boolean result;
        Object rhs = state.stack[i + 1];
        Object lhs = state.stack[i];
        if (rhs == DBL_MRK) {
            if (lhs == DBL_MRK) {
                result = (state.sDbl[i] == state.sDbl[i + 1]);
            } else {
                result = ScriptRuntime.eqNumber(state.sDbl[i + 1], lhs);
            }
        } else {
            if (lhs == DBL_MRK) {
                result = ScriptRuntime.eqNumber(state.sDbl[i], rhs);
            } else {
                result = ScriptRuntime.eq(lhs, rhs);
            }
        }
        result ^= (op == Token.NE);
        state.stack[i] = (result) ? Boolean.TRUE : Boolean.FALSE;
    }

    private static void do_sheq(State state, int i, int op)
    {
        Object rhs = state.stack[i + 1];
        Object lhs = state.stack[i];
        boolean result;
      double_compare: {
            double rdbl, ldbl;
            if (rhs == DBL_MRK) {
                rdbl = state.sDbl[i + 1];
                if (lhs == DBL_MRK) {
                    ldbl = state.sDbl[i];
                } else if (lhs instanceof Number) {
                    ldbl = ((Number)lhs).doubleValue();
                } else {
                    result = false;
                    break double_compare;
                }
            } else if (lhs == DBL_MRK) {
                ldbl = state.sDbl[i];
                if (rhs == DBL_MRK) {
                    rdbl = state.sDbl[i + 1];
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
        state.stack[i] = (result) ? Boolean.TRUE : Boolean.FALSE;
    }

    private static void do_getElem(State state, int i, Context cx)
    {
        Object lhs = state.stack[i];
        if (lhs == DBL_MRK) lhs = doubleWrap(state.sDbl[i]);

        Object result;
        Object id = state.stack[i + 1];
        if (id != DBL_MRK) {
            result = ScriptRuntime.getObjectElem(lhs, id, cx, state.scope);
        } else {
            double val = state.sDbl[i + 1];
            if (lhs == null || lhs == Undefined.instance) {
                throw ScriptRuntime.undefReadError(
                          lhs, ScriptRuntime.toString(val));
            }
            Scriptable obj = (lhs instanceof Scriptable)
                             ? (Scriptable)lhs
                             : ScriptRuntime.toObject(cx, state.scope, lhs);
            int index = (int)val;
            if (index == val) {
                result = ScriptRuntime.getObjectIndex(obj, index, cx);
            } else {
                String s = ScriptRuntime.toString(val);
                result = ScriptRuntime.getObjectProp(obj, s, cx);
            }
        }
        state.stack[i] = result;
    }

    private static void do_setElem(State state, int i, Context cx)
    {
        Object rhs = state.stack[i + 2];
        if (rhs == DBL_MRK) rhs = doubleWrap(state.sDbl[i + 2]);
        Object lhs = state.stack[i];
        if (lhs == DBL_MRK) lhs = doubleWrap(state.sDbl[i]);

        Object result;
        Object id = state.stack[i + 1];
        if (id != DBL_MRK) {
            result = ScriptRuntime.setObjectElem(lhs, id, rhs, cx, state.scope);
        } else {
            double val = state.sDbl[i + 1];
            if (lhs == null || lhs == Undefined.instance) {
                throw ScriptRuntime.undefWriteError(
                          lhs, ScriptRuntime.toString(val), rhs);
            }
            Scriptable obj = (lhs instanceof Scriptable)
                             ? (Scriptable)lhs
                             : ScriptRuntime.toObject(cx, state.scope, lhs);
            int index = (int)val;
            if (index == val) {
                result = ScriptRuntime.setObjectIndex(obj, index, rhs, cx);
            } else {
                String s = ScriptRuntime.toString(val);
                result = ScriptRuntime.setObjectProp(obj, s, rhs, cx);
            }
        }
        state.stack[i] = result;
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

    private static Object activationGet(State state, int slot)
    {
        String name = state.fnOrScript.argNames[slot];
        Scriptable scope = state.scope;
        Object val = scope.get(name, scope);
    // Activation parameter or var is permanent
        if (val == Scriptable.NOT_FOUND) Kit.codeBug();
        return val;
    }

    private static void activationPut(State state, int slot, Object value)
    {
        String name = state.fnOrScript.argNames[slot];
        Scriptable scope = state.scope;
        scope.put(name, scope, value);
    }

    private static void addInstructionCount(Context cx, State state, int extra)
    {
        cx.instructionCount += state.pc - state.pcPrevBranch + extra;
        if (cx.instructionCount > cx.instructionThreshold) {
            cx.observeInstructionCount(cx.instructionCount);
            cx.instructionCount = 0;
        }
    }

}
