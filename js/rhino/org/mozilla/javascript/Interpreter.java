/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package org.mozilla.javascript;

import java.io.*;
import java.util.Vector;
import java.util.Enumeration;

public class Interpreter extends LabelTable {
    
    public static final boolean printICode = false;
    
    public IRFactory createIRFactory(TokenStream ts, 
                                     ClassNameHelper nameHelper) 
    {
        return new IRFactory(ts);
    }
    
    public Node transform(Node tree, TokenStream ts) {
        return (new NodeTransformer()).transform(tree, null, ts);
    }
    
    public Object compile(Context cx, Scriptable scope, Node tree, 
                          Object securityDomain,
                          SecuritySupport securitySupport,
                          ClassNameHelper nameHelper)
        throws IOException
    {
        version = cx.getLanguageVersion();
        itsData = new InterpreterData(0, 0, 0, securityDomain);
        if (tree instanceof FunctionNode) {
            FunctionNode f = (FunctionNode) tree;
            InterpretedFunction result = 
                generateFunctionICode(cx, scope, f, securityDomain);
            createFunctionObject(result, scope);
            return result;
        }
        return generateScriptICode(cx, scope, tree, securityDomain);
    }
    
    public void defineJavaAdapter(Scriptable scope) {
        // do nothing; only the code generator can define the JavaAdapter
        try {
        	String adapterName = System.getProperty("org.mozilla.javascript.JavaAdapter");
        	if (adapterName != null) {
        		Class adapterClass = Class.forName(adapterName);
				ScriptableObject.defineClass(scope, adapterClass);
			}
        } catch (Exception e) {
        }
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
        Object v = scope.get("RegExp", scope);   
        Object[] result = new Object[regexps.size()];
        for (int i = 0; i < regexps.size(); i++) {
            Node regexp = (Node) regexps.elementAt(i);
            Node left = regexp.getFirstChild();
            Node right = regexp.getLastChild();
            Object[] args = new Object[left == right ? 1 : 2];
            args[0] = left.getString();
            if (left != right)
                args[1] = right.getString();
            try {
                result[i] = ScriptRuntime.call(cx, v, scope, args);
            } catch (JavaScriptException e) {
                result[i] = null;
            }
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
        itsFunctionList = (Vector) tree.getProp(Node.FUNCTION_PROP);        
        if (itsFunctionList != null)
            generateNestedFunctions(scope, cx, securityDomain);
        Object[] regExpLiterals = null;
        Vector regexps = (Vector)tree.getProp(Node.REGEXP_PROP);
        if (regexps != null) 
            regExpLiterals = generateRegExpLiterals(cx, scope, regexps);
        
        VariableTable varTable = (VariableTable)tree.getProp(Node.VARS_PROP);
        generateICodeFromTree(tree, varTable, false, securityDomain);
        itsData.itsNestedFunctions = itsNestedFunctions;
        itsData.itsRegExpLiterals = regExpLiterals;
        if (printICode) dumpICode(itsData);
                                                               
        return new InterpretedScript(itsData, cx);
    }
    
    private void generateNestedFunctions(Scriptable scope,
                                         Context cx, 
                                         Object securityDomain)
    {
        itsNestedFunctions = new InterpretedFunction[itsFunctionList.size()];
        for (short i = 0; i < itsFunctionList.size(); i++) {
            FunctionNode def = (FunctionNode)itsFunctionList.elementAt(i);
            Interpreter jsi = new Interpreter();
            jsi.itsData = new InterpreterData(0, 0, 0, securityDomain);
            jsi.itsInFunctionFlag = true;
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
        generateICodeFromTree(theFunction.getLastChild(), 
                               varTable, theFunction.requiresActivation(),
                               securityDomain);
            
        itsData.itsName = theFunction.getFunctionName();
        itsData.itsSource = (String)theFunction.getProp(Node.SOURCE_PROP);
        itsData.itsNestedFunctions = itsNestedFunctions;
        itsData.itsRegExpLiterals = regExpLiterals;
        if (printICode) dumpICode(itsData);            
            
        return new InterpretedFunction(itsData, cx);
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
    
    private void updateLineNumber(Node node)
    {
        Object datum = node.getDatum();
        if (datum == null || !(datum instanceof Number))
            return;
        itsLineNumber = ((Number) datum).shortValue();
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
    
    private int generateICode(Node node, int iCodeTop)
    {
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
                updateLineNumber(node);
                while (child != null) {
                    if (child.getType() != TokenStream.FUNCTION) 
                        iCodeTop = generateICode(child, iCodeTop);
                    child = child.getNextSibling();
                }
                break;

            case TokenStream.CASE :
                updateLineNumber(node);
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
                updateLineNumber(node);
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
                    updateLineNumber(node);
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
                    int childCount = 0;
                    while (child != null) {
                        iCodeTop = generateICode(child, iCodeTop);
                        child = child.getNextSibling();
                        childCount++;
                    }
                    if (node.getProp(Node.SPECIALCALL_PROP) != null) {
                        // embed line number and source filename
                        iCodeTop = addByte((byte) TokenStream.CALLSPECIAL, iCodeTop);
                        iCodeTop = addByte((byte)(itsLineNumber >> 8), iCodeTop);
                        iCodeTop = addByte((byte)(itsLineNumber & 0xff), iCodeTop);
                        iCodeTop = addString(itsSourceFile, iCodeTop);
                    }                        
                    else
                        iCodeTop = addByte((byte) type, iCodeTop);
                    
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
                    iCodeTop = addGoto(node, TokenStream.GOSUB,
                                                        iCodeTop);
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
                    if (itsInFunctionFlag)
                        index = itsData.itsVariableTable.getOrdinal(name);
                    if (index == -1) {                    
                        iCodeTop = addByte((byte) TokenStream.TYPEOFNAME, iCodeTop);
                        iCodeTop = addString(name, iCodeTop);
                    }
                    else {
                        iCodeTop = addByte((byte) TokenStream.GETVAR, iCodeTop);
                        iCodeTop = addByte((byte)index, iCodeTop);
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
                    Number num = (Number)node.getDatum();
                    if (num.doubleValue() == 0.0)
                        iCodeTop = addByte((byte) TokenStream.ZERO, iCodeTop);
                    else
                        if (num.doubleValue() == 1.0)
                            iCodeTop = addByte((byte) TokenStream.ONE, iCodeTop);
                        else {
                            iCodeTop = addByte((byte) TokenStream.NUMBER, iCodeTop);
                       	    iCodeTop = addNumber(num, iCodeTop);
                       	}
                }
                itsStackDepth++;
                if (itsStackDepth > itsData.itsMaxStack)
                    itsData.itsMaxStack = itsStackDepth;
                break;

            case TokenStream.POP :
            case TokenStream.POPV :
                updateLineNumber(node);
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
                    while (child != null) {
                        if (lastChild == catchTarget) {
                            itsStackDepth = 1;
                            if (itsStackDepth > itsData.itsMaxStack)
                                itsData.itsMaxStack = itsStackDepth;
                        }
                        iCodeTop = generateICode(child, iCodeTop);
                        lastChild = child;
                        child = child.getNextSibling();
                    }
                    itsStackDepth = 0;
                    iCodeTop = addByte((byte) TokenStream.ENDTRY, iCodeTop);
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
                updateLineNumber(node);
                iCodeTop = generateICode(child, iCodeTop);
                iCodeTop = addByte((byte) TokenStream.THROW, iCodeTop);
                itsStackDepth--;
                break;
                
            case TokenStream.RETURN :
                updateLineNumber(node);
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
        iCodeTop = addByte((byte)(itsData.itsStringTableIndex >> 8), iCodeTop);
        iCodeTop = addByte((byte)(itsData.itsStringTableIndex & 0xFF), iCodeTop);
        if (itsData.itsStringTable.length == itsData.itsStringTableIndex) {
            String[] sa = new String[itsData.itsStringTableIndex * 2];
            System.arraycopy(itsData.itsStringTable, 0, sa, 0, itsData.itsStringTableIndex);
            itsData.itsStringTable = sa;
        }
        itsData.itsStringTable[itsData.itsStringTableIndex++] = str;
        return iCodeTop;
    }
    
    private final int addNumber(Number num, int iCodeTop)
    {
        iCodeTop = addByte((byte)(itsData.itsNumberTableIndex >> 8), iCodeTop);
        iCodeTop = addByte((byte)(itsData.itsNumberTableIndex & 0xFF), iCodeTop);
        if (itsData.itsNumberTable.length == itsData.itsNumberTableIndex) {
            Number[] na = new Number[itsData.itsNumberTableIndex * 2];
            System.arraycopy(itsData.itsNumberTable, 0, na, 0, itsData.itsNumberTableIndex);
            itsData.itsNumberTable = na;
        }
        itsData.itsNumberTable[itsData.itsNumberTableIndex++] = num;
        return iCodeTop;
    }
    
    private static String getString(String[] theStringTable, byte[] iCode, 
                                    int pc)
    {
        int index = (iCode[pc] << 8) + (iCode[pc + 1] & 0xFF);
        return theStringTable[index];
    }
    
    private static Number getNumber(Number[] theNumberTable, byte[] iCode, 
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
                    switch (iCode[pc]) {
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
                        case TokenStream.FALSE :
                        case TokenStream.TRUE :
                        case TokenStream.UNDEFINED :
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
                                int count = (iCode[pc + 1] << 8) | (iCode[pc + 2] & 0xFF);
                                out.println(
                                    TokenStream.tokenToName(iCode[pc] & 0xff) +
                                    " " + count);
                                pc += 2;
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
    
    private static final Double zero = new Double(0.0);
    private static final Double one = new Double(1.0);
    
    private static void createFunctionObject(InterpretedFunction fn, 
                                             Scriptable scope)
    {
        fn.setPrototype(ScriptableObject.getClassPrototype(scope, "Function"));
        fn.setParentScope(scope);
        if (fn.itsData.itsName.length() > 0)
            ScriptRuntime.setName(scope, fn, scope, fn.itsData.itsName);
    }
    
    public static Object interpret(InterpreterData theData)
        throws JavaScriptException
    {
        Object lhs;
        Object[] stack = new Object[theData.itsMaxStack];
        int stackTop = -1;
        byte[] iCode = theData.itsICode;        
        int pc = 0;
        int iCodeLength = theData.itsICodeTop;
        
        Object[] local = null;        // used for newtemp/usetemp etc.
        if (theData.itsMaxLocals > 0)
            local = new Object[theData.itsMaxLocals];        
            
        Object[] vars = null;
        
        int i = theData.itsVariableTable.size();
        if (i > 0) {
            vars = new Object[i];
            for (i = 0; i < theData.itsVariableTable.getParameterCount(); i++) {
                if (i >= theData.itsInArgs.length)
                    vars[i] = Undefined.instance;
                else                
                    vars[i] = theData.itsInArgs[i];    
            }
            for ( ; i < vars.length; i++)
                vars[i] = Undefined.instance;
        }
        
        Context cx = theData.itsCX;
        Scriptable scope = theData.itsScope;

        if (theData.itsNestedFunctions != null) {
            for (i = 0; i < theData.itsNestedFunctions.length; i++)
                createFunctionObject(theData.itsNestedFunctions[i], scope);
        }        

        Object id;
        Object rhs;

        int count;
        int slot;
                
        String name;
               
        Object[] outArgs;

        int lIntValue;
        long lLongValue;
        int rIntValue;
                
        int[] catchStack = null;
        int[] finallyStack = null;
        Scriptable[] scopeStack = null;
        int tryStackTop = 0;
        
        if (theData.itsMaxTryDepth > 0) {
            catchStack = new int[theData.itsMaxTryDepth];
            finallyStack = new int[theData.itsMaxTryDepth];
            scopeStack = new Scriptable[theData.itsMaxTryDepth];
        }
        
        /* Save the security domain. Must restore upon normal exit. 
         * If we exit the interpreter loop by throwing an exception,
         * set cx.interpreterSecurityDomain to null, and require the
         * catching function to restore it.
         */
        Object savedSecurityDomain = cx.interpreterSecurityDomain;
        cx.interpreterSecurityDomain = theData.securityDomain;
        Object result = Undefined.instance;
        
        while (pc < iCodeLength) {
            try {
                switch (iCode[pc]) {
                    case TokenStream.ENDTRY :
                        tryStackTop--;
                        break;
                    case TokenStream.TRY :
                        i = getTarget(iCode, pc + 1);
                        if (i == pc) i = 0;
                        catchStack[tryStackTop] = i;
                        i = getTarget(iCode, pc + 3);
                        if (i == (pc + 2)) i = 0;
                        finallyStack[tryStackTop] = i;
                        scopeStack[tryStackTop++] = scope;
                        pc += 4;
                        break;
                    case TokenStream.GE :
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] = ScriptRuntime.cmp_LEB(rhs, lhs);
                        break;
                    case TokenStream.LE :
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] = ScriptRuntime.cmp_LEB(lhs, rhs);
                        break;
                    case TokenStream.GT :
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] = ScriptRuntime.cmp_LTB(rhs, lhs);
                        break;
                    case TokenStream.LT :
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] = ScriptRuntime.cmp_LTB(lhs, rhs);
                        break;
                    case TokenStream.IN :
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] 
                                = new Boolean(ScriptRuntime.in(lhs, rhs));
                        break;
                    case TokenStream.INSTANCEOF :
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop]
                             = new Boolean(ScriptRuntime.instanceOf(lhs, rhs));
                        break;
                    case TokenStream.EQ :
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] = ScriptRuntime.eqB(rhs, lhs);
                        break;
                    case TokenStream.NE :
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] = ScriptRuntime.neB(lhs, rhs);
                        break;
                    case TokenStream.SHEQ :
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] = ScriptRuntime.seqB(lhs, rhs);
                        break;
                    case TokenStream.SHNE :
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] = ScriptRuntime.sneB(lhs, rhs);
                        break;
                    case TokenStream.IFNE :
                        if (!ScriptRuntime.toBoolean(stack[stackTop--])) {
                            pc = getTarget(iCode, pc + 1);
                            continue;
                        }
                        pc += 2;
                        break;
                    case TokenStream.IFEQ :
                        if (ScriptRuntime.toBoolean(stack[stackTop--])) {
                            pc = getTarget(iCode, pc + 1);
                            continue;
                        }
                        pc += 2;
                        break;
                    case TokenStream.GOTO :
                        pc = getTarget(iCode, pc + 1);
                        continue;
                    case TokenStream.GOSUB :
                        stack[++stackTop] = new Integer(pc + 3);
                        pc = getTarget(iCode, pc + 1);                        
                        continue;
                    case TokenStream.RETSUB :
                        slot = (iCode[++pc] & 0xFF);
                        pc = ((Integer)local[slot]).intValue();
                        continue;
                    case TokenStream.POP :
                        stackTop--;
                        break;
                    case TokenStream.DUP :
                        stack[stackTop + 1] = stack[stackTop];
                        stackTop++;
                        break;
                    case TokenStream.POPV :
                        result = stack[stackTop--];
                        break;
                    case TokenStream.RETURN :
                        result = stack[stackTop--];
                        pc = getTarget(iCode, pc + 1);
                        break;
                    case TokenStream.BITNOT :                
                        rIntValue = ScriptRuntime.toInt32(stack[stackTop]);
                        stack[stackTop] = new Double(~rIntValue);
                        break;
                    case TokenStream.BITAND :                
                        rIntValue = ScriptRuntime.toInt32(stack[stackTop--]);
                        lIntValue = ScriptRuntime.toInt32(stack[stackTop]);
                        stack[stackTop] = new Double(lIntValue & rIntValue);
                        break;
                    case TokenStream.BITOR :
                        rIntValue = ScriptRuntime.toInt32(stack[stackTop--]);
                        lIntValue = ScriptRuntime.toInt32(stack[stackTop]);
                        stack[stackTop] = new Double(lIntValue | rIntValue);
                        break;
                    case TokenStream.BITXOR :
                        rIntValue = ScriptRuntime.toInt32(stack[stackTop--]);
                        lIntValue = ScriptRuntime.toInt32(stack[stackTop]);
                        stack[stackTop] = new Double(lIntValue ^ rIntValue);
                        break;
                    case TokenStream.LSH :
                        rIntValue = ScriptRuntime.toInt32(stack[stackTop--]);
                        lIntValue = ScriptRuntime.toInt32(stack[stackTop]);
                        stack[stackTop] = new Double(lIntValue << rIntValue);
                        break;
                    case TokenStream.RSH :
                        rIntValue = ScriptRuntime.toInt32(stack[stackTop--]);
                        lIntValue = ScriptRuntime.toInt32(stack[stackTop]);
                        stack[stackTop] = new Double(lIntValue >> rIntValue);
                        break;
                    case TokenStream.URSH :
                        rIntValue = (ScriptRuntime.toInt32(stack[stackTop--]) & 0x1F);
                        lLongValue = ScriptRuntime.toUint32(stack[stackTop]);
                        stack[stackTop] = new Double(lLongValue >>> rIntValue);
                        break;
                    case TokenStream.ADD :
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] = ScriptRuntime.add(lhs, rhs);
                        break;
                    case TokenStream.SUB :
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] = new Double(ScriptRuntime.toNumber(lhs)
                                                    - ScriptRuntime.toNumber(rhs));
                        break;
                    case TokenStream.NEG :
                        rhs = stack[stackTop];
                        stack[stackTop] = new Double(-ScriptRuntime.toNumber(rhs));
                        break;
                    case TokenStream.POS :
                        rhs = stack[stackTop];
                        stack[stackTop] = new Double(ScriptRuntime.toNumber(rhs));
                        break;
                    case TokenStream.MUL :
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] = new Double(ScriptRuntime.toNumber(lhs)
                                                    * ScriptRuntime.toNumber(rhs));
                        break;
                    case TokenStream.DIV :
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        // Detect the divide by zero or let Java do it ?
                        stack[stackTop] = new Double(ScriptRuntime.toNumber(lhs)
                                                    / ScriptRuntime.toNumber(rhs));
                        break;
                    case TokenStream.MOD :
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] = new Double(ScriptRuntime.toNumber(lhs)
                                                    % ScriptRuntime.toNumber(rhs));
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
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        // what about class cast exception here ?
                        stack[stackTop] = 
                                ScriptRuntime.setName((Scriptable)lhs, rhs, scope, 
                                         getString(theData.itsStringTable,
                                                                iCode, pc + 1));
                        pc += 2;
                        break;
                    case TokenStream.DELPROP :
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] 
                                = ScriptRuntime.delete(lhs, rhs);
                        break;
                    case TokenStream.GETPROP :
                        name = (String)stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] 
                                = ScriptRuntime.getProp(lhs, name, scope);
                        break;
                    case TokenStream.SETPROP :
                        rhs = stack[stackTop--];
                        name = (String)stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] 
                                = ScriptRuntime.setProp(lhs, name, rhs, scope);
                        break;
                    case TokenStream.GETELEM :
                        id = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] 
                                = ScriptRuntime.getElem(lhs, id, scope);
                        break;
                    case TokenStream.SETELEM :
                        rhs = stack[stackTop--];
                        id = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] 
                                = ScriptRuntime.setElem(lhs, id, rhs, scope);
                        break;
                    case TokenStream.PROPINC :
                        name = (String)stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] 
                                = ScriptRuntime.postIncrement(lhs, name, scope);
                        break;
                    case TokenStream.PROPDEC :
                        name = (String)stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] 
                                = ScriptRuntime.postDecrement(lhs, name, scope);
                        break;
                    case TokenStream.ELEMINC :
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] 
                           = ScriptRuntime.postIncrementElem(lhs, rhs, scope);
                        break;
                    case TokenStream.ELEMDEC :
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] 
                           = ScriptRuntime.postDecrementElem(lhs, rhs, scope);
                        break;
                    case TokenStream.GETTHIS :
                        lhs = stack[stackTop];
                        stack[stackTop] = ScriptRuntime.getThis((Scriptable)lhs);
                        break;
                    case TokenStream.NEWTEMP :
                        lhs = stack[stackTop];
                        slot = (iCode[++pc] & 0xFF);
                        local[slot] = lhs;
                        break;
                    case TokenStream.USETEMP :
                        slot = (iCode[++pc] & 0xFF);
                        stack[++stackTop] = local[slot];
                        break;
                    case TokenStream.CALLSPECIAL :
                        i = (iCode[pc + 1] << 8) | (iCode[pc + 2] & 0xFF);                    
                        name = getString(theData.itsStringTable, iCode, pc + 3);
                        count = (iCode[pc + 5] << 8) | (iCode[pc + 6] & 0xFF);
                        outArgs = new Object[count];
                        for (i = count - 1; i >= 0; i--)
                            outArgs[i] = stack[stackTop--];
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] = ScriptRuntime.callSpecial(
                                            cx, lhs, rhs, outArgs, 
                                            theData.itsThisObj,
                                            scope, name, i);
                        pc += 6;
                        break;
                    case TokenStream.CALL :
                        count = (iCode[pc + 1] << 8) | (iCode[pc + 2] & 0xFF);
                        outArgs = new Object[count];
                        for (i = count - 1; i >= 0; i--)
                            outArgs[i] = stack[stackTop--];
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] = ScriptRuntime.call(cx, lhs, 
                                                             rhs, outArgs);
                        pc += 2;                                                            
                        break;
                    case TokenStream.NEW :
                        count = (iCode[pc + 1] << 8) | (iCode[pc + 2] & 0xFF);
                        outArgs = new Object[count];
                        for (i = count - 1; i >= 0; i--)
                            outArgs[i] = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop] = ScriptRuntime.newObject(cx, 
                                                            lhs, outArgs);
                        pc += 2;                                                            
                        break;
                    case TokenStream.TYPEOF :
                        lhs = stack[stackTop];
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
                        stack[++stackTop] = getNumber(theData.itsNumberTable,
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
                        lhs = stack[stackTop];
                        slot = (iCode[++pc] & 0xFF);
                        vars[slot] = lhs;
                        break;
                    case TokenStream.GETVAR :
                        slot = (iCode[++pc] & 0xFF);
                        stack[++stackTop] = vars[slot];
                        break;
                    case TokenStream.VARINC :
                        slot = (iCode[++pc] & 0xFF);
                        stack[++stackTop] = vars[slot];
                        vars[slot] = ScriptRuntime.postIncrement(vars[slot]);
                        break;
                    case TokenStream.VARDEC :
                        slot = (iCode[++pc] & 0xFF);
                        stack[++stackTop] = vars[slot];
                        vars[slot] = ScriptRuntime.postDecrement(vars[slot]);
                        break;
                    case TokenStream.ZERO :
                        stack[++stackTop] = zero;
                        break;
                    case TokenStream.ONE :
                        stack[++stackTop] = one;
                        break;
                    case TokenStream.NULL :
                        stack[++stackTop] = null;
                        break;
                    case TokenStream.THIS :
                        stack[++stackTop] = theData.itsThisObj;
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
                        throw new JavaScriptException(stack[stackTop--]);
                    case TokenStream.JTHROW :
                        cx.interpreterSecurityDomain = null;
                        lhs = stack[stackTop--];
                        if (lhs instanceof JavaScriptException)
                            throw (JavaScriptException)lhs;
                        else
                            throw (RuntimeException)lhs;
                    case TokenStream.ENTERWITH :
                        lhs = stack[stackTop--];
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
                        lhs = stack[stackTop--];
                        local[slot] = ScriptRuntime.initEnum(lhs, scope);
                        break;
                    case TokenStream.ENUMNEXT :
                        slot = (iCode[++pc] & 0xFF);
                        stack[++stackTop]
                            = ScriptRuntime.nextEnum((Enumeration)local[slot]);
                        break;
                    case TokenStream.GETPROTO :
                        lhs = stack[stackTop];
                        stack[stackTop] = ScriptRuntime.getProto(lhs, scope);
                        break;
                    case TokenStream.GETPARENT :
                        lhs = stack[stackTop];
                        stack[stackTop] = ScriptRuntime.getParent(lhs);
                        break;
                    case TokenStream.GETSCOPEPARENT :
                        lhs = stack[stackTop];
                        stack[stackTop] = ScriptRuntime.getParent(lhs, scope);
                        break;
                    case TokenStream.SETPROTO :
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop]
                                = ScriptRuntime.setProto(lhs, rhs, scope);
                        break;
                    case TokenStream.SETPARENT :
                        rhs = stack[stackTop--];
                        lhs = stack[stackTop];
                        stack[stackTop]
                                = ScriptRuntime.setParent(lhs, rhs, scope);
                        break;
                    case TokenStream.SCOPE :
                        stack[++stackTop] = scope;
                        break;
                    case TokenStream.CLOSURE :
                        i = (iCode[pc + 1] << 8) | (iCode[pc + 2] & 0xFF);                    
                        stack[++stackTop] = new NativeClosure(cx, scope, 
                                                theData.itsNestedFunctions[i]);
                        pc += 2;
                        break;
                    case TokenStream.OBJECT :
                        i = (iCode[pc + 1] << 8) | (iCode[pc + 2] & 0xFF);                    
                        stack[++stackTop] = theData.itsRegExpLiterals[i];
                        pc += 2;
                        break;
                    default :
                        dumpICode(theData);
                        throw new RuntimeException("Unknown icode : "
                                     + (iCode[pc] & 0xff) + " @ pc : " + pc);
                }
                pc++;
            }
            catch (JavaScriptException jsx) {
                stackTop = 0;
                cx.interpreterSecurityDomain = null;
                if (tryStackTop > 0) {
                    pc = catchStack[--tryStackTop];
                    scope = scopeStack[tryStackTop];
                    if (pc == 0) {
                        pc = finallyStack[tryStackTop];
                        if (pc == 0) 
                            throw jsx;
                        stack[0] = jsx;
                    }
                    else
                        stack[0] = ScriptRuntime.unwrapJavaScriptException(jsx);
                }
                else
                    throw jsx;
                // We caught an exception; restore this function's 
                // security domain.
                cx.interpreterSecurityDomain = theData.securityDomain;
            }
            /* debug only, will be going soon
            catch (ArrayIndexOutOfBoundsException oob) {
                System.out.println("OOB @ " + pc + "  " + oob + " in " + theData.itsName + " " + oob.getMessage());
                throw oob;
            }
            */
            catch (RuntimeException jx) {
                cx.interpreterSecurityDomain = null;
                if (tryStackTop > 0) {
                    stackTop = 0;
                    stack[0] = jx;
                    pc = finallyStack[--tryStackTop];
                    scope = scopeStack[tryStackTop];
                    if (pc == 0) throw jx;
                }
                else
                    throw jx;
                // We caught an exception; restore this function's 
                // security domain.
                cx.interpreterSecurityDomain = theData.securityDomain;
            }
        }
        cx.interpreterSecurityDomain = savedSecurityDomain;
        return result;    
    }
    
    private int version;
}

