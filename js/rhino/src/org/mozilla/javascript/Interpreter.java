/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
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
import java.util.Vector;
import java.util.Enumeration;

import org.mozilla.javascript.debug.*;

public class Interpreter extends LabelTable {
    
    public static final boolean printICode = false;
    
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
                          SecuritySupport securitySupport,
                          ClassNameHelper nameHelper)
        throws IOException
    {
        version = cx.getLanguageVersion();
        itsData = new InterpreterData(0, 0, 0, securityDomain, 
                    cx.hasCompileFunctionsWithDynamicScope());
        if (tree instanceof FunctionNode) {
            FunctionNode f = (FunctionNode) tree;
            InterpretedFunction result = 
                generateFunctionICode(cx, scope, f, securityDomain);
            result.itsData.itsFunctionType = f.getFunctionType();
            createFunctionObject(result, scope);
            return result;
        }
        return generateScriptICode(cx, scope, tree, securityDomain);
    }
       
    private void generateICodeFromTree(Node tree, 
                                       VariableTable varTable, 
                                       boolean needsActivation,
                                       Object securityDomain)
    {
        int theICodeTop = 0;
        itsData.itsVariableTable = varTable;
        itsData.itsNeedsActivation = needsActivation;
        theICodeTop = generateICode(tree, theICodeTop);
        itsData.itsICodeTop = theICodeTop;
        if (itsEpilogLabel != -1)
            markLabel(itsEpilogLabel, theICodeTop);
        for (int i = 0; i < itsLabelTableTop; i++)
            itsLabelTable[i].fixGotos(itsData.itsICode);            
    }

    private Object[] generateRegExpLiterals(Context cx,
                                            Scriptable scope,
                                            Vector regexps)
    {
        Object[] result = new Object[regexps.size()];
        RegExpProxy rep = cx.getRegExpProxy();
        for (int i = 0; i < regexps.size(); i++) {
            Node regexp = (Node) regexps.elementAt(i);
            Node left = regexp.getFirstChild();
            Node right = regexp.getLastChild();
            result[i] = rep.newRegExp(cx, scope, left.getString(), 
                                (left != right) ? right.getString() : null, false);
            regexp.putProp(Node.REGEXP_PROP, new Integer(i));
        }
        return result;
    }
        
    private InterpretedScript generateScriptICode(Context cx, 
                                                  Scriptable scope, 
                                                  Node tree,
                                                  Object securityDomain)
    {        
        itsSourceFile = (String) tree.getProp(Node.SOURCENAME_PROP);
        itsData.itsSourceFile = itsSourceFile;
        itsFunctionList = (Vector) tree.getProp(Node.FUNCTION_PROP);        
        debugSource = (StringBuffer) tree.getProp(Node.DEBUGSOURCE_PROP);
        if (itsFunctionList != null)
            generateNestedFunctions(scope, cx, securityDomain);
        Object[] regExpLiterals = null;
        Vector regexps = (Vector)tree.getProp(Node.REGEXP_PROP);
        if (regexps != null) 
            regExpLiterals = generateRegExpLiterals(cx, scope, regexps);
        
        VariableTable varTable = (VariableTable)tree.getProp(Node.VARS_PROP);
        // The default is not to generate debug information
        boolean activationNeeded = cx.isGeneratingDebugChanged() && 
                                   cx.isGeneratingDebug();
        generateICodeFromTree(tree, varTable, activationNeeded, securityDomain);
        itsData.itsNestedFunctions = itsNestedFunctions;
        itsData.itsRegExpLiterals = regExpLiterals;
        if (printICode) dumpICode(itsData);
                                                               
        InterpretedScript result = new InterpretedScript(itsData, cx);
        if (cx.debugger != null) {
            cx.debugger.handleCompilationDone(cx, result, debugSource);
        }
        return result;
    }
    
    private void generateNestedFunctions(Scriptable scope,
                                         Context cx, 
                                         Object securityDomain)
    {
        itsNestedFunctions = new InterpretedFunction[itsFunctionList.size()];
        for (short i = 0; i < itsFunctionList.size(); i++) {
            FunctionNode def = (FunctionNode)itsFunctionList.elementAt(i);
            Interpreter jsi = new Interpreter();
            jsi.itsSourceFile = itsSourceFile;
            jsi.itsData = new InterpreterData(0, 0, 0, securityDomain,
                            cx.hasCompileFunctionsWithDynamicScope());
            jsi.itsData.itsFunctionType = def.getFunctionType();
            jsi.itsInFunctionFlag = true;
            jsi.debugSource = debugSource;
            itsNestedFunctions[i] = jsi.generateFunctionICode(cx, scope, def, 
                                                              securityDomain);
            def.putProp(Node.FUNCTION_PROP, new Short(i));
        }
    }        
    
    private InterpretedFunction 
    generateFunctionICode(Context cx, Scriptable scope, 
                          FunctionNode theFunction, Object securityDomain)
    {
        itsFunctionList = (Vector) theFunction.getProp(Node.FUNCTION_PROP);
        if (itsFunctionList != null) 
            generateNestedFunctions(scope, cx, securityDomain);
        Object[] regExpLiterals = null;
        Vector regexps = (Vector)theFunction.getProp(Node.REGEXP_PROP);
        if (regexps != null) 
            regExpLiterals = generateRegExpLiterals(cx, scope, regexps);

        VariableTable varTable = theFunction.getVariableTable();
        boolean needsActivation = theFunction.requiresActivation() ||
                                  (cx.isGeneratingDebugChanged() && 
                                   cx.isGeneratingDebug());
        generateICodeFromTree(theFunction.getLastChild(), 
                              varTable, needsActivation,
                              securityDomain);
            
        itsData.itsName = theFunction.getFunctionName();
        itsData.itsSourceFile = (String) theFunction.getProp(
                                    Node.SOURCENAME_PROP);
        itsData.itsSource = (String)theFunction.getProp(Node.SOURCE_PROP);
        itsData.itsNestedFunctions = itsNestedFunctions;
        itsData.itsRegExpLiterals = regExpLiterals;
        if (printICode) dumpICode(itsData);            
            
        InterpretedFunction result = new InterpretedFunction(itsData, cx);
        if (cx.debugger != null) {
            cx.debugger.handleCompilationDone(cx, result, debugSource);
        }
        return result;
    }
    
    boolean itsInFunctionFlag;
    Vector itsFunctionList;
    
    InterpreterData itsData;
    int itsTryDepth = 0;
    int itsStackDepth = 0;
    int itsEpilogLabel = -1;
    String itsSourceFile;
    int itsLineNumber = 0;
    InterpretedFunction[] itsNestedFunctions = null;
    
    private int updateLineNumber(Node node, int iCodeTop)
    {
        Object datum = node.getDatum();
        if (datum == null || !(datum instanceof Number))
            return iCodeTop;
        short lineNumber = ((Number) datum).shortValue(); 
        if (lineNumber != itsLineNumber) {
            itsLineNumber = lineNumber;
            if (itsData.itsLineNumberTable == null && 
                Context.getCurrentContext().isGeneratingDebug())
            {
                itsData.itsLineNumberTable = new java.util.Hashtable();
            }
            if (itsData.itsLineNumberTable != null) {
                itsData.itsLineNumberTable.put(new Integer(lineNumber), 
                                               new Integer(iCodeTop));
            }
            iCodeTop = addByte((byte) TokenStream.LINE, iCodeTop);
            iCodeTop = addByte((byte)(lineNumber >> 8), iCodeTop);
            iCodeTop = addByte((byte)(lineNumber & 0xff), iCodeTop);
            
        }
        
        return iCodeTop;
    }
    
    private void badTree(Node node)
    {
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
                    iCodeTop = addByte((byte) TokenStream.CLOSURE, iCodeTop);
                    Node fn = (Node) node.getProp(Node.FUNCTION_PROP);
                    Short index = (Short) fn.getProp(Node.FUNCTION_PROP);
                    iCodeTop = addByte((byte)(index.shortValue() >> 8), iCodeTop);
                    iCodeTop = addByte((byte)(index.shortValue() & 0xff), iCodeTop);                    
                    itsStackDepth++;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                }
                break;

            case TokenStream.SCRIPT :
                iCodeTop = updateLineNumber(node, iCodeTop);
                while (child != null) {
                    if (child.getType() != TokenStream.FUNCTION) 
                        iCodeTop = generateICode(child, iCodeTop);
                    child = child.getNextSibling();
                }
                break;

            case TokenStream.CASE :
                iCodeTop = updateLineNumber(node, iCodeTop);
                child = child.getNextSibling();
                while (child != null) {
                    iCodeTop = generateICode(child, iCodeTop);
                    child = child.getNextSibling();
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
                    child = child.getNextSibling();
                }
                break;

            case TokenStream.COMMA :
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addByte((byte) TokenStream.POP, iCodeTop);
                itsStackDepth--;
                child = child.getNextSibling();
                iCodeTop = generateICode(child, iCodeTop);
                break;
               
            case TokenStream.SWITCH : {
                    iCodeTop = updateLineNumber(node, iCodeTop);
                    iCodeTop = generateICode(child, iCodeTop);
                    int theLocalSlot = itsData.itsMaxLocals++;
                    iCodeTop = addByte((byte) TokenStream.NEWTEMP, iCodeTop);
                    iCodeTop = addByte((byte)theLocalSlot, iCodeTop);
                    iCodeTop = addByte((byte) TokenStream.POP, iCodeTop);
                    itsStackDepth--;
         /*
            reminder - below we construct new GOTO nodes that aren't
            linked into the tree just for the purpose of having a node
            to pass to the addGoto routine. (Parallels codegen here).
            Seems unnecessary.        
         */
                    Vector cases = (Vector) node.getProp(Node.CASES_PROP);
                    for (int i = 0; i < cases.size(); i++) {
                        Node thisCase = (Node)cases.elementAt(i);
                        Node first = thisCase.getFirstChild();
                        // the case expression is the firstmost child
                        // the rest will be generated when the case
                        // statements are encountered as siblings of
                        // the switch statement.
                        iCodeTop = generateICode(first, iCodeTop);                   
                        iCodeTop = addByte((byte) TokenStream.USETEMP, iCodeTop);
                        itsStackDepth++;
                        if (itsStackDepth > itsData.itsMaxStack)
                            itsData.itsMaxStack = itsStackDepth;
                        iCodeTop = addByte((byte) theLocalSlot, iCodeTop);
                        iCodeTop = addByte((byte) TokenStream.SHEQ, iCodeTop);
                        Node target = new Node(TokenStream.TARGET);
                        thisCase.addChildAfter(target, first);
                        Node branch = new Node(TokenStream.IFEQ);
                        branch.putProp(Node.TARGET_PROP, target);
                        iCodeTop = addGoto(branch, TokenStream.IFEQ, 
                                           iCodeTop);
                        itsStackDepth--;
                    }

                    Node defaultNode = (Node) node.getProp(Node.DEFAULT_PROP);
                    if (defaultNode != null) {
                        Node defaultTarget = new Node(TokenStream.TARGET);
                        defaultNode.getFirstChild().addChildToFront(defaultTarget);
                        Node branch = new Node(TokenStream.GOTO);
                        branch.putProp(Node.TARGET_PROP, defaultTarget);
                        iCodeTop = addGoto(branch, TokenStream.GOTO,
                                                            iCodeTop);                    
                    }

                    Node breakTarget = (Node) node.getProp(Node.BREAK_PROP);
                    Node branch = new Node(TokenStream.GOTO);
                    branch.putProp(Node.TARGET_PROP, breakTarget);
                    iCodeTop = addGoto(branch, TokenStream.GOTO, 
                                       iCodeTop);                    
                }
                break;
                                
            case TokenStream.TARGET : { 
                    Object lblObect = node.getProp(Node.LABEL_PROP);
                    if (lblObect == null) {
                        int label = markLabel(acquireLabel(), iCodeTop);
                        node.putProp(Node.LABEL_PROP, new Integer(label));
                    }
                    else {
                        int label = ((Integer)lblObect).intValue();
                        markLabel(label, iCodeTop);
                    }
                    // if this target has a FINALLY_PROP, it is a JSR target
                    // and so has a PC value on the top of the stack
                    if (node.getProp(Node.FINALLY_PROP) != null) {
                        itsStackDepth = 1;
                        if (itsStackDepth > itsData.itsMaxStack)
                            itsData.itsMaxStack = itsStackDepth;
                    }
                }
                break;
                
            case TokenStream.EQOP :
            case TokenStream.RELOP : {
                    iCodeTop = generateICode(child, iCodeTop);
                    child = child.getNextSibling();
                    iCodeTop = generateICode(child, iCodeTop);
                    int op = node.getInt();
                    if (version == Context.VERSION_1_2) {
                        if (op == TokenStream.EQ)
                            op = TokenStream.SHEQ;
                        else if (op == TokenStream.NE)
                            op = TokenStream.SHNE;
                    }
                    iCodeTop = addByte((byte) op, iCodeTop);
                    itsStackDepth--;
                }
                break;
                
            case TokenStream.NEW :
            case TokenStream.CALL : {
                    if (itsSourceFile != null && (itsData.itsSourceFile == null || ! itsSourceFile.equals(itsData.itsSourceFile))) 
                        itsData.itsSourceFile = itsSourceFile;
                    iCodeTop = addByte((byte)TokenStream.SOURCEFILE, iCodeTop);
                    
                    int childCount = 0;
                    short nameIndex = -1;
                    while (child != null) {
                        iCodeTop = generateICode(child, iCodeTop);
                        if (nameIndex == -1) {
                            if (child.getType() == TokenStream.NAME)
                                nameIndex = (short)(itsData.itsStringTableIndex - 1);
                            else if (child.getType() == TokenStream.GETPROP)
                                nameIndex = (short)(itsData.itsStringTableIndex - 1);
                        }
                        child = child.getNextSibling();
                        childCount++;
                    }
                    if (node.getProp(Node.SPECIALCALL_PROP) != null) {
                        // embed line number and source filename
                        iCodeTop = addByte((byte) TokenStream.CALLSPECIAL, iCodeTop);
                        iCodeTop = addByte((byte)(itsLineNumber >> 8), iCodeTop);
                        iCodeTop = addByte((byte)(itsLineNumber & 0xff), iCodeTop);
                        iCodeTop = addString(itsSourceFile, iCodeTop);
                    } else {
                        iCodeTop = addByte((byte) type, iCodeTop);
                        iCodeTop = addByte((byte)(nameIndex >> 8), iCodeTop);
                        iCodeTop = addByte((byte)(nameIndex & 0xFF), iCodeTop);
                    }
                    
                    itsStackDepth -= (childCount - 1);  // always a result value
                    // subtract from child count to account for [thisObj &] fun
                    if (type == TokenStream.NEW)
                        childCount -= 1;
                    else
                        childCount -= 2;
                    iCodeTop = addByte((byte)(childCount >> 8), iCodeTop);
                    iCodeTop = addByte((byte)(childCount & 0xff), iCodeTop);
                    if (childCount > itsData.itsMaxArgs)
                        itsData.itsMaxArgs = childCount;
                    
                    iCodeTop = addByte((byte)TokenStream.SOURCEFILE, iCodeTop);
                }
                break;
                
            case TokenStream.NEWLOCAL :
            case TokenStream.NEWTEMP : {
                    iCodeTop = generateICode(child, iCodeTop);
                    iCodeTop = addByte((byte) TokenStream.NEWTEMP, iCodeTop);
                    iCodeTop = addLocalRef(node, iCodeTop);
                }
                break;                
                   
            case TokenStream.USELOCAL : {
                    if (node.getProp(Node.TARGET_PROP) != null) 
                        iCodeTop = addByte((byte) TokenStream.RETSUB, iCodeTop);
                    else {
                        iCodeTop = addByte((byte) TokenStream.USETEMP, iCodeTop);
                        itsStackDepth++;
                        if (itsStackDepth > itsData.itsMaxStack)
                            itsData.itsMaxStack = itsStackDepth;
                    }
                    Node temp = (Node) node.getProp(Node.LOCAL_PROP);
                    iCodeTop = addLocalRef(temp, iCodeTop);
                }
                break;                

            case TokenStream.USETEMP : {
                    iCodeTop = addByte((byte) TokenStream.USETEMP, iCodeTop);
                    Node temp = (Node) node.getProp(Node.TEMP_PROP);
                    iCodeTop = addLocalRef(temp, iCodeTop);
                    itsStackDepth++;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                }
                break;                
                
            case TokenStream.IFEQ :
            case TokenStream.IFNE :
                iCodeTop = generateICode(child, iCodeTop);
                itsStackDepth--;    // after the conditional GOTO, really
                    // fall thru...
            case TokenStream.GOTO :
                iCodeTop = addGoto(node, (byte) type, iCodeTop);
                break;

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
                    iCodeTop = addGoto(node, TokenStream.GOSUB, iCodeTop);
                }
                break;
            
            case TokenStream.AND : {            
                    iCodeTop = generateICode(child, iCodeTop);
                    iCodeTop = addByte((byte) TokenStream.DUP, iCodeTop);                
                    itsStackDepth++;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                    int falseTarget = acquireLabel();
                    iCodeTop = addGoto(falseTarget, TokenStream.IFNE, 
                                                    iCodeTop);
                    iCodeTop = addByte((byte) TokenStream.POP, iCodeTop);
                    itsStackDepth--;
                    child = child.getNextSibling();
                    iCodeTop = generateICode(child, iCodeTop);
                    markLabel(falseTarget, iCodeTop);
                }
                break;

            case TokenStream.OR : {
                    iCodeTop = generateICode(child, iCodeTop);
                    iCodeTop = addByte((byte) TokenStream.DUP, iCodeTop);                
                    itsStackDepth++;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                    int trueTarget = acquireLabel();
                    iCodeTop = addGoto(trueTarget, TokenStream.IFEQ,
                                       iCodeTop);
                    iCodeTop = addByte((byte) TokenStream.POP, iCodeTop);                
                    itsStackDepth--;
                    child = child.getNextSibling();
                    iCodeTop = generateICode(child, iCodeTop);
                    markLabel(trueTarget, iCodeTop);
                }
                break;

            case TokenStream.GETPROP : {
                    iCodeTop = generateICode(child, iCodeTop);
                    String s = (String) node.getProp(Node.SPECIAL_PROP_PROP);
                    if (s != null) {
                        if (s.equals("__proto__"))
                            iCodeTop = addByte((byte) TokenStream.GETPROTO, iCodeTop);
                        else
                            if (s.equals("__parent__"))
                                iCodeTop = addByte((byte) TokenStream.GETSCOPEPARENT, iCodeTop);
                            else
                                badTree(node);
                    }
                    else {
                        child = child.getNextSibling();
                        iCodeTop = generateICode(child, iCodeTop);
                        iCodeTop = addByte((byte) TokenStream.GETPROP, iCodeTop);
                        itsStackDepth--;
                    }
                }
                break;

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
                child = child.getNextSibling();
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addByte((byte) type, iCodeTop);
                itsStackDepth--;
                break;

            case TokenStream.CONVERT : {
                    iCodeTop = generateICode(child, iCodeTop);
                    Object toType = node.getProp(Node.TYPE_PROP);
                    if (toType == ScriptRuntime.NumberClass)
                        iCodeTop = addByte((byte) TokenStream.POS, iCodeTop);
                    else
                        badTree(node);
                }
                break;

            case TokenStream.UNARYOP :
                iCodeTop = generateICode(child, iCodeTop);
                switch (node.getInt()) {
                    case TokenStream.VOID :
                        iCodeTop = addByte((byte) TokenStream.POP, iCodeTop);
                        iCodeTop = addByte((byte) TokenStream.UNDEFINED, iCodeTop);
                        break;
                    case TokenStream.NOT : {
                            int trueTarget = acquireLabel();
                            int beyond = acquireLabel();
                            iCodeTop = addGoto(trueTarget, TokenStream.IFEQ,
                                                        iCodeTop);
                            iCodeTop = addByte((byte) TokenStream.TRUE, iCodeTop);
                            iCodeTop = addGoto(beyond, TokenStream.GOTO, 
                                                        iCodeTop);
                            markLabel(trueTarget, iCodeTop);
                            iCodeTop = addByte((byte) TokenStream.FALSE, iCodeTop);
                            markLabel(beyond, iCodeTop);
                        }
                        break;
                    case TokenStream.BITNOT :
                        iCodeTop = addByte((byte) TokenStream.BITNOT, iCodeTop);
                        break;
                    case TokenStream.TYPEOF :
                        iCodeTop = addByte((byte) TokenStream.TYPEOF, iCodeTop);
                        break;
                    case TokenStream.SUB :
                        iCodeTop = addByte((byte) TokenStream.NEG, iCodeTop);
                        break;
                    case TokenStream.ADD :
                        iCodeTop = addByte((byte) TokenStream.POS, iCodeTop);
                        break;
                    default:
                        badTree(node);
                        break;
                }
                break;

            case TokenStream.SETPROP : {
                    iCodeTop = generateICode(child, iCodeTop);
                    child = child.getNextSibling();
                    iCodeTop = generateICode(child, iCodeTop);
                    String s = (String) node.getProp(Node.SPECIAL_PROP_PROP);
                    if (s != null) {
                        if (s.equals("__proto__"))
                            iCodeTop = addByte((byte) TokenStream.SETPROTO, iCodeTop);
                        else
                            if (s.equals("__parent__"))
                                iCodeTop = addByte((byte) TokenStream.SETPARENT, iCodeTop);
                            else
                                badTree(node);
                    }
                    else {
                        child = child.getNextSibling();
                        iCodeTop = generateICode(child, iCodeTop);
                        iCodeTop = addByte((byte) TokenStream.SETPROP, iCodeTop);
                        itsStackDepth -= 2;
                    }
                }
                break;            

            case TokenStream.SETELEM :
                iCodeTop = generateICode(child, iCodeTop);
                child = child.getNextSibling();
                iCodeTop = generateICode(child, iCodeTop);
                child = child.getNextSibling();
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addByte((byte) type, iCodeTop);
                itsStackDepth -= 2;
                break;

            case TokenStream.SETNAME :
                iCodeTop = generateICode(child, iCodeTop);
                child = child.getNextSibling();
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addByte((byte) TokenStream.SETNAME, iCodeTop);                
                iCodeTop = addString(firstChild.getString(), iCodeTop);
                itsStackDepth--;
                break;
                
            case TokenStream.TYPEOF : {
                    String name = node.getString();
                    int index = -1;
                    // use typeofname if an activation frame exists
                    // since the vars all exist there instead of in jregs
                    if (itsInFunctionFlag && !itsData.itsNeedsActivation)
                        index = itsData.itsVariableTable.getOrdinal(name);
                    if (index == -1) {                    
                        iCodeTop = addByte((byte) TokenStream.TYPEOFNAME, iCodeTop);
                        iCodeTop = addString(name, iCodeTop);
                    }
                    else {
                        iCodeTop = addByte((byte) TokenStream.GETVAR, iCodeTop);
                        iCodeTop = addByte((byte) index, iCodeTop);
                        iCodeTop = addByte((byte) TokenStream.TYPEOF, iCodeTop);
                    }
                    itsStackDepth++;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                }
                break;

            case TokenStream.PARENT :
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addByte((byte) TokenStream.GETPARENT, iCodeTop);
                break;

            case TokenStream.GETBASE :
            case TokenStream.BINDNAME :
            case TokenStream.NAME :
            case TokenStream.STRING :
                iCodeTop = addByte((byte) type, iCodeTop);
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
                                    iCodeTop = addByte((byte) TokenStream.SCOPE, iCodeTop);
                                    iCodeTop = addByte((byte) TokenStream.STRING, iCodeTop);
                                    iCodeTop = addString(name, iCodeTop);
                                    itsStackDepth += 2;
                                    if (itsStackDepth > itsData.itsMaxStack)
                                        itsData.itsMaxStack = itsStackDepth;
                                    iCodeTop = addByte((byte)
                                                (type == TokenStream.INC
                                                    ? TokenStream.PROPINC 
                                                    : TokenStream.PROPDEC),
                                                 iCodeTop);
                                    itsStackDepth--;                                        
                                }
                                else {
                                    iCodeTop = addByte((byte)
                                                (type == TokenStream.INC
                                                    ? TokenStream.VARINC
                                                    : TokenStream.VARDEC),
                                                iCodeTop);
                                    int i = itsData.itsVariableTable.
                                                            getOrdinal(name);
                                    iCodeTop = addByte((byte)i, iCodeTop);
                                    itsStackDepth++;
                                    if (itsStackDepth > itsData.itsMaxStack)
                                        itsData.itsMaxStack = itsStackDepth;
                                }
                            }
                            break;
                        case TokenStream.GETPROP :
                        case TokenStream.GETELEM : {
                                Node getPropChild = child.getFirstChild();
                                iCodeTop = generateICode(getPropChild,
                                                              iCodeTop);
                                getPropChild = getPropChild.getNextSibling();
                                iCodeTop = generateICode(getPropChild,
                                                              iCodeTop);
                                if (childType == TokenStream.GETPROP)
                                    iCodeTop = addByte((byte)
                                                    (type == TokenStream.INC
                                                        ? TokenStream.PROPINC 
                                                        : TokenStream.PROPDEC),
                                                    iCodeTop);
                                else                                                        
                                    iCodeTop = addByte((byte)
                                                    (type == TokenStream.INC
                                                        ? TokenStream.ELEMINC 
                                                        : TokenStream.ELEMDEC),
                                                    iCodeTop);
                                itsStackDepth--;                                        
                            }
                            break;
                        default : {
                                iCodeTop = addByte((byte)
                                                    (type == TokenStream.INC 
                                                        ? TokenStream.NAMEINC 
                                                        : TokenStream.NAMEDEC),
                                                    iCodeTop);
                                iCodeTop = addString(child.getString(), 
                                                            iCodeTop);
                                itsStackDepth++;
                                if (itsStackDepth > itsData.itsMaxStack)
                                    itsData.itsMaxStack = itsStackDepth;
                            }
                            break;
                    }
                }
                break;

            case TokenStream.NUMBER : {
                double num = node.getDouble();
                if (num == 0.0) {
                    iCodeTop = addByte((byte) TokenStream.ZERO, iCodeTop);
                }
                else if (num == 1.0) {
                    iCodeTop = addByte((byte) TokenStream.ONE, iCodeTop);
                }
                else {
                    iCodeTop = addByte((byte) TokenStream.NUMBER, iCodeTop);
                    iCodeTop = addNumber(num, iCodeTop);
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
                iCodeTop = addByte((byte) type, iCodeTop);
                itsStackDepth--;
                break;

            case TokenStream.GETTHIS :
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addByte((byte) type, iCodeTop);
                break;
                
            case TokenStream.NEWSCOPE :
                iCodeTop = addByte((byte) type, iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;

            case TokenStream.LEAVEWITH :
                iCodeTop = addByte((byte) type, iCodeTop);
                break;

            case TokenStream.TRY : {
                    itsTryDepth++;
                    if (itsTryDepth > itsData.itsMaxTryDepth)
                        itsData.itsMaxTryDepth = itsTryDepth;
                    Node catchTarget = (Node)node.getProp(Node.TARGET_PROP);
                    Node finallyTarget = (Node)node.getProp(Node.FINALLY_PROP);
                    if (catchTarget == null) {
                        iCodeTop = addByte((byte) TokenStream.TRY, iCodeTop);
                        iCodeTop = addByte((byte)0, iCodeTop);
                        iCodeTop = addByte((byte)0, iCodeTop);
                    }
                    else
                        iCodeTop = 
                            addGoto(node, TokenStream.TRY, iCodeTop);
                    int finallyHandler = 0;
                    if (finallyTarget != null) {
                        finallyHandler = acquireLabel();
                        int theLabel = finallyHandler & 0x7FFFFFFF;
                        itsLabelTable[theLabel].addFixup(iCodeTop);
                    }
                    iCodeTop = addByte((byte)0, iCodeTop);
                    iCodeTop = addByte((byte)0, iCodeTop);
                    
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
                        Node nextSibling = child.getNextSibling();
                        if (!insertedEndTry && nextSibling != null &&
                            (nextSibling == catchTarget ||
                             nextSibling == finallyTarget))
                        {
                            iCodeTop = addByte((byte) TokenStream.ENDTRY,
                                               iCodeTop);
                            insertedEndTry = true;
                        }
                        iCodeTop = generateICode(child, iCodeTop);
                        lastChild = child;
                        child = child.getNextSibling();
                    }
                    itsStackDepth = 0;
                    if (finallyTarget != null) {
                        // normal flow goes around the finally handler stublet
                        int skippy = acquireLabel();
                        iCodeTop = 
                            addGoto(skippy, TokenStream.GOTO, iCodeTop);
                        // on entry the stack will have the exception object
                        markLabel(finallyHandler, iCodeTop);
                        itsStackDepth = 1;
                        if (itsStackDepth > itsData.itsMaxStack)
                            itsData.itsMaxStack = itsStackDepth;
                        int theLocalSlot = itsData.itsMaxLocals++;
                        iCodeTop = addByte((byte) TokenStream.NEWTEMP, iCodeTop);
                        iCodeTop = addByte((byte)theLocalSlot, iCodeTop);
                        iCodeTop = addByte((byte) TokenStream.POP, iCodeTop);
                        Integer finallyLabel 
                           = (Integer)(finallyTarget.getProp(Node.LABEL_PROP));
                        iCodeTop = addGoto(finallyLabel.intValue(), 
                                         TokenStream.GOSUB, iCodeTop);
                        iCodeTop = addByte((byte) TokenStream.USETEMP, iCodeTop);
                        iCodeTop = addByte((byte)theLocalSlot, iCodeTop);
                        iCodeTop = addByte((byte) TokenStream.JTHROW, iCodeTop);
                        itsStackDepth = 0;
                        markLabel(skippy, iCodeTop);
                    }
                    itsTryDepth--;
                }
                break;
                
            case TokenStream.THROW :
                iCodeTop = updateLineNumber(node, iCodeTop);
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addByte((byte) TokenStream.THROW, iCodeTop);
                itsStackDepth--;
                break;
                
            case TokenStream.RETURN :
                iCodeTop = updateLineNumber(node, iCodeTop);
                if (child != null) 
                    iCodeTop = generateICode(child, iCodeTop);
                else {
                    iCodeTop = addByte((byte) TokenStream.UNDEFINED, iCodeTop);
                    itsStackDepth++;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                }
                iCodeTop = addGoto(node, TokenStream.RETURN, iCodeTop);
                itsStackDepth--;
                break;
                
            case TokenStream.GETVAR : {
                    String name = node.getString();
                    if (itsData.itsNeedsActivation) {
                        // SETVAR handled this by turning into a SETPROP, but
                        // we can't do that to a GETVAR without manufacturing
                        // bogus children. Instead we use a special op to
                        // push the current scope.
                        iCodeTop = addByte((byte) TokenStream.SCOPE, iCodeTop);
                        iCodeTop = addByte((byte) TokenStream.STRING, iCodeTop);
                        iCodeTop = addString(name, iCodeTop);
                        itsStackDepth += 2;
                        if (itsStackDepth > itsData.itsMaxStack)
                            itsData.itsMaxStack = itsStackDepth;
                        iCodeTop = addByte((byte) TokenStream.GETPROP, iCodeTop);
                        itsStackDepth--;
                    }
                    else {
                        int index = itsData.itsVariableTable.getOrdinal(name);
                        iCodeTop = addByte((byte) TokenStream.GETVAR, iCodeTop);
                        iCodeTop = addByte((byte)index, iCodeTop);
                        itsStackDepth++;
                        if (itsStackDepth > itsData.itsMaxStack)
                            itsData.itsMaxStack = itsStackDepth;
                    }
                }
                break;
                
            case TokenStream.SETVAR : {
                    if (itsData.itsNeedsActivation) {
                        child.setType(TokenStream.BINDNAME);
                        node.setType(TokenStream.SETNAME);
                        iCodeTop = generateICode(node, iCodeTop);
                    }
                    else {
                        String name = child.getString();
                        child = child.getNextSibling();
                        iCodeTop = generateICode(child, iCodeTop);
                        int index = itsData.itsVariableTable.getOrdinal(name);
                        iCodeTop = addByte((byte) TokenStream.SETVAR, iCodeTop);
                        iCodeTop = addByte((byte)index, iCodeTop);
                    }
                }
                break;
                
            case TokenStream.PRIMARY:
                iCodeTop = addByte((byte) node.getInt(), iCodeTop);
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;

            case TokenStream.ENUMINIT :
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addByte((byte) TokenStream.ENUMINIT, iCodeTop);
                iCodeTop = addLocalRef(node, iCodeTop);
                itsStackDepth--;
                break;
                
            case TokenStream.ENUMNEXT : {
                    iCodeTop = addByte((byte) TokenStream.ENUMNEXT, iCodeTop);
                    Node init = (Node)node.getProp(Node.ENUM_PROP);
                    iCodeTop = addLocalRef(init, iCodeTop);
                    itsStackDepth++;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                }                        
                break;
                
            case TokenStream.ENUMDONE :
                // could release the local here??
                break;
                
            case TokenStream.OBJECT : {
                    Node regexp = (Node) node.getProp(Node.REGEXP_PROP);
                    int index = ((Integer)(regexp.getProp(
                                            Node.REGEXP_PROP))).intValue();
                    iCodeTop = addByte((byte) TokenStream.OBJECT, iCodeTop);
                    iCodeTop = addByte((byte)(index >> 8), iCodeTop);
                    iCodeTop = addByte((byte)(index & 0xff), iCodeTop);
                    itsStackDepth++;
                    if (itsStackDepth > itsData.itsMaxStack)
                        itsData.itsMaxStack = itsStackDepth;
                }
                break;
                
            default : 
                badTree(node);
                break;
        }
        return iCodeTop;
    }
    
    private int addLocalRef(Node node, int iCodeTop)
    {
        int theLocalSlot;
        Integer localProp = (Integer)node.getProp(Node.LOCAL_PROP);
        if (localProp == null) {
            theLocalSlot = itsData.itsMaxLocals++;
            node.putProp(Node.LOCAL_PROP, new Integer(theLocalSlot));
        }
        else
            theLocalSlot = localProp.intValue();
        iCodeTop = addByte((byte)theLocalSlot, iCodeTop);
        if (theLocalSlot >= itsData.itsMaxLocals)
            itsData.itsMaxLocals = theLocalSlot + 1;
        return iCodeTop;            
    }
    
    private int addGoto(Node node, int gotoOp, int iCodeTop)
    {
        int targetLabel;
        if (node.getType() == TokenStream.RETURN) {
            if (itsEpilogLabel == -1)
                itsEpilogLabel = acquireLabel();
            targetLabel = itsEpilogLabel;
        }
        else {
            Node target = (Node)(node.getProp(Node.TARGET_PROP));
            Object lblObect = target.getProp(Node.LABEL_PROP);
            if (lblObect == null) {
                targetLabel = acquireLabel();
                target.putProp(Node.LABEL_PROP, new Integer(targetLabel));
            }
            else
                targetLabel = ((Integer)lblObect).intValue();
        }
        iCodeTop = addGoto(targetLabel, (byte) gotoOp, iCodeTop);
        return iCodeTop;
    }
    
    private int addGoto(int targetLabel, int gotoOp, int iCodeTop)
    {
        int gotoPC = iCodeTop;
        iCodeTop = addByte((byte) gotoOp, iCodeTop);
        int theLabel = targetLabel & 0x7FFFFFFF;
        int targetPC = itsLabelTable[theLabel].getPC();
        if (targetPC != -1) {
            short offset = (short)(targetPC - gotoPC);
            iCodeTop = addByte((byte)(offset >> 8), iCodeTop);
            iCodeTop = addByte((byte)offset, iCodeTop);
        }
        else {
            itsLabelTable[theLabel].addFixup(gotoPC + 1);
            iCodeTop = addByte((byte)0, iCodeTop);
            iCodeTop = addByte((byte)0, iCodeTop);
        }
        return iCodeTop;
    }
    
    private final int addByte(byte b, int iCodeTop) {
        if (itsData.itsICode.length == iCodeTop) {
            byte[] ba = new byte[iCodeTop * 2];
            System.arraycopy(itsData.itsICode, 0, ba, 0, iCodeTop);
            itsData.itsICode = ba;
        }
        itsData.itsICode[iCodeTop++] = b;
        return iCodeTop;
    }
    
    private final int addString(String str, int iCodeTop)
    {
        int index = itsData.itsStringTableIndex;
        if (itsData.itsStringTable.length == index) {
            String[] sa = new String[index * 2];
            System.arraycopy(itsData.itsStringTable, 0, sa, 0, index);
            itsData.itsStringTable = sa;
        }
        itsData.itsStringTable[index] = str;
        itsData.itsStringTableIndex = index + 1;

        iCodeTop = addByte((byte)(index >> 8), iCodeTop);
        iCodeTop = addByte((byte)(index & 0xFF), iCodeTop);
        return iCodeTop;
    }
    
    private final int addNumber(double num, int iCodeTop)
    {
        int index = itsData.itsNumberTableIndex;
        if (itsData.itsNumberTable.length == index) {
            double[] na = new double[index * 2];
            System.arraycopy(itsData.itsNumberTable, 0, na, 0, index);
            itsData.itsNumberTable = na;
        }
        itsData.itsNumberTable[index] = num;
        itsData.itsNumberTableIndex = index + 1;

        iCodeTop = addByte((byte)(index >> 8), iCodeTop);
        iCodeTop = addByte((byte)(index & 0xFF), iCodeTop);
        return iCodeTop;
    }
    
    private static String getString(String[] theStringTable, byte[] iCode, 
                                    int pc)
    {
        int index = (iCode[pc] << 8) + (iCode[pc + 1] & 0xFF);
        return theStringTable[index];
    }
    
    private static double getNumber(double[] theNumberTable, byte[] iCode, 
                                    int pc)
    {
        int index = (iCode[pc] << 8) + (iCode[pc + 1] & 0xFF);
        return theNumberTable[index];
    }
    
    private static int getTarget(byte[] iCode, int pc)
    {
        int displacement = (iCode[pc] << 8) + (iCode[pc + 1] & 0xFF);
        return pc - 1 + displacement;
    }
    
    static PrintWriter out;
    static {
        if (printICode) {
            try {
                out = new PrintWriter(new FileOutputStream("icode.txt"));
                out.close();
            }
            catch (IOException x) {
            }
        }
    }   
    
    private static void dumpICode(InterpreterData theData) {
        if (printICode) {
            try {
                int iCodeLength = theData.itsICodeTop;
                byte iCode[] = theData.itsICode;
                
                out = new PrintWriter(new FileOutputStream("icode.txt", true));
                out.println("ICode dump, for " + theData.itsName + ", length = " + iCodeLength);
                out.println("MaxStack = " + theData.itsMaxStack);
                
                for (int pc = 0; pc < iCodeLength; ) {
                    out.print("[" + pc + "] ");
                    switch ((int)(iCode[pc] & 0xff)) {
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
                        case TokenStream.SOURCEFILE : 
                            out.println(TokenStream.tokenToName(iCode[pc] & 0xff));
                            break;
                        case TokenStream.GOSUB :
                        case TokenStream.RETURN :
                        case TokenStream.GOTO :
                        case TokenStream.IFEQ :
                        case TokenStream.IFNE : {
                                int newPC = getTarget(iCode, pc + 1);                    
                                out.println(
                                    TokenStream.tokenToName(iCode[pc] & 0xff) +
                                    " " + newPC);
                                pc += 2;
                            }
                            break;
                        case TokenStream.TRY : {
                                int newPC1 = getTarget(iCode, pc + 1);                    
                                int newPC2 = getTarget(iCode, pc + 3);                    
                                out.println(
                                    TokenStream.tokenToName(iCode[pc] & 0xff) +
                                    " " + newPC1 +
                                    " " + newPC2);
                                pc += 4;
                            }
                            break;
                        case TokenStream.RETSUB :                        
                        case TokenStream.ENUMINIT :
                        case TokenStream.ENUMNEXT :
                        case TokenStream.VARINC :
                        case TokenStream.VARDEC :
                        case TokenStream.GETVAR :
                        case TokenStream.SETVAR :
                        case TokenStream.NEWTEMP :
                        case TokenStream.USETEMP : {
                                int slot = (iCode[pc + 1] & 0xFF);
                                out.println(
                                    TokenStream.tokenToName(iCode[pc] & 0xff) +
                                    " " + slot);
                                pc++;
                            }
                            break;
                        case TokenStream.CALLSPECIAL : {
                                int line = (iCode[pc + 1] << 8) 
                                                        | (iCode[pc + 2] & 0xFF);
                                String name = getString(theData.itsStringTable,
                                                                  iCode, pc + 3);
                                int count = (iCode[pc + 5] << 8) | (iCode[pc + 6] & 0xFF);
                                out.println(
                                    TokenStream.tokenToName(iCode[pc] & 0xff) +
                                    " " + count + " " + line + " " + name);
                                pc += 6;
                            }
                            break;
                        case TokenStream.OBJECT :
                        case TokenStream.CLOSURE :
                        case TokenStream.NEW :
                        case TokenStream.CALL : {
                                int count = (iCode[pc + 3] << 8) | (iCode[pc + 4] & 0xFF);
                                out.println(
                                    TokenStream.tokenToName(iCode[pc] & 0xff) +
                                    " " + count + " \"" + 
                                    getString(theData.itsStringTable, iCode, 
                                              pc + 1) + "\"");
                                pc += 5;
                            }
                            break;
                        case TokenStream.NUMBER :
                            out.println(
                                TokenStream.tokenToName(iCode[pc] & 0xff) + 
                                " " + getNumber(theData.itsNumberTable,
                                                iCode, pc + 1));
                            pc += 2;
                            break;
                        case TokenStream.TYPEOFNAME :
                        case TokenStream.GETBASE :
                        case TokenStream.BINDNAME :
                        case TokenStream.SETNAME :
                        case TokenStream.NAME :
                        case TokenStream.NAMEINC :
                        case TokenStream.NAMEDEC :
                        case TokenStream.STRING :
                            out.println(
                                TokenStream.tokenToName(iCode[pc] & 0xff) +
                                " \"" +
                                getString(theData.itsStringTable, iCode, pc + 1) +
                                "\"");
                            pc += 2;
                            break;
                        case TokenStream.LINE : {
                                int line = (iCode[pc + 1] << 8) | (iCode[pc + 2] & 0xFF);
                                out.println(
                                    TokenStream.tokenToName(iCode[pc] & 0xff) + " : " + line);
                                pc += 2;
                            }
                            break;
                        default :
                            out.close();
                            throw new RuntimeException("Unknown icode : "
                                                    + (iCode[pc] & 0xff)  + " @ pc : " + pc);
                    }
                    pc++;
                }
                out.close();
            }
            catch (IOException x) {}
        }
    }
    
    private static void createFunctionObject(InterpretedFunction fn, 
                                             Scriptable scope)
    {
        fn.setPrototype(ScriptableObject.getClassPrototype(scope, "Function"));
        fn.setParentScope(scope);
        InterpreterData id = fn.itsData;
        if (id.itsName.length() == 0)
            return;
        if ((id.itsFunctionType == FunctionNode.FUNCTION_STATEMENT &&
             fn.itsClosure == null) ||
            (id.itsFunctionType == FunctionNode.FUNCTION_EXPRESSION_STATEMENT &&
             fn.itsClosure != null))
        {
            ScriptRuntime.setProp(scope, fn.itsData.itsName, fn, scope);
        }
    }
    
    public static Object interpret(Context cx, Scriptable scope, 
                                   Scriptable thisObj, Object[] args, 
                                   Scriptable fnOrScript,
                                   InterpreterData theData)
        throws JavaScriptException
    {
        int i;
        Object lhs;

        final int maxStack = theData.itsMaxStack;
        final int maxVars = theData.itsVariableTable.size();
        final int maxLocals = theData.itsMaxLocals;
        
        final int VAR_SHFT = maxStack; 
        final int LOCAL_SHFT = VAR_SHFT + maxVars; 

        // stack[0 <= i < VAR_SHFT]: stack data
        // stack[VAR_SHFT <= i < LOCAL_SHFT]: variables
        // stack[LOCAL_SHFT <= i]:  // used for newtemp/usetemp etc.
        // id stack[x] == DBL_MRK, real value for stack[x] is in sDbl[x]
        
        final Object DBL_MRK = Interpreter.DBL_MRK;
        
        Object[] stack = new Object[LOCAL_SHFT + maxLocals];
        double[] sDbl = new double[LOCAL_SHFT + maxLocals];
        int stackTop = -1;
        byte[] iCode = theData.itsICode;        
        int pc = 0;
        int iCodeLength = theData.itsICodeTop;
        
        final Scriptable undefined = Undefined.instance;
        if (maxVars != 0) {
            for (i = 0; i < theData.itsVariableTable.getParameterCount(); i++) {
                if (i >= args.length)
                    stack[VAR_SHFT + i] = undefined;
                else                
                    stack[VAR_SHFT + i] = args[i];    
            }
            for ( ; i < maxVars; i++)
                stack[VAR_SHFT + i] = undefined;
        }
        
        if (theData.itsNestedFunctions != null) {
            for (i = 0; i < theData.itsNestedFunctions.length; i++)
                createFunctionObject(theData.itsNestedFunctions[i], scope);
        }        

        Object id;
        Object rhs, val;
        double valDbl;
        boolean valBln;

        int count;
        int slot;
                
        String name = null;
               
        Object[] outArgs;

        int lIntValue;
        long lLongValue;
        double lDbl;
        int rIntValue;
        double rDbl;
                
        int[] catchStack = null;
        Scriptable[] scopeStack = null;
        int tryStackTop = 0;
        InterpreterFrame frame = null;
        
        if (cx.debugger != null) {
            frame = new InterpreterFrame(scope, theData, fnOrScript);
            cx.pushFrame(frame);
        }
            
        if (theData.itsMaxTryDepth > 0) {
            // catchStack[2 * i]: catch data
            // catchStack[2 * i + 1]: finally data
            catchStack = new int[theData.itsMaxTryDepth * 2];
            scopeStack = new Scriptable[theData.itsMaxTryDepth];
        }
        
        /* Save the security domain. Must restore upon normal exit. 
         * If we exit the interpreter loop by throwing an exception,
         * set cx.interpreterSecurityDomain to null, and require the
         * catching function to restore it.
         */
        Object savedSecurityDomain = cx.interpreterSecurityDomain;
        cx.interpreterSecurityDomain = theData.securityDomain;
        Object result = undefined;
        
        while (pc < iCodeLength) {
            try {
                switch (iCode[pc] & 0xff) {
                    case TokenStream.ENDTRY :
                        tryStackTop--;
                        break;
                    case TokenStream.TRY :
                        i = getTarget(iCode, pc + 1);
                        if (i == pc) i = 0;
                        catchStack[tryStackTop * 2] = i;
                        i = getTarget(iCode, pc + 3);
                        if (i == (pc + 2)) i = 0;
                        catchStack[tryStackTop * 2 + 1] = i;
                        scopeStack[tryStackTop++] = scope;
                        pc += 4;
                        break;
                    case TokenStream.GE :
                        --stackTop;
                        rhs = stack[stackTop + 1];    
                        lhs = stack[stackTop];
                        if (rhs == DBL_MRK || lhs == DBL_MRK) {
                            rDbl = stack_double(stack, sDbl, stackTop + 1);
                            lDbl = stack_double(stack, sDbl, stackTop);
                            valBln = (rDbl == rDbl && lDbl == lDbl 
                                      && rDbl <= lDbl);
                        }
                        else {
                            valBln = (1 == ScriptRuntime.cmp_LE(rhs, lhs));
                        }
                        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
                        break;
                    case TokenStream.LE :
                        --stackTop;
                        rhs = stack[stackTop + 1];    
                        lhs = stack[stackTop];
                        if (rhs == DBL_MRK || lhs == DBL_MRK) {
                            rDbl = stack_double(stack, sDbl, stackTop + 1);
                            lDbl = stack_double(stack, sDbl, stackTop);
                            valBln = (rDbl == rDbl && lDbl == lDbl 
                                      && lDbl <= rDbl);
                        }
                        else {
                            valBln = (1 == ScriptRuntime.cmp_LE(lhs, rhs));
                        }
                        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
                        break;
                    case TokenStream.GT :
                        --stackTop;
                        rhs = stack[stackTop + 1];    
                        lhs = stack[stackTop];
                        if (rhs == DBL_MRK || lhs == DBL_MRK) {
                            rDbl = stack_double(stack, sDbl, stackTop + 1);
                            lDbl = stack_double(stack, sDbl, stackTop);
                            valBln = (rDbl == rDbl && lDbl == lDbl 
                                      && rDbl < lDbl);
                        }
                        else {
                            valBln = (1 == ScriptRuntime.cmp_LT(rhs, lhs));
                        }
                        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
                        break;
                    case TokenStream.LT :
                        --stackTop;
                        rhs = stack[stackTop + 1];    
                        lhs = stack[stackTop];
                        if (rhs == DBL_MRK || lhs == DBL_MRK) {
                            rDbl = stack_double(stack, sDbl, stackTop + 1);
                            lDbl = stack_double(stack, sDbl, stackTop);
                            valBln = (rDbl == rDbl && lDbl == lDbl 
                                      && lDbl < rDbl);
                        }
                        else {
                            valBln = (1 == ScriptRuntime.cmp_LT(lhs, rhs));
                        }
                        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
                        break;
                    case TokenStream.IN :
                        rhs = stack[stackTop];    
                        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
                        --stackTop;
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        valBln = ScriptRuntime.in(lhs, rhs, scope);
                        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
                        break;
                    case TokenStream.INSTANCEOF :
                        rhs = stack[stackTop];    
                        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
                        --stackTop;
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        valBln = ScriptRuntime.instanceOf(scope, lhs, rhs);
                        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
                        break;
                    case TokenStream.EQ :
                        --stackTop;
                        valBln = do_eq(stack, sDbl, stackTop);
                        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
                        break;
                    case TokenStream.NE :
                        --stackTop;
                        valBln = !do_eq(stack, sDbl, stackTop);
                        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
                        break;
                    case TokenStream.SHEQ :
                        --stackTop;
                        valBln = do_sheq(stack, sDbl, stackTop);
                        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
                        break;
                    case TokenStream.SHNE :
                        --stackTop;
                        valBln = !do_sheq(stack, sDbl, stackTop);
                        stack[stackTop] = valBln ? Boolean.TRUE : Boolean.FALSE;
                        break;
                    case TokenStream.IFNE :
                        val = stack[stackTop];
                        if (val != DBL_MRK) {
                            valBln = !ScriptRuntime.toBoolean(val);
                        }
                        else {
                            valDbl = sDbl[stackTop];
                            valBln = !(valDbl == valDbl && valDbl != 0.0);
                        }
                        --stackTop;
                        if (valBln) {
                            pc = getTarget(iCode, pc + 1);
                            continue;
                        }
                        pc += 2;
                        break;
                    case TokenStream.IFEQ :
                        val = stack[stackTop];
                        if (val != DBL_MRK) {
                            valBln = ScriptRuntime.toBoolean(val);
                        }
                        else {
                            valDbl = sDbl[stackTop];
                            valBln = (valDbl == valDbl && valDbl != 0.0);
                        }
                        --stackTop;
                        if (valBln) {
                            pc = getTarget(iCode, pc + 1);
                            continue;
                        }
                        pc += 2;
                        break;
                    case TokenStream.GOTO :
                        pc = getTarget(iCode, pc + 1);
                        continue;
                    case TokenStream.GOSUB :
                        sDbl[++stackTop] = pc + 3;
                        pc = getTarget(iCode, pc + 1);                        
                        continue;
                    case TokenStream.RETSUB :
                        slot = (iCode[++pc] & 0xFF);
                        pc = (int)sDbl[LOCAL_SHFT + slot];
                        continue;
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
                        if (result == DBL_MRK) 
                            result = doubleWrap(sDbl[stackTop]);
                        --stackTop;
                        break;
                    case TokenStream.RETURN :
                        result = stack[stackTop];    
                        if (result == DBL_MRK) 
                            result = doubleWrap(sDbl[stackTop]);
                        --stackTop;
                        pc = getTarget(iCode, pc + 1);
                        break;
                    case TokenStream.BITNOT :
                        rIntValue = stack_int32(stack, sDbl, stackTop);
                        stack[stackTop] = DBL_MRK;
                        sDbl[stackTop] = ~rIntValue;
                        break;
                    case TokenStream.BITAND :                
                        rIntValue = stack_int32(stack, sDbl, stackTop);
                        --stackTop;
                        lIntValue = stack_int32(stack, sDbl, stackTop);
                        stack[stackTop] = DBL_MRK;
                        sDbl[stackTop] = lIntValue & rIntValue;
                        break;
                    case TokenStream.BITOR :
                        rIntValue = stack_int32(stack, sDbl, stackTop);
                        --stackTop;
                        lIntValue = stack_int32(stack, sDbl, stackTop);
                        stack[stackTop] = DBL_MRK;
                        sDbl[stackTop] = lIntValue | rIntValue;
                        break;
                    case TokenStream.BITXOR :
                        rIntValue = stack_int32(stack, sDbl, stackTop);
                        --stackTop;
                        lIntValue = stack_int32(stack, sDbl, stackTop);
                        stack[stackTop] = DBL_MRK;
                        sDbl[stackTop] = lIntValue ^ rIntValue;
                        break;
                    case TokenStream.LSH :
                        rIntValue = stack_int32(stack, sDbl, stackTop);
                        --stackTop;
                        lIntValue = stack_int32(stack, sDbl, stackTop);
                        stack[stackTop] = DBL_MRK;
                        sDbl[stackTop] = lIntValue << rIntValue;
                        break;
                    case TokenStream.RSH :
                        rIntValue = stack_int32(stack, sDbl, stackTop);
                        --stackTop;
                        lIntValue = stack_int32(stack, sDbl, stackTop);
                        stack[stackTop] = DBL_MRK;
                        sDbl[stackTop] = lIntValue >> rIntValue;
                        break;
                    case TokenStream.URSH :
                        rIntValue = stack_int32(stack, sDbl, stackTop) & 0x1F;
                        --stackTop;
                        lLongValue = stack_uint32(stack, sDbl, stackTop);
                        stack[stackTop] = DBL_MRK;
                        sDbl[stackTop] = lLongValue >>> rIntValue;
                        break;
                    case TokenStream.ADD :
                        --stackTop;
                        do_add(stack, sDbl, stackTop);
                        break;
                    case TokenStream.SUB :
                        rDbl = stack_double(stack, sDbl, stackTop);
                        --stackTop;
                        lDbl = stack_double(stack, sDbl, stackTop);
                        stack[stackTop] = DBL_MRK;
                        sDbl[stackTop] = lDbl - rDbl;
                        break;
                    case TokenStream.NEG :
                        rDbl = stack_double(stack, sDbl, stackTop);
                        stack[stackTop] = DBL_MRK;
                        sDbl[stackTop] = -rDbl;
                        break;
                    case TokenStream.POS :
                        rDbl = stack_double(stack, sDbl, stackTop);
                        stack[stackTop] = DBL_MRK;
                        sDbl[stackTop] = rDbl;
                        break;
                    case TokenStream.MUL :
                        rDbl = stack_double(stack, sDbl, stackTop);
                        --stackTop;
                        lDbl = stack_double(stack, sDbl, stackTop);
                        stack[stackTop] = DBL_MRK;
                        sDbl[stackTop] = lDbl * rDbl;
                        break;
                    case TokenStream.DIV :
                        rDbl = stack_double(stack, sDbl, stackTop);
                        --stackTop;
                        lDbl = stack_double(stack, sDbl, stackTop);
                        stack[stackTop] = DBL_MRK;
                        // Detect the divide by zero or let Java do it ?
                        sDbl[stackTop] = lDbl / rDbl;
                        break;
                    case TokenStream.MOD :
                        rDbl = stack_double(stack, sDbl, stackTop);
                        --stackTop;
                        lDbl = stack_double(stack, sDbl, stackTop);
                        stack[stackTop] = DBL_MRK;
                        sDbl[stackTop] = lDbl % rDbl;
                        break;
                    case TokenStream.BINDNAME :
                        stack[++stackTop] = 
                                ScriptRuntime.bind(scope, 
                                         getString(theData.itsStringTable, 
                                                   iCode, pc + 1));
                        pc += 2;
                        break;
                    case TokenStream.GETBASE :
                        stack[++stackTop] =
                                ScriptRuntime.getBase(scope, 
                                         getString(theData.itsStringTable,
                                                                iCode, pc + 1));
                        pc += 2;
                        break;
                    case TokenStream.SETNAME :
                        rhs = stack[stackTop];    
                        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
                        --stackTop;
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        // what about class cast exception here ?
                        stack[stackTop] = ScriptRuntime.setName
                            ((Scriptable)lhs, rhs, scope, 
                             getString(theData.itsStringTable, iCode, pc + 1));
                        pc += 2;
                        break;
                    case TokenStream.DELPROP :
                        rhs = stack[stackTop];    
                        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
                        --stackTop;
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        stack[stackTop] = ScriptRuntime.delete(lhs, rhs);
                        break;
                    case TokenStream.GETPROP :
                        name = (String)stack[stackTop];
                        --stackTop;
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        stack[stackTop] 
                                = ScriptRuntime.getProp(lhs, name, scope);
                        break;
                    case TokenStream.SETPROP :
                        rhs = stack[stackTop];    
                        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
                        --stackTop;
                        name = (String)stack[stackTop];
                        --stackTop;
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        stack[stackTop] 
                                = ScriptRuntime.setProp(lhs, name, rhs, scope);
                        break;
                    case TokenStream.GETELEM :
                        id = stack[stackTop];    
                        if (id == DBL_MRK) id = doubleWrap(sDbl[stackTop]);
                        --stackTop;
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        stack[stackTop] 
                                = ScriptRuntime.getElem(lhs, id, scope);
                        break;
                    case TokenStream.SETELEM :
                        rhs = stack[stackTop];    
                        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
                        --stackTop;
                        id = stack[stackTop];    
                        if (id == DBL_MRK) id = doubleWrap(sDbl[stackTop]);
                        --stackTop;
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        stack[stackTop] 
                                = ScriptRuntime.setElem(lhs, id, rhs, scope);
                        break;
                    case TokenStream.PROPINC :
                        name = (String)stack[stackTop];
                        --stackTop;
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        stack[stackTop] 
                                = ScriptRuntime.postIncrement(lhs, name, scope);
                        break;
                    case TokenStream.PROPDEC :
                        name = (String)stack[stackTop];
                        --stackTop;
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        stack[stackTop] 
                                = ScriptRuntime.postDecrement(lhs, name, scope);
                        break;
                    case TokenStream.ELEMINC :
                        rhs = stack[stackTop];    
                        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
                        --stackTop;
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        stack[stackTop] 
                           = ScriptRuntime.postIncrementElem(lhs, rhs, scope);
                        break;
                    case TokenStream.ELEMDEC :
                        rhs = stack[stackTop];    
                        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
                        --stackTop;
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        stack[stackTop] 
                           = ScriptRuntime.postDecrementElem(lhs, rhs, scope);
                        break;
                    case TokenStream.GETTHIS :
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        stack[stackTop] 
                            = ScriptRuntime.getThis((Scriptable)lhs);
                        break;
                    case TokenStream.NEWTEMP :
                        slot = (iCode[++pc] & 0xFF);
                        stack[LOCAL_SHFT + slot] = stack[stackTop];
                        sDbl[LOCAL_SHFT + slot] = sDbl[stackTop];
                        break;
                    case TokenStream.USETEMP :
                        slot = (iCode[++pc] & 0xFF);
                        ++stackTop;
                        stack[stackTop] = stack[LOCAL_SHFT + slot];
                        sDbl[stackTop] = sDbl[LOCAL_SHFT + slot];
                        break;
                    case TokenStream.CALLSPECIAL :
                        int lineNum = (iCode[pc + 1] << 8) 
                                      | (iCode[pc + 2] & 0xFF);   
                        name = getString(theData.itsStringTable, iCode, pc + 3);
                        count = (iCode[pc + 5] << 8) | (iCode[pc + 6] & 0xFF);
                        outArgs = new Object[count];
                        for (i = count - 1; i >= 0; i--) {
                            val = stack[stackTop];    
                            if (val == DBL_MRK) 
                                val = doubleWrap(sDbl[stackTop]);
                            outArgs[i] = val;
                            --stackTop;
                        }
                        rhs = stack[stackTop];    
                        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
                        --stackTop;
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        stack[stackTop] = ScriptRuntime.callSpecial(
                                            cx, lhs, rhs, outArgs, 
                                            thisObj, scope, name, lineNum);
                        pc += 6;
                        break;
                    case TokenStream.CALL :
                        count = (iCode[pc + 3] << 8) | (iCode[pc + 4] & 0xFF);
                        outArgs = new Object[count];
                        for (i = count - 1; i >= 0; i--) {
                            val = stack[stackTop];    
                            if (val == DBL_MRK) 
                                val = doubleWrap(sDbl[stackTop]);
                            outArgs[i] = val;
                            --stackTop;
                        }
                        rhs = stack[stackTop];    
                        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
                        --stackTop;
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        if (lhs == undefined) {
                            lhs = getString(theData.itsStringTable, iCode, 
                                            pc + 1);
                        }
                        Scriptable calleeScope = scope;
                        if (theData.itsNeedsActivation) {
                            calleeScope = ScriptableObject.
                                getTopLevelScope(scope);
                        }
                        stack[stackTop] = ScriptRuntime.call(cx, lhs, rhs, 
                                                             outArgs, 
                                                             calleeScope);
                        pc += 4;                                                            
                        break;
                    case TokenStream.NEW :
                        count = (iCode[pc + 3] << 8) | (iCode[pc + 4] & 0xFF);
                        outArgs = new Object[count];
                        for (i = count - 1; i >= 0; i--) {
                            val = stack[stackTop];    
                            if (val == DBL_MRK) 
                                val = doubleWrap(sDbl[stackTop]);
                            outArgs[i] = val;
                            --stackTop;
                        }
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        if (lhs == undefined && 
                            (iCode[pc+1] << 8) + (iCode[pc+2] & 0xFF) != -1) 
                        {
                            // special code for better error message for call 
                            //  to undefined
                            lhs = getString(theData.itsStringTable, iCode, 
                                            pc + 1);
                        }
                        stack[stackTop] = ScriptRuntime.newObject(cx, lhs, 
                                                                  outArgs, 
                                                                  scope);
                        pc += 4;                                                            
                        break;
                    case TokenStream.TYPEOF :
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        stack[stackTop] = ScriptRuntime.typeof(lhs);
                        break;
                    case TokenStream.TYPEOFNAME :
                        name = getString(theData.itsStringTable, iCode, pc + 1);
                        stack[++stackTop] 
                                    = ScriptRuntime.typeofName(scope, name);
                        pc += 2;
                        break;
                    case TokenStream.STRING :
                        stack[++stackTop] = getString(theData.itsStringTable,
                                                                iCode, pc + 1);
                        pc += 2;
                        break;
                    case TokenStream.NUMBER :
                        ++stackTop;
                        stack[stackTop] = DBL_MRK;
                        sDbl[stackTop] = getNumber(theData.itsNumberTable,
                                                   iCode, pc + 1);
                        pc += 2;
                        break;
                    case TokenStream.NAME :
                        stack[++stackTop] = ScriptRuntime.name(scope,
                                       getString(theData.itsStringTable,
                                                                iCode, pc + 1));
                        pc += 2;
                        break;
                    case TokenStream.NAMEINC :
                        stack[++stackTop] = ScriptRuntime.postIncrement(scope,
                                       getString(theData.itsStringTable,
                                                                iCode, pc + 1));
                        pc += 2;
                        break;
                    case TokenStream.NAMEDEC :
                        stack[++stackTop] = ScriptRuntime.postDecrement(scope,
                                       getString(theData.itsStringTable,
                                                                iCode, pc + 1));
                        pc += 2;
                        break;
                    case TokenStream.SETVAR :
                        slot = (iCode[++pc] & 0xFF);
                        stack[VAR_SHFT + slot] = stack[stackTop];
                        sDbl[VAR_SHFT + slot] = sDbl[stackTop];
                        break;
                    case TokenStream.GETVAR :
                        slot = (iCode[++pc] & 0xFF);
                        ++stackTop;
                        stack[stackTop] = stack[VAR_SHFT + slot];
                        sDbl[stackTop] = sDbl[VAR_SHFT + slot];
                        break;
                    case TokenStream.VARINC :
                        slot = (iCode[++pc] & 0xFF);
                        ++stackTop;
                        stack[stackTop] = stack[VAR_SHFT + slot];
                        sDbl[stackTop] = sDbl[VAR_SHFT + slot];
                        stack[VAR_SHFT + slot] = DBL_MRK;
                        sDbl[VAR_SHFT + slot] 
                            = stack_double(stack, sDbl, stackTop) + 1.0;
                        break;
                    case TokenStream.VARDEC :
                        slot = (iCode[++pc] & 0xFF);
                        ++stackTop;
                        stack[stackTop] = stack[VAR_SHFT + slot];
                        sDbl[stackTop] = sDbl[VAR_SHFT + slot];
                        stack[VAR_SHFT + slot] = DBL_MRK;
                        sDbl[VAR_SHFT + slot] 
                            = stack_double(stack, sDbl, stackTop) - 1.0;
                        break;
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
                    case TokenStream.THROW :
                        cx.interpreterSecurityDomain = null;
                        result = stack[stackTop];
                        if (result == DBL_MRK) 
                            result = doubleWrap(sDbl[stackTop]);
                        --stackTop;
                        throw new JavaScriptException(result);
                    case TokenStream.JTHROW :
                        cx.interpreterSecurityDomain = null;
                        result = stack[stackTop];
                        // No need to check for DBL_MRK: result is Exception
                        --stackTop;
                        if (result instanceof JavaScriptException)
                            throw (JavaScriptException)result;
                        else
                            throw (RuntimeException)result;
                    case TokenStream.ENTERWITH :
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        --stackTop;
                        scope = ScriptRuntime.enterWith(lhs, scope);
                        break;
                    case TokenStream.LEAVEWITH :
                        scope = ScriptRuntime.leaveWith(scope);
                        break;
                    case TokenStream.NEWSCOPE :
                        stack[++stackTop] = ScriptRuntime.newScope();
                        break;
                    case TokenStream.ENUMINIT :
                        slot = (iCode[++pc] & 0xFF);
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        --stackTop;
                        stack[LOCAL_SHFT + slot] 
                            = ScriptRuntime.initEnum(lhs, scope);
                        break;
                    case TokenStream.ENUMNEXT :
                        slot = (iCode[++pc] & 0xFF);
                        val = stack[LOCAL_SHFT + slot];    
                        ++stackTop;
                        stack[stackTop] = ScriptRuntime.
                            nextEnum((Enumeration)val);
                        break;
                    case TokenStream.GETPROTO :
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        stack[stackTop] = ScriptRuntime.getProto(lhs, scope);
                        break;
                    case TokenStream.GETPARENT :
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        stack[stackTop] = ScriptRuntime.getParent(lhs);
                        break;
                    case TokenStream.GETSCOPEPARENT :
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        stack[stackTop] = ScriptRuntime.getParent(lhs, scope);
                        break;
                    case TokenStream.SETPROTO :
                        rhs = stack[stackTop];    
                        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
                        --stackTop;
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        stack[stackTop]
                                = ScriptRuntime.setProto(lhs, rhs, scope);
                        break;
                    case TokenStream.SETPARENT :
                        rhs = stack[stackTop];    
                        if (rhs == DBL_MRK) rhs = doubleWrap(sDbl[stackTop]);
                        --stackTop;
                        lhs = stack[stackTop];    
                        if (lhs == DBL_MRK) lhs = doubleWrap(sDbl[stackTop]);
                        stack[stackTop]
                                = ScriptRuntime.setParent(lhs, rhs, scope);
                        break;
                    case TokenStream.SCOPE :
                        stack[++stackTop] = scope;
                        break;
                    case TokenStream.CLOSURE :
                        i = (iCode[pc + 1] << 8) | (iCode[pc + 2] & 0xFF);
                        stack[++stackTop] 
                            = new InterpretedFunction(
                                    theData.itsNestedFunctions[i],
                                    scope, cx);
                        createFunctionObject(
                              (InterpretedFunction)stack[stackTop], scope);
                        pc += 2;
                        break;
                    case TokenStream.OBJECT :
                        i = (iCode[pc + 1] << 8) | (iCode[pc + 2] & 0xFF);                    
                        stack[++stackTop] = theData.itsRegExpLiterals[i];
                        pc += 2;
                        break;
                    case TokenStream.SOURCEFILE :    
                        cx.interpreterSourceFile = theData.itsSourceFile;
                        break;
                    case TokenStream.LINE :    
                    case TokenStream.BREAKPOINT :
                        i = (iCode[pc + 1] << 8) | (iCode[pc + 2] & 0xFF);                    
                        cx.interpreterLine = i;
                        if (frame != null)
                            frame.setLineNumber(i);
                        if ((iCode[pc] & 0xff) == TokenStream.BREAKPOINT ||
                            cx.inLineStepMode) 
                        {
                            cx.getDebuggableEngine().
                                getDebugger().handleBreakpointHit(cx);
                        }
                        pc += 2;
                        break;
                    default :
                        dumpICode(theData);
                        throw new RuntimeException("Unknown icode : "
                                     + (iCode[pc] & 0xff) + " @ pc : " + pc);
                }
                pc++;
            }
            catch (EcmaError ee) {
                if (cx.debugger != null)
                    cx.debugger.handleExceptionThrown(cx, ee.getErrorObject());
                // an offical ECMA error object, 
                // handle as if it were a JavaScriptException
                stackTop = 0;
                cx.interpreterSecurityDomain = null;
                if (tryStackTop > 0) {
                    pc = catchStack[--tryStackTop * 2];
                    scope = scopeStack[tryStackTop];
                    if (pc == 0) {
                        pc = catchStack[tryStackTop * 2 + 1];
                        if (pc == 0) {
                            if (frame != null)
                                cx.popFrame();
                            throw ee;
                        }
                    }
                    stack[0] = ee.getErrorObject();
                } else {
                    if (frame != null)
                        cx.popFrame();
                    throw ee;
                }
                // We caught an exception; restore this function's 
                // security domain.
                cx.interpreterSecurityDomain = theData.securityDomain;
            }
            catch (JavaScriptException jsx) {
                if (cx.debugger != null) {
                    cx.debugger.handleExceptionThrown(cx, 
                        ScriptRuntime.unwrapJavaScriptException(jsx));
                }
                stackTop = 0;
                cx.interpreterSecurityDomain = null;
                if (tryStackTop > 0) {
                    pc = catchStack[--tryStackTop * 2];
                    scope = scopeStack[tryStackTop];
                    if (pc == 0) {
                        pc = catchStack[tryStackTop * 2 + 1];
                        if (pc == 0) {
                            if (frame != null)
                                cx.popFrame();
                            throw jsx;
                        }
                        stack[0] = jsx;
                    } else {
                        stack[0] = ScriptRuntime.unwrapJavaScriptException(jsx);
                    }
                } else {
                    if (frame != null)
                        cx.popFrame();
                    throw jsx;
                }
                // We caught an exception; restore this function's 
                // security domain.
                cx.interpreterSecurityDomain = theData.securityDomain;
            }
            catch (RuntimeException jx) {
                if (cx.debugger != null)
                    cx.debugger.handleExceptionThrown(cx, jx);
                cx.interpreterSecurityDomain = null;
                if (tryStackTop > 0) {
                    stackTop = 0;
                    stack[0] = jx;
                    pc = catchStack[--tryStackTop * 2 + 1];
                    scope = scopeStack[tryStackTop];
                    if (pc == 0) {
                        if (frame != null)
                            cx.popFrame();
                        throw jx;
                    }
                } else {
                    if (frame != null)
                        cx.popFrame();
                    throw jx;
                }
                // We caught an exception; restore this function's 
                // security domain.
                cx.interpreterSecurityDomain = theData.securityDomain;
            }
        }
        cx.interpreterSecurityDomain = savedSecurityDomain;
        if (frame != null)
            cx.popFrame();
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
    
    private static long stack_uint32(Object[] stack, double[] stackDbl, int i) {
        Object x = stack[i];
        return (x != DBL_MRK)
            ? ScriptRuntime.toUint32(x)
            : ScriptRuntime.toUint32(stackDbl[i]);
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
            }
            else {
                do_add(lhs, rDbl, stack, stackDbl, stackTop, true);
            }
        }
        else if (lhs == DBL_MRK) {
            do_add(rhs, stackDbl[stackTop], stack, stackDbl, stackTop, false);
        }
        else {
            if (lhs instanceof Scriptable)
                lhs = ((Scriptable) lhs).getDefaultValue(null);
            if (rhs instanceof Scriptable)
                rhs = ((Scriptable) rhs).getDefaultValue(null);
            if (lhs instanceof String || rhs instanceof String) {
                stack[stackTop] = ScriptRuntime.toString(lhs)
                                   + ScriptRuntime.toString(rhs);
            }
            else {
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
            if (lhs == Undefined.instance) { lhs = ScriptRuntime.NaNobj; }
            lhs = ((Scriptable)lhs).getDefaultValue(null);
        }
        if (lhs instanceof String) {
            if (left_right_order) {
                stack[stackTop] = (String)lhs + ScriptRuntime.toString(rDbl);
            }
            else {
                stack[stackTop] = ScriptRuntime.toString(rDbl) + (String)lhs;
            }
        }
        else {
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
            }
            else {
                result = do_eq(stackDbl[stackTop + 1], lhs);
            }
        }
        else {
            if (lhs == DBL_MRK) {
                result = do_eq(stackDbl[stackTop], rhs);
            }
            else {
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
            }
            else {
                result = (lhs instanceof Number);
                if (result) {
                    result = (((Number)lhs).doubleValue() == rDbl);
                }
            }
        }
        else if (rhs instanceof Number) {
            double rDbl = ((Number)rhs).doubleValue();
            if (lhs == DBL_MRK) {
                result = (stackDbl[stackTop] == rDbl);
            }
            else {
                result = (lhs instanceof Number);
                if (result) {
                    result = (((Number)lhs).doubleValue() == rDbl);
                }
            }
        }
        else {
            result = ScriptRuntime.shallowEq(lhs, rhs);
        }
        return result;
    }
    
    private int version;
    private boolean inLineStepMode;
    private StringBuffer debugSource;

    private static final Object DBL_MRK = new Object();
}
