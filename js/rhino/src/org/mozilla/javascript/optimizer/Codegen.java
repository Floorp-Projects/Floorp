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
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s):
 * Norris Boyd
 * Roger Lawrence
 * Andi Vajda
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
import java.util.*;
import java.io.*;
import java.lang.reflect.*;

/**
 * This class generates code for a given IR tree.
 *
 * @author Norris Boyd
 * @author Roger Lawrence
 */

public class Codegen extends Interpreter {

    public Codegen() {
        itsConstantList = new ConstantList();
    }
    
    public IRFactory createIRFactory(TokenStream ts, 
                                     ClassNameHelper nameHelper, Scriptable scope) 
    {
        return new OptIRFactory(ts, nameHelper, scope);
    }
    
    public Node transform(Node tree, TokenStream ts, Scriptable scope) {
        OptTransformer opt = new OptTransformer(new Hashtable(11));
        return opt.transform(tree, null, ts, scope);
    }
    
    public Object compile(Context cx, Scriptable scope, Node tree, 
                          Object securityDomain,
                          SecuritySupport securitySupport,
                          ClassNameHelper nameHelper)
        throws IOException
    {
        Vector classFiles = new Vector();
        Vector names = new Vector();
        String generatedName = null;
        
        Exception e = null;
        Class result = null;
        DefiningClassLoader classLoader = new DefiningClassLoader();

        try {
            if (cx.getOptimizationLevel() > 0) {
                (new Optimizer()).optimize(tree, cx.getOptimizationLevel());
            }
            generatedName = generateCode(tree, names, classFiles, nameHelper);

            for (int i=0; i < names.size(); i++) {
                String name = (String) names.elementAt(i);
                byte[] classFile = (byte[]) classFiles.elementAt(i);
                if (nameHelper.getGeneratingDirectory() != null) {
                    try {
                        int lastDot = name.lastIndexOf('.');
                        if (lastDot != -1)
                            name = name.substring(lastDot+1);
                        String filename 
                                    = nameHelper.getTargetClassFileName(name);
                        FileOutputStream out = new FileOutputStream(filename);
                        out.write(classFile);
                        out.close();
                    }
                    catch (IOException iox) {
                        throw WrappedException.wrapException(iox);
                    }
                } else {
                    boolean isTopLevel = name.equals(generatedName);
                    ClassOutput classOutput = nameHelper.getClassOutput();
                    if (classOutput != null) {
                        try {
                            OutputStream out =
                                classOutput.getOutputStream(name, isTopLevel);

                            out.write(classFile);
                            out.close();
                        } catch (IOException iox) {
                            throw WrappedException.wrapException(iox);
                        }
                    }
                    try {
                        Class clazz = null;
                        if (securitySupport != null) {
                            clazz = securitySupport.defineClass(name, classFile, 
                                                                securityDomain);
                        }
                        if (clazz == null) {
                            Context.checkSecurityDomainRequired();
                            clazz = classLoader.defineClass(name, classFile);
                            ClassLoader loader = clazz.getClassLoader();
                            clazz = loader.loadClass(name);
                        }
                        if (isTopLevel)
                            result = clazz;
                    } catch (ClassFormatError ex) {
                        throw new RuntimeException(ex.toString());
                    } catch (ClassNotFoundException ex) {
                        throw new RuntimeException(ex.toString());
                    }   
                }
            }
        }
        catch (SecurityException x) {   
            e = x;
        }
        catch (IllegalArgumentException x) {
            e = x;
        }
        if (e != null) 
            throw new RuntimeException("Malformed optimizer package " + e);

        OptClassNameHelper onh = (OptClassNameHelper) nameHelper;
        if (onh.getTargetImplements() != null ||
            onh.getTargetExtends() != null)
        {
            String name = onh.getJavaScriptClassName(null, true);
            ScriptableObject obj = new NativeObject();
            ShallowNodeIterator i = tree.getChildIterator();
            while (i.hasMoreElements()) {
                Node n = i.nextNode();
                if (n.getType() == TokenStream.FUNCTION) 
                    obj.put((String) n.getDatum(), obj, 
                            n.getProp(Node.FUNCTION_PROP));
            }
            try {
                Class superClass = onh.getTargetExtends();
                if (superClass == null)
                    superClass = Object.class;
                JavaAdapter.createAdapterClass(cx, obj, name, 
                                               superClass, 
                                               onh.getTargetImplements(),
                                               generatedName,
                                               onh);
            } catch (ClassNotFoundException exn) {
                // should never happen
                throw new Error(exn.toString());
            }
        }
        if (tree instanceof OptFunctionNode) {
            return ScriptRuntime.createFunctionObject(scope, result, cx, true);
        } else {
            try {
                if (result == null) 
                    return null;
                NativeScript script = (NativeScript) result.newInstance();
                if (scope != null) {
                    script.setPrototype(script.getClassPrototype(scope, 
                                                                 "Script"));
                    script.setParentScope(scope);
                }
                return script;
            }
            catch (InstantiationException x) {
            }
            catch (IllegalAccessException x) {
            }
            throw new RuntimeException("Unable to instantiate compiled class");
        }
    }
    
    void addByteCode(byte theOpcode)
    {
        classFile.add(theOpcode);
    }

    void addByteCode(byte theOpcode, int theOperand)
    {
        classFile.add(theOpcode, theOperand);
    }

    void addByteCode(byte theOpcode, String className)
    {
        classFile.add(theOpcode, className);
    }
    
    void addVirtualInvoke(String className, String methodName, String parameterSignature, String resultSignature)
    {
        classFile.add(ByteCode.INVOKEVIRTUAL,
                        className,
                        methodName,
                        parameterSignature,
                        resultSignature);
    }
    
    void addStaticInvoke(String className, String methodName, String parameterSignature, String resultSignature)
    {
        classFile.add(ByteCode.INVOKESTATIC,
                        className,
                        methodName,
                        parameterSignature,
                        resultSignature);
    }
    
    void addScriptRuntimeInvoke(String methodName, String parameterSignature, String resultSignature)
    {
        classFile.add(ByteCode.INVOKESTATIC,
                        "org/mozilla/javascript/ScriptRuntime",
                        methodName,
                        parameterSignature,
                        resultSignature);
    }
    
    void addOptRuntimeInvoke(String methodName, String parameterSignature, String resultSignature)
    {
        classFile.add(ByteCode.INVOKESTATIC,
                        "org/mozilla/javascript/optimizer/OptRuntime",
                        methodName,
                        parameterSignature,
                        resultSignature);
    }
    
    void addSpecialInvoke(String className, String methodName, String parameterSignature, String resultSignature)
    {
        classFile.add(ByteCode.INVOKESPECIAL,
                        className,
                        methodName,
                        parameterSignature,
                        resultSignature);
    }
    
    void addDoubleConstructor()
    {
        classFile.add(ByteCode.INVOKESPECIAL,
                                "java/lang/Double",
                                "<init>", "(D)", "V");
    }
    
    public int markLabel(int label)
    {
        return classFile.markLabel(label);
    }
    
    public int markLabel(int label, short stackheight)
    {
        return classFile.markLabel(label, stackheight);
    }
    
    public int acquireLabel()
    {
        return classFile.acquireLabel();
    }
    
    public void emitDirectConstructor(OptFunctionNode fnNode)
    {
/*
    we generate ..
        Scriptable directConstruct(<directCallArgs>) {
            Scriptable newInstance = new NativeObject();
            newInstance.setPrototype(getClassPrototype());
            newInstance.setParentScope(getParentScope());
            
            Object val = callDirect(<directCallArgs>);
            // except that the incoming thisArg is discarded
            // and replaced by newInstance
            //
            
            if (val != null && val != Undefined.instance &&
                val instanceof Scriptable)
            {
                return (Scriptable) val;
            }
            return newInstance;    
        }
*/
        short flags = (short)(ClassFileWriter.ACC_PUBLIC 
                            | ClassFileWriter.ACC_FINAL);
        classFile.startMethod("constructDirect",
                        fnNode.getDirectCallParameterSignature()
                            + "Ljava/lang/Object;", 
                        flags);
                        
        int argCount = fnNode.getVariableTable().getParameterCount();
        short firstLocal = (short)((4 + argCount * 3) + 1);
        
        addByteCode(ByteCode.NEW, "org/mozilla/javascript/NativeObject");
        addByteCode(ByteCode.DUP);
        classFile.add(ByteCode.INVOKESPECIAL,
                                "org/mozilla/javascript/NativeObject",
                                "<init>", "()", "V");
        astore(firstLocal);

        aload(firstLocal);
        aload((short)0);
        addVirtualInvoke("org/mozilla/javascript/NativeFunction", 
                            "getClassPrototype",
                            "()", "Lorg/mozilla/javascript/Scriptable;");
        classFile.add(ByteCode.INVOKEINTERFACE,
                        "org/mozilla/javascript/Scriptable",
                        "setPrototype",
                        "(Lorg/mozilla/javascript/Scriptable;)",
                        "V");


        aload(firstLocal);
        aload((short)0);
        addVirtualInvoke("org/mozilla/javascript/NativeFunction", 
                            "getParentScope",
                            "()", "Lorg/mozilla/javascript/Scriptable;");
        classFile.add(ByteCode.INVOKEINTERFACE,
                        "org/mozilla/javascript/Scriptable",
                        "setPrototype",
                        "(Lorg/mozilla/javascript/Scriptable;)",
                        "V");
                       
       
        aload((short)0);
        aload((short)1);
        aload((short)2);
        aload(firstLocal);        
        for (int i = 0; i < argCount; i++) {
            aload((short)(4 + (i * 3)));
            dload((short)(5 + (i * 3)));
        }
        aload((short)(4 + argCount * 3));
        addVirtualInvoke(this.name,
                            "callDirect",
                            fnNode.getDirectCallParameterSignature(),
                            "Ljava/lang/Object;");
        astore((short)(firstLocal + 1));
      
        int exitLabel = acquireLabel();
        aload((short)(firstLocal + 1));
        addByteCode(ByteCode.IFNULL, exitLabel);
        aload((short)(firstLocal + 1));
        pushUndefined();
        addByteCode(ByteCode.IF_ACMPEQ, exitLabel);        
        aload((short)(firstLocal + 1));        
        addByteCode(ByteCode.INSTANCEOF, "org/mozilla/javascript/Scriptable");
        addByteCode(ByteCode.IFEQ, exitLabel);
        aload((short)(firstLocal + 1));        
        addByteCode(ByteCode.CHECKCAST, "org/mozilla/javascript/Scriptable");
        addByteCode(ByteCode.ARETURN);
        markLabel(exitLabel);        
 
        aload(firstLocal);
        addByteCode(ByteCode.ARETURN);

        classFile.stopMethod((short)(firstLocal + 2), null);

    }
    
    public String generateCode(Node tree, Vector names, Vector classFiles,
                               ClassNameHelper nameHelper) 
    {
        this.itsNameHelper = (OptClassNameHelper) nameHelper;
        this.namesVector = names;
        this.classFilesVector = classFiles;
        Context cx = Context.getCurrentContext();
        itsSourceFile = null;
        // default is to generate debug info
        if (!cx.isGeneratingDebugChanged() || cx.isGeneratingDebug())
            itsSourceFile = (String) tree.getProp(Node.SOURCENAME_PROP);
        version = cx.getLanguageVersion();
        optLevel = cx.getOptimizationLevel();
        inFunction = tree.getType() == TokenStream.FUNCTION;
        superClassName = inFunction
                         ? normalFunctionSuperClassName
                         : normalScriptSuperClassName;
        superClassSlashName = superClassName.replace('.', '/');

        Node codegenBase;
        if (inFunction) {
            OptFunctionNode fnNode = (OptFunctionNode) tree;
            inDirectCallFunction = fnNode.isTargetOfDirectCall();
            vars = (OptVariableTable) fnNode.getVariableTable();
            this.name = fnNode.getClassName();
            classFile = new ClassFileWriter(name, superClassName, itsSourceFile);
            Node args = tree.getFirstChild();
            String name = fnNode.getFunctionName();
            generateInit(cx, "<init>", tree, name, args);
            if (fnNode.isTargetOfDirectCall()) {
                classFile.startMethod("call",
                                      "(Lorg/mozilla/javascript/Context;" +
                                      "Lorg/mozilla/javascript/Scriptable;" +
                                      "Lorg/mozilla/javascript/Scriptable;" +
                                      "[Ljava/lang/Object;)Ljava/lang/Object;",
                                      (short)(ClassFileWriter.ACC_PUBLIC
                                                    + ClassFileWriter.ACC_FINAL));
                addByteCode(ByteCode.ALOAD_0);
                addByteCode(ByteCode.ALOAD_1);
                addByteCode(ByteCode.ALOAD_2);
                addByteCode(ByteCode.ALOAD_3);
                for (int i = 0; i < vars.getParameterCount(); i++) {
                    push(i);
                    addByteCode(ByteCode.ALOAD, 4);
                    addByteCode(ByteCode.ARRAYLENGTH);
                    int undefArg = acquireLabel();
                    int beyond = acquireLabel();
                    addByteCode(ByteCode.IF_ICMPGE, undefArg);
                    addByteCode(ByteCode.ALOAD, 4);
                    push(i);
                    addByteCode(ByteCode.AALOAD);
                    push(0.0);
                    addByteCode(ByteCode.GOTO, beyond);
                    markLabel(undefArg);
                    pushUndefined();
                    push(0.0);
                    markLabel(beyond);
                }
                addByteCode(ByteCode.ALOAD, 4);                 
                addVirtualInvoke(this.name,
                                "callDirect",
                                fnNode.getDirectCallParameterSignature(),
                                "Ljava/lang/Object;");
                addByteCode(ByteCode.ARETURN);
                classFile.stopMethod((short)5, null);
                // 1 for this, 1 for js this, 1 for args[]
                
                emitDirectConstructor(fnNode);

                startNewMethod("callDirect",
                               fnNode.getDirectCallParameterSignature() +
                                "Ljava/lang/Object;",
                               1, false, true);
                vars.assignParameterJRegs();
                if (!fnNode.getParameterNumberContext()) {
                    // make sure that all parameters are objects
                    itsForcedObjectParameters = true;
                    for (int i = 0; i < vars.getParameterCount(); i++) {
                        OptLocalVariable lVar = (OptLocalVariable) vars.getVariable(i);
                        aload(lVar.getJRegister());
                        classFile.add(ByteCode.GETSTATIC,
                                      "java/lang/Void",
                                      "TYPE",
                                      "Ljava/lang/Class;");
                        int isObjectLabel = acquireLabel();
                        addByteCode(ByteCode.IF_ACMPNE, isObjectLabel);
                        addByteCode(ByteCode.NEW,"java/lang/Double");
                        addByteCode(ByteCode.DUP);
                        dload((short)(lVar.getJRegister() + 1));
                        addDoubleConstructor();
                        astore(lVar.getJRegister());
                        markLabel(isObjectLabel);                    
                    }
                }
                generatePrologue(cx, tree, true, vars.getParameterCount());
            } else {
                startNewMethod("call",
                               "(Lorg/mozilla/javascript/Context;" +
                                "Lorg/mozilla/javascript/Scriptable;" +
                                "Lorg/mozilla/javascript/Scriptable;" +
                               "[Ljava/lang/Object;)Ljava/lang/Object;",
                               1, false, true);
                generatePrologue(cx, tree, true, -1);
            }
            codegenBase = tree.getLastChild();
        } else {
            // better be a script
            if (tree.getType() != TokenStream.SCRIPT)
                badTree();
            vars = (OptVariableTable) tree.getProp(Node.VARS_PROP);
            boolean isPrimary = itsNameHelper.getTargetExtends() == null &&
                                itsNameHelper.getTargetImplements() == null;
            this.name = itsNameHelper.getJavaScriptClassName(null, isPrimary);
            classFile = new ClassFileWriter(name, superClassName, itsSourceFile);
            classFile.addInterface("org/mozilla/javascript/Script");
            generateScriptCtor(cx, tree);
            generateMain(cx);
            generateInit(cx, "initScript", tree, "", null);
            generateExecute(cx);
            startNewMethod("call",
                           "(Lorg/mozilla/javascript/Context;" +
                            "Lorg/mozilla/javascript/Scriptable;" +
                            "Lorg/mozilla/javascript/Scriptable;" +
                            "[Ljava/lang/Object;)Ljava/lang/Object;",
                           1, false, true);
            generatePrologue(cx, tree, false, -1);
            Object linenum = tree.getProp(Node.END_LINENO_PROP);
            if (linenum != null)
              classFile.addLineNumberEntry(((Integer) linenum).shortValue());
            tree.addChildToBack(new Node(TokenStream.RETURN));
            codegenBase = tree;
        }
        
        generateCodeFromNode(codegenBase, null, -1, -1);

        generateEpilogue();

        finishMethod(cx, debugVars);

        emitConstantDudeInitializers();

        ByteArrayOutputStream out = new ByteArrayOutputStream(512);
        try {
            classFile.write(out);
        }
        catch (IOException ioe) {
            throw new RuntimeException("unexpected IOException");
        }
        byte[] bytes = out.toByteArray();

        namesVector.addElement(name);
        classFilesVector.addElement(bytes);
        
        classFile = null;
        namesVector = null;
        classFilesVector = null;
        
        return name;
    }

    private void generateCodeFromNode(Node node, Node parent, int trueLabel, 
                                      int falseLabel) 
    {
        // System.out.println("gen code for " + node.toString());

        int type = node.getType();
        Node child = node.getFirstChild();
        switch (type) {
              case TokenStream.LOOP:
              case TokenStream.WITH:
              case TokenStream.LABEL:
                visitStatement(node);
                while (child != null) {
                    generateCodeFromNode(child, node, trueLabel, falseLabel);
                    child = child.getNextSibling();
                }
                break;

              case TokenStream.CASE:
              case TokenStream.DEFAULT:
                // XXX shouldn't these be StatementNodes?

              case TokenStream.SCRIPT:
              case TokenStream.BLOCK:
              case TokenStream.VOID:
              case TokenStream.NOP:
                // no-ops.
                visitStatement(node);
                while (child != null) {
                    generateCodeFromNode(child, node, trueLabel, falseLabel);
                    child = child.getNextSibling();
                }
                break;

              case TokenStream.FUNCTION:
                if (inFunction || parent.getType() != TokenStream.SCRIPT) {
                    FunctionNode fn = (FunctionNode) node.getProp(Node.FUNCTION_PROP);
                    byte t = fn.getFunctionType();
                    if (t != FunctionNode.FUNCTION_STATEMENT) {
                        visitFunction(fn, t == FunctionNode.FUNCTION_EXPRESSION_STATEMENT);
                    }
                }
                break;

              case TokenStream.NAME:
                visitName(node);
                break;

              case TokenStream.NEW:
              case TokenStream.CALL:
                visitCall(node, type, child);
                break;

              case TokenStream.NUMBER:
              case TokenStream.STRING:
                visitLiteral(node);
                break;

              case TokenStream.PRIMARY:
                visitPrimary(node);
                break;

              case TokenStream.OBJECT:
                visitObject(node);
                break;

              case TokenStream.TRY:
                visitTryCatchFinally(node, child);
                break;

              case TokenStream.THROW:
                visitThrow(node, child);
                break;

              case TokenStream.RETURN:
                visitReturn(node, child);
                break;

              case TokenStream.SWITCH:
                visitSwitch(node, child);
                break;

              case TokenStream.COMMA:
                generateCodeFromNode(child, node, -1, -1);
                addByteCode(ByteCode.POP);
                generateCodeFromNode(child.getNextSibling(), 
                                            node, trueLabel, falseLabel);
                break;

              case TokenStream.NEWSCOPE:
                addScriptRuntimeInvoke("newScope", "()", 
                                       "Lorg/mozilla/javascript/Scriptable;");
                break;

              case TokenStream.ENTERWITH:
                visitEnterWith(node, child);
                break;

              case TokenStream.LEAVEWITH:
                visitLeaveWith(node, child);
                break;

              case TokenStream.ENUMINIT:
                visitEnumInit(node, child);
                break;

              case TokenStream.ENUMNEXT:
                visitEnumNext(node, child);
                break;

              case TokenStream.ENUMDONE:
                visitEnumDone(node, child);
                break;

              case TokenStream.POP:
                visitStatement(node);
                if (child.getType() == TokenStream.SETVAR) {
                    /* special case this so as to avoid unnecessary
                    load's & pop's */
                    visitSetVar(child, child.getFirstChild(), false);
                }
                else {
                    while (child != null) {
                        generateCodeFromNode(child, node, trueLabel, falseLabel);
                        child = child.getNextSibling();
                    }
                    if (node.getProp(Node.ISNUMBER_PROP) != null)
                        addByteCode(ByteCode.POP2);
                    else
                        addByteCode(ByteCode.POP);
                }
                break;

              case TokenStream.POPV:
                visitStatement(node);
                while (child != null) {
                    generateCodeFromNode(child, node, trueLabel, falseLabel);
                    child = child.getNextSibling();
                }
                astore(scriptResultLocal);
                break;

              case TokenStream.TARGET:
                visitTarget(node);
                break;

              case TokenStream.JSR:
              case TokenStream.GOTO:
              case TokenStream.IFEQ:
              case TokenStream.IFNE:
                visitGOTO(node, type, child);
                break;

              case TokenStream.UNARYOP:
                visitUnary(node, child, trueLabel, falseLabel);
                break;

              case TokenStream.TYPEOF:
                visitTypeof(node, child);
                break;

              case TokenStream.INC:
                visitIncDec(node, true);
                break;

              case TokenStream.DEC:
                visitIncDec(node, false);
                break;

              case TokenStream.OR:
              case TokenStream.AND: {
                    if (trueLabel == -1) {
                        generateCodeFromNode(child, node, trueLabel, falseLabel);
                        addByteCode(ByteCode.DUP);
                        addScriptRuntimeInvoke("toBoolean", "(Ljava/lang/Object;)", "Z");
                        int falseTarget = acquireLabel();
                        if (type == TokenStream.AND)
                            addByteCode(ByteCode.IFEQ, falseTarget);
                        else
                            addByteCode(ByteCode.IFNE, falseTarget);
                        addByteCode(ByteCode.POP);
                        generateCodeFromNode(child.getNextSibling(), node, trueLabel, falseLabel);
                        markLabel(falseTarget);
                    }
                    else {
                        int interLabel = acquireLabel();
                        if (type == TokenStream.AND) {
                            generateCodeFromNode(child, node, interLabel, falseLabel);
                            if (((child.getType() == TokenStream.UNARYOP) && (child.getInt() == TokenStream.NOT))
                                    || (child.getType() == TokenStream.AND)
                                    || (child.getType() == TokenStream.OR)
                                    || (child.getType() == TokenStream.RELOP)
                                    || (child.getType() == TokenStream.EQOP)) {
                            }
                            else {
                                addScriptRuntimeInvoke("toBoolean",
                                                "(Ljava/lang/Object;)", "Z");
                                addByteCode(ByteCode.IFNE, interLabel);
                                addByteCode(ByteCode.GOTO, falseLabel);
                            }
                        }
                        else {
                            generateCodeFromNode(child, node, trueLabel, interLabel);
                            if (((child.getType() == TokenStream.UNARYOP) && (child.getInt() == TokenStream.NOT))
                                    || (child.getType() == TokenStream.AND)
                                    || (child.getType() == TokenStream.OR)
                                    || (child.getType() == TokenStream.RELOP)
                                    || (child.getType() == TokenStream.EQOP)) {
                            }
                            else {
                                addScriptRuntimeInvoke("toBoolean",
                                                "(Ljava/lang/Object;)", "Z");
                                addByteCode(ByteCode.IFNE, trueLabel);
                                addByteCode(ByteCode.GOTO, interLabel);
                            }
                        }
                        markLabel(interLabel);
                        child = child.getNextSibling();
                        generateCodeFromNode(child, node, trueLabel, falseLabel);
                        if (((child.getType() == TokenStream.UNARYOP) && (child.getInt() == TokenStream.NOT))
                                || (child.getType() == TokenStream.AND)
                                || (child.getType() == TokenStream.OR)
                                || (child.getType() == TokenStream.RELOP)
                                || (child.getType() == TokenStream.EQOP)) {
                        }
                        else {
                            addScriptRuntimeInvoke("toBoolean",
                                            "(Ljava/lang/Object;)", "Z");
                            addByteCode(ByteCode.IFNE, trueLabel);
                            addByteCode(ByteCode.GOTO, falseLabel);
                        }
                    }
                }
                break;

              case TokenStream.ADD: {
                    generateCodeFromNode(child, node, trueLabel, falseLabel);
                    generateCodeFromNode(child.getNextSibling(), 
                                                 node, trueLabel, falseLabel);
                    Integer childNumberFlag = 
                                  (Integer)(node.getProp(Node.ISNUMBER_PROP));
                    if (childNumberFlag != null) {
                        if (childNumberFlag.intValue() == Node.BOTH) {
                            addByteCode(ByteCode.DADD);
                        }
                        else {
                            if (childNumberFlag.intValue() == Node.LEFT) {
                                addOptRuntimeInvoke("add",
                                      "(DLjava/lang/Object;)",
                                      "Ljava/lang/Object;");
                            }
                            else {
                                addOptRuntimeInvoke("add",
                                      "(Ljava/lang/Object;D)",
                                      "Ljava/lang/Object;");
                            }
                        }
                    }
                    else {
                        addScriptRuntimeInvoke("add",
                                    "(Ljava/lang/Object;Ljava/lang/Object;)",
                                    "Ljava/lang/Object;");
                    }
                }
                break;

              case TokenStream.MUL:
                visitArithmetic(node, ByteCode.DMUL, child, parent);
                break;

              case TokenStream.SUB:
                visitArithmetic(node, ByteCode.DSUB, child, parent);
                break;

              case TokenStream.DIV:
              case TokenStream.MOD:
                visitArithmetic(node, type == TokenStream.DIV
                                      ? ByteCode.DDIV
                                      : ByteCode.DREM, child, parent);
                break;

              case TokenStream.BITOR:
              case TokenStream.BITXOR:
              case TokenStream.BITAND:
              case TokenStream.LSH:
              case TokenStream.RSH:
              case TokenStream.URSH:
                visitBitOp(node, type, child);
                break;

              case TokenStream.CONVERT: {
                    Object toType = node.getProp(Node.TYPE_PROP);
                    if (toType == ScriptRuntime.NumberClass) {
                        addByteCode(ByteCode.NEW, "java/lang/Double");
                        addByteCode(ByteCode.DUP);
                        generateCodeFromNode(child, node, trueLabel, falseLabel);
                        addScriptRuntimeInvoke("toNumber",
                                              "(Ljava/lang/Object;)", "D");
                        addDoubleConstructor();
                    }
                    else {
                        if (toType == ScriptRuntime.DoubleClass) {
                                                               // cnvt to double
                                                               // (not Double)
                            generateCodeFromNode(child, node, trueLabel, falseLabel);
                            addScriptRuntimeInvoke("toNumber",
                                             "(Ljava/lang/Object;)", "D");
                        }
                        else {
                            if (toType == ScriptRuntime.ObjectClass) {// convert from double
                                if ((child.getType() == TokenStream.NUMBER)
                                     && (child.getProp(Node.ISNUMBER_PROP)
                                                                    != null)) {
                                    Object oldProp
                                           = child.getProp(Node.ISNUMBER_PROP);
                                    child.putProp(Node.ISNUMBER_PROP, null);
                                    generateCodeFromNode(child, node, trueLabel, falseLabel);
                                    child.putProp(Node.ISNUMBER_PROP, oldProp);
                                }
                                else {
                                    addByteCode(ByteCode.NEW, "java/lang/Double");
                                    addByteCode(ByteCode.DUP);
                                    generateCodeFromNode(child, node, trueLabel, falseLabel);
                                    addDoubleConstructor();
                                }
                            }
                            else
                                badTree();
                        }
                    }
                }
                break;

              case TokenStream.RELOP:
                if (trueLabel == -1)    // need a result Object
                    visitRelOp(node, child, parent);
                else
                    visitGOTOingRelOp(node, child, parent, trueLabel, falseLabel);
                break;

              case TokenStream.EQOP:
                visitEqOp(node, child, parent, trueLabel, falseLabel);
                break;

              case TokenStream.GETPROP:
                visitGetProp(node, child);
                break;

              case TokenStream.GETELEM:
                while (child != null) {
                    generateCodeFromNode(child, node, trueLabel, falseLabel);
                    child = child.getNextSibling();
                }
                aload(variableObjectLocal);
                if (node.getProp(Node.ISNUMBER_PROP) != null) {
                    addOptRuntimeInvoke("getElem",
                                      "(Ljava/lang/Object;D" +
                                      "Lorg/mozilla/javascript/Scriptable;)",
                                      "Ljava/lang/Object;");
                }
                else {
                    addScriptRuntimeInvoke("getElem",
                                      "(Ljava/lang/Object;Ljava/lang/Object;" +
                                      "Lorg/mozilla/javascript/Scriptable;)",
                                      "Ljava/lang/Object;");
                }
                break;

              case TokenStream.GETVAR: {
                OptLocalVariable lVar
                        = (OptLocalVariable)(node.getProp(Node.VARIABLE_PROP));
                visitGetVar(lVar,
                            node.getProp(Node.ISNUMBER_PROP) != null,
                            node.getString());
              }
              break;

              case TokenStream.SETVAR:
                visitSetVar(node, child, true);
                break;

              case TokenStream.SETNAME:
                visitSetName(node, child);
                break;

              case TokenStream.SETPROP:
                visitSetProp(node, child);
                break;

              case TokenStream.SETELEM:
                while (child != null) {
                    generateCodeFromNode(child, node, trueLabel, falseLabel);
                    child = child.getNextSibling();
                }
                aload(variableObjectLocal);
                if (node.getProp(Node.ISNUMBER_PROP) != null) {
                    addOptRuntimeInvoke("setElem",
                              "(Ljava/lang/Object;D" +
                              "Ljava/lang/Object;Lorg/mozilla/javascript/Scriptable;)",
                              "Ljava/lang/Object;");
                }
                else {
                    addScriptRuntimeInvoke("setElem",
                              "(Ljava/lang/Object;Ljava/lang/Object;" +
                              "Ljava/lang/Object;Lorg/mozilla/javascript/Scriptable;)",
                              "Ljava/lang/Object;");
                }
                break;

              case TokenStream.DELPROP:
                while (child != null) {
                    generateCodeFromNode(child, node, trueLabel, falseLabel);
                    child = child.getNextSibling();
                }
                addScriptRuntimeInvoke("delete",
                          "(Ljava/lang/Object;Ljava/lang/Object;)",
                          "Ljava/lang/Object;");
                break;

              case TokenStream.BINDNAME:
              case TokenStream.GETBASE:
                visitBind(node, type, child);
                break;

              case TokenStream.GETTHIS:
                generateCodeFromNode(child, node, trueLabel, falseLabel);
                addScriptRuntimeInvoke("getThis",
                          "(Lorg/mozilla/javascript/Scriptable;)",
                          "Lorg/mozilla/javascript/Scriptable;");
                break;

              case TokenStream.PARENT:
                generateCodeFromNode(child, node, trueLabel, falseLabel);
                addScriptRuntimeInvoke("getParent",
                              "(Ljava/lang/Object;)",
                              "Lorg/mozilla/javascript/Scriptable;");
                break;

              case TokenStream.NEWTEMP:
                visitNewTemp(node, child);
                break;

              case TokenStream.USETEMP:
                visitUseTemp(node, child);
                break;

              case TokenStream.NEWLOCAL:
                visitNewLocal(node, child);
                break;

              case TokenStream.USELOCAL:
                visitUseLocal(node, child);
                break;

              default:
                throw new RuntimeException("Unexpected node type " +
					      TokenStream.tokenToName(type));
        }

    }

    private void startNewMethod(String methodName, String methodDesc,
                                int parmCount, boolean isStatic,
                                boolean isFinal)
    {
        locals = new boolean[MAX_LOCALS];
        localsMax = (short) (parmCount+1);  // number of parms + "this"
        firstFreeLocal = 0;
        contextLocal = -1;
        variableObjectLocal = -1;
        scriptResultLocal = -1;
        argsLocal = -1;
        thisObjLocal = -1;
        funObjLocal = -1;
        debug_pcLocal = -1;
        debugStopSubRetLocal = -1;
        itsZeroArgArray = -1;
        itsOneArgArray = -1;
        short flags = ClassFileWriter.ACC_PUBLIC;
        if (isStatic)
            flags |= ClassFileWriter.ACC_STATIC;
        if (isFinal)
            flags |= ClassFileWriter.ACC_FINAL;
        epilogueLabel = -1;
        classFile.startMethod(methodName, methodDesc, (short) flags);
    }

    private void finishMethod(Context cx, VariableTable vars) {
        classFile.stopMethod((short)(localsMax + 1), vars);
        contextLocal = -1;
    }

    private void generateMain(Context cx) {
        startNewMethod("main", "([Ljava/lang/String;)V", 1, true, true);

        push(this.name);        // load the name of this class
        addByteCode(ByteCode.ALOAD_0); // load 'args'
        addScriptRuntimeInvoke("main",
                              "(Ljava/lang/String;[Ljava/lang/String;)",
                              "V");
        addByteCode(ByteCode.RETURN);
        finishMethod(cx, null);
    }


    private void generateExecute(Context cx) {
        String signature = "(Lorg/mozilla/javascript/Context;" +
                            "Lorg/mozilla/javascript/Scriptable;)" +
                           "Ljava/lang/Object;";
        startNewMethod("exec", signature, 2, false, true);
        String slashName = this.name.replace('.', '/');

        if (!trivialInit) {
            // to begin a script, call the initScript method
            addByteCode(ByteCode.ALOAD_0); // load 'this'
            addByteCode(ByteCode.ALOAD_2); // load 'scope'
            addByteCode(ByteCode.ALOAD_1); // load 'cx'
            addVirtualInvoke(slashName,
                          "initScript",
                          "(Lorg/mozilla/javascript/Scriptable;" +
                          "Lorg/mozilla/javascript/Context;)",
                          "V");
        }

        addByteCode(ByteCode.ALOAD_0); // load 'this'
        addByteCode(ByteCode.ALOAD_1); // load 'cx'
        addByteCode(ByteCode.ALOAD_2); // load 'scope'
        addByteCode(ByteCode.DUP);
        addByteCode(ByteCode.ACONST_NULL);
        addVirtualInvoke(slashName,
                      "call",
                      "(Lorg/mozilla/javascript/Context;" +
                       "Lorg/mozilla/javascript/Scriptable;" +
                       "Lorg/mozilla/javascript/Scriptable;" +
                       "[Ljava/lang/Object;)",
                      "Ljava/lang/Object;");

        addByteCode(ByteCode.ARETURN);
        finishMethod(cx, null);
    }

    private void generateScriptCtor(Context cx, Node tree) {
        startNewMethod("<init>", "()V", 1, false, false);
        addByteCode(ByteCode.ALOAD_0);
        addSpecialInvoke(superClassSlashName,
                      "<init>", "()", "V");
        addByteCode(ByteCode.RETURN);
        finishMethod(cx, null);
    }
    
    /**
     * Indicate that the init is non-trivial.
     *
     * For trivial inits we can omit creating the init method
     * altogether. (Only applies to scripts, since function
     * inits are constructors, which are required.) For trivial
     * inits we also omit the call to initScript from exec().
     */
    private void setNonTrivialInit(String methodName) {
        if (!trivialInit)
            return;     // already set
        trivialInit = false;
        startNewMethod(methodName, "(Lorg/mozilla/javascript/Scriptable;"
                                    + "Lorg/mozilla/javascript/Context;)V",
                       1, false, false);
        reserveWordLocal(0);  // reserve 0 for 'this'
        variableObjectLocal = reserveWordLocal(1);  // reserve 1 for 'scope'
        contextLocal = reserveWordLocal(2);  // reserve 2 for 'context'
    }

    private void generateInit(Context cx, String methodName,
                              Node tree, String name, Node args)
    {
        trivialInit = true;
        boolean inCtor = false;
        VariableTable vars;
        if (tree instanceof OptFunctionNode)
            vars = ((OptFunctionNode)tree).getVariableTable();
        else
            vars = (VariableTable) tree.getProp(Node.VARS_PROP);

        if (methodName.equals("<init>")) {
            inCtor = true;
            setNonTrivialInit(methodName);
            addByteCode(ByteCode.ALOAD_0);
            addSpecialInvoke(superClassSlashName, "<init>", "()", "V");
            
            addByteCode(ByteCode.ALOAD_0);
            addByteCode(ByteCode.ALOAD_1);
            classFile.add(ByteCode.PUTFIELD,
                          "org/mozilla/javascript/ScriptableObject",
                          "parent", "Lorg/mozilla/javascript/Scriptable;");
        }

        /*
         * Generate code to initialize functionName field with the name 
         * of the function and argNames string array with the names 
         * of the parameters and the vars. Initialize argCount
         * to the number of formal parameters.
         */
        
        if (name.length() != 0) {
            setNonTrivialInit(methodName);
               addByteCode(ByteCode.ALOAD_0);
            classFile.addLoadConstant(name);
            classFile.add(ByteCode.PUTFIELD,
                          "org/mozilla/javascript/NativeFunction",
                          "functionName", "Ljava/lang/String;");
        }

        if (vars != null) {
            int N = vars.size();
            if (N != 0) {
                setNonTrivialInit(methodName);
                push(N);
                addByteCode(ByteCode.ANEWARRAY, "java/lang/String");
                for (int i = 0; i != N; i++) {
                    addByteCode(ByteCode.DUP);
                    push(i);
                    push(vars.getName(i));
                    addByteCode(ByteCode.AASTORE);
                }
                addByteCode(ByteCode.ALOAD_0);
                addByteCode(ByteCode.SWAP);
                classFile.add(ByteCode.PUTFIELD,
                              "org/mozilla/javascript/NativeFunction",
                              "argNames", "[Ljava/lang/String;");
            }
        }

        int parmCount = vars == null ? 0 : vars.getParameterCount();
        if (parmCount != 0) {
            setNonTrivialInit(methodName);
            addByteCode(ByteCode.ALOAD_0);
            push(parmCount);
            classFile.add(ByteCode.PUTFIELD,
                    "org/mozilla/javascript/NativeFunction",
                    "argCount", "S");
        }

        // Initialize NativeFunction.version with Context's version.
        if (cx.getLanguageVersion() != 0) {
            setNonTrivialInit(methodName);
            addByteCode(ByteCode.ALOAD_0);
            push(cx.getLanguageVersion());
            classFile.add(ByteCode.PUTFIELD,
                    "org/mozilla/javascript/NativeFunction",
                    "version", "S");
        }

        // add the String representing the source.
        String source = (String) tree.getProp(Node.SOURCE_PROP);
        if ((source != null) 
                    && cx.isGeneratingSource() 
                        && (source.length() < 65536)) {
            setNonTrivialInit(methodName);
            addByteCode(ByteCode.ALOAD_0);
            push(source);
            classFile.add(ByteCode.PUTFIELD,
                    "org/mozilla/javascript/NativeFunction",
                    "source", "Ljava/lang/String;");
        }

        // precompile all regexp literals
        Vector regexps = (Vector) tree.getProp(Node.REGEXP_PROP);
        if (regexps != null) {
            setNonTrivialInit(methodName);
            generateRegExpLiterals(regexps, inCtor);
        }

        Vector fns = (Vector) tree.getProp(Node.FUNCTION_PROP);
        if (fns != null) {
            setNonTrivialInit(methodName);
            generateFunctionInits(fns);
        }

        if (tree instanceof OptFunctionNode) {

            OptFunctionNode fnNode = (OptFunctionNode)tree;
            Vector directCallTargets
                        = ((OptFunctionNode)tree).getDirectCallTargets();
            if (directCallTargets != null) {
                setNonTrivialInit(methodName);
                classFile.addField("EmptyArray",
                              "[Ljava/lang/Object;",
                              (short)ClassFileWriter.ACC_PRIVATE);
                addByteCode(ByteCode.ALOAD_0);
                push(0);
                addByteCode(ByteCode.ANEWARRAY, "java/lang/Object");
                classFile.add(ByteCode.PUTFIELD,
                              classFile.fullyQualifiedForm(this.name),
                              "EmptyArray",
                              "[Ljava/lang/Object;");
            }
            if (fnNode.isTargetOfDirectCall()) {
                setNonTrivialInit(methodName);
                String className
                     = classFile.fullyQualifiedForm(fnNode.getClassName());
                String fieldName = className.replace('/', '_');
                classFile.addField(fieldName,
                                       "L" + className + ";",
                   (short)(ClassFileWriter.ACC_PUBLIC
                            + ClassFileWriter.ACC_STATIC));
                addByteCode(ByteCode.ALOAD_0);
                classFile.add(ByteCode.PUTSTATIC,
                            className,
                            fieldName,
                            "L" + className + ";");
            }
        }

        if (!trivialInit) {
            addByteCode(ByteCode.RETURN);
            finishMethod(cx, null);
        }
    }
	
    private void generateRegExpLiterals(Vector regexps, boolean inCtor) {
        for (int i=0; i < regexps.size(); i++) {
            Node regexp = (Node) regexps.elementAt(i);
            StringBuffer sb = new StringBuffer("_re");
            sb.append(i);
            String fieldName = sb.toString();
            short flags = ClassFileWriter.ACC_PRIVATE;
            if (inCtor) 
                flags |= ClassFileWriter.ACC_FINAL;
            classFile.addField(fieldName, 
                               "Lorg/mozilla/javascript/regexp/NativeRegExp;",
                               flags);
            addByteCode(ByteCode.ALOAD_0);    // load 'this'

            addByteCode(ByteCode.NEW, "org/mozilla/javascript/regexp/NativeRegExp");
            addByteCode(ByteCode.DUP);

            aload(contextLocal);    // load 'context'
            aload(variableObjectLocal);    // load 'scope'
            Node left = regexp.getFirstChild();
            push(left.getString());
            Node right = regexp.getLastChild();
            if (left == right) {
                addByteCode(ByteCode.ACONST_NULL);
            } else {
                push(right.getString());
            }
            push(0);

            addSpecialInvoke("org/mozilla/javascript/regexp/NativeRegExp",
                                "<init>",
                                "(Lorg/mozilla/javascript/Context;" +
                                  "Lorg/mozilla/javascript/Scriptable;" +
                                  "Ljava/lang/String;Ljava/lang/String;Z)",
                                  "V");

            regexp.putProp(Node.REGEXP_PROP, fieldName);
            classFile.add(ByteCode.PUTFIELD,
                            classFile.fullyQualifiedForm(this.name),
                            fieldName, "Lorg/mozilla/javascript/regexp/NativeRegExp;");
        }
    }

    private void generateFunctionInits(Vector fns) {
        // make an array to put them in, so they're available
        // for decompilation.
        push(fns.size());
        addByteCode(ByteCode.ANEWARRAY, "org/mozilla/javascript/NativeFunction");

        /* note that nested function decompilation currently
         * depends on the elements of the nestedFunctions array
         * being defined in source order.  (per function/script,
         * starting from 0.)  Change Parser and NativeFunction if
         * changing ordering.
         */

        for (short i = 0; i < fns.size(); i++) {
            addByteCode(ByteCode.DUP); // make another reference to the array
            push(i);            // to set up for aastore at end of loop

            Node def = (Node) fns.elementAt(i);
            Codegen codegen = new Codegen();
            String fnClassName = codegen.generateCode(def, namesVector, 
                                                      classFilesVector, 
                                                      itsNameHelper);
            
            addByteCode(ByteCode.NEW, fnClassName);
            addByteCode(ByteCode.DUP);
            if (inFunction) {
                addByteCode(ByteCode.ALOAD_0); // load "this" argument
            } else {
                aload(variableObjectLocal);
            }
            aload(contextLocal);           // load 'cx'
            addSpecialInvoke(fnClassName, "<init>", 
                             "(Lorg/mozilla/javascript/Scriptable;" +
                              "Lorg/mozilla/javascript/Context;)",
                             "V");
                
            // 'fn' still on stack
            // load 'scope'
            if (inFunction) {
                addByteCode(ByteCode.ALOAD_0); // load "this" argument
            } else {
                aload(variableObjectLocal);
            }
            // load 'fnName'
            String str = def.getString();
            if (str != null) {
                push(str);
            } else {
                addByteCode(ByteCode.ACONST_NULL);
            }
            // load 'cx'
            aload(contextLocal);  
            // load boolean indicating whether fn name should be set in scope
            boolean setFnName = str != null && str.length() > 0 && 
                                ((FunctionNode) def).getFunctionType() ==
                                    FunctionNode.FUNCTION_STATEMENT;
            addByteCode(setFnName ? ByteCode.ICONST_1 : ByteCode.ICONST_0);
            
            addScriptRuntimeInvoke("initFunction",
                                   "(Lorg/mozilla/javascript/NativeFunction;" +
                                    "Lorg/mozilla/javascript/Scriptable;" +
                                    "Ljava/lang/String;" +
                                    "Lorg/mozilla/javascript/Context;Z)",
                                   "Lorg/mozilla/javascript/NativeFunction;");
            def.putProp(Node.FUNCTION_PROP, new Short(i));
            addByteCode(ByteCode.AASTORE);    // store NativeFunction
                                              // instance to array
        }

        // add the array as the nestedFunctions field; array should
        // still be on the stack.

        addByteCode(ByteCode.ALOAD_0);  // load 'this'
        addByteCode(ByteCode.SWAP);     // swap with original array reference

        classFile.add(ByteCode.PUTFIELD, "org/mozilla/javascript/NativeFunction",
                "nestedFunctions", "[Lorg/mozilla/javascript/NativeFunction;");
    }


    /**
     * Generate the prologue for a function or script.
     *
     * @param cx the context
     * @param tree the tree to generate code for
     * @param inFunction true if generating the prologue for a function
     *        (as opposed to a script)
     * @param directParameterCount number of parameters for direct call,
     *        or -1 if not direct call
     */
    private void generatePrologue(Context cx, Node tree, boolean inFunction,
                                  int directParameterCount)
    {
        funObjLocal = reserveWordLocal(0);
        contextLocal = reserveWordLocal(1);
        variableObjectLocal = reserveWordLocal(2);
        thisObjLocal = reserveWordLocal(3);
        
        if (inFunction && !cx.hasCompileFunctionsWithDynamicScope() &&
            directParameterCount == -1)
        {
            // Unless we're either using dynamic scope or we're in a
            // direct call, use the enclosing scope of the function as our 
            // variable object.
            aload(funObjLocal);
            classFile.add(ByteCode.INVOKEINTERFACE,
                          "org/mozilla/javascript/Scriptable",
                          "getParentScope",
                          "()", 
                          "Lorg/mozilla/javascript/Scriptable;");
            astore(variableObjectLocal);
        }

        if (directParameterCount > 0) {
            for (int i = 0; i < (3 * directParameterCount); i++)
                reserveWordLocal(i + 4);               // reserve 'args'
        }
        // reserve 'args[]'
        argsLocal = reserveWordLocal(directParameterCount <= 0
                                     ? 4 : (3 * directParameterCount) + 4);

        // These locals are to be pre-allocated since they need function scope.
        // They are primarily used by the exception handling mechanism
        Integer localCount = (Integer)(tree.getProp(Node.LOCALCOUNT_PROP));
        if (localCount != null) {
            itsLocalAllocationBase = (short)(argsLocal + 1);
            for (int i = 0; i < localCount.intValue(); i++) {
                reserveWordLocal(itsLocalAllocationBase + i);
            }
        }

        hasVarsInRegs = inFunction && 
                        !((OptFunctionNode)tree).requiresActivation();
        if (hasVarsInRegs) {
            // No need to create activation. Pad arguments if need be.
            int parmCount = vars.getParameterCount();
            if (inFunction && parmCount > 0 && directParameterCount < 0) {
                // Set up args array
                // check length of arguments, pad if need be
                aload(argsLocal);
                addByteCode(ByteCode.ARRAYLENGTH);
                push(parmCount);
                int label = acquireLabel();
                addByteCode(ByteCode.IF_ICMPGE, label);
                aload(argsLocal);
                push(parmCount);
                addScriptRuntimeInvoke("padArguments",
                    	      "([Ljava/lang/Object;I)",
                    	      "[Ljava/lang/Object;");
                astore(argsLocal);
                markLabel(label);
            }
            
            // REMIND - only need to initialize the vars that don't get a value
            // before the next call and are used in the function
            short firstUndefVar = -1;
            for (int i = 0; i < vars.size(); i++) {
                OptLocalVariable lVar = (OptLocalVariable) vars.getVariable(i);
                if (lVar.isNumber()) {
                    lVar.assignJRegister(getNewWordPairLocal());
                    push(0.0);
                    dstore(lVar.getJRegister());
                } else if (lVar.isParameter()) {
                    if (directParameterCount < 0) {
                        lVar.assignJRegister(getNewWordLocal());
                        aload(argsLocal);
                        push(i);
                        addByteCode(ByteCode.AALOAD);
                        astore(lVar.getJRegister());
                    }
                } else {
                    lVar.assignJRegister(getNewWordLocal());
                    if (firstUndefVar == -1) {
                        pushUndefined();
                        firstUndefVar = lVar.getJRegister();
                    } else {
                        aload(firstUndefVar);
                    }
                    astore(lVar.getJRegister());
                }
                lVar.setStartPC(classFile.getCurrentCodeOffset());
            }
            
            // Indicate that we should generate debug information for 
            // the variable table. (If we're generating debug info at
            // all.)
            debugVars = vars;
            
            // Skip creating activation object.
            return;
        }

        if (directParameterCount > 0) {
            // We're going to create an activation object, so we
            // need to get an args array with all the arguments in it.
            
            aload(argsLocal);
            push(directParameterCount);
            addOptRuntimeInvoke("padStart",
                                   "([Ljava/lang/Object;I)",
                                   "[Ljava/lang/Object;");
            astore(argsLocal);
            for (int i=0; i < directParameterCount; i++) {
                aload(argsLocal);
                push(i);
                // "3" is 1 for Object parm and 2 for double parm, and
                // "4" is to account for the context, etc. parms
                aload((short) (3*i+4));                 
                addByteCode(ByteCode.AASTORE);
            }
        }
        
        String debugVariableName;
        if (inFunction) {
            aload(contextLocal);
            aload(variableObjectLocal);
            aload(funObjLocal);
            aload(thisObjLocal);
            aload(argsLocal);
            addScriptRuntimeInvoke("initVarObj",
                          "(Lorg/mozilla/javascript/Context;" +
                           "Lorg/mozilla/javascript/Scriptable;" +
                           "Lorg/mozilla/javascript/NativeFunction;" +
                           "Lorg/mozilla/javascript/Scriptable;" +
                           "[Ljava/lang/Object;)",
                          "Lorg/mozilla/javascript/Scriptable;");
            debugVariableName = "activation";
        } else {
            aload(contextLocal);
            aload(variableObjectLocal);
            aload(funObjLocal);
            aload(thisObjLocal);
            push(0);
            addScriptRuntimeInvoke("initScript",
                          "(Lorg/mozilla/javascript/Context;" +
                           "Lorg/mozilla/javascript/Scriptable;" +
                           "Lorg/mozilla/javascript/NativeFunction;" +
                           "Lorg/mozilla/javascript/Scriptable;Z)",
                          "Lorg/mozilla/javascript/Scriptable;");
            debugVariableName = "global";
        }
        astore(variableObjectLocal);
        
        Vector fns = (Vector) tree.getProp(Node.FUNCTION_PROP);
        if (inFunction && fns != null) {
            for (int i=0; i < fns.size(); i++) {
                FunctionNode fn = (FunctionNode) fns.elementAt(i);
                if (fn.getFunctionType() == FunctionNode.FUNCTION_STATEMENT) {
                    visitFunction(fn, true);
                }
            }
        }
        
        
        // default is to generate debug info
        if (!cx.isGeneratingDebugChanged() || cx.isGeneratingDebug()) {
            debugVars = new OptVariableTable();
            debugVars.addLocal(debugVariableName);
            OptLocalVariable lv = (OptLocalVariable) debugVars.getVariable(debugVariableName);
            lv.assignJRegister(variableObjectLocal);
            lv.setStartPC(classFile.getCurrentCodeOffset());
        }

        if (!inFunction) { 
            // OPT: use dataflow to prove that this assignment is dead
            scriptResultLocal = getNewWordLocal();
            pushUndefined();
            astore(scriptResultLocal);
        } 
        
        if (inFunction && ((OptFunctionNode)tree).containsCalls(-1)) {
            if (((OptFunctionNode)tree).containsCalls(0)) {
                itsZeroArgArray = getNewWordLocal();
                classFile.add(ByteCode.GETSTATIC, 
                        "org/mozilla/javascript/ScriptRuntime",
                        "emptyArgs", "[Ljava/lang/Object;");
                astore(itsZeroArgArray);
            }
            if (((OptFunctionNode)tree).containsCalls(1)) {
                itsOneArgArray = getNewWordLocal();
                push(1);
                addByteCode(ByteCode.ANEWARRAY, "java/lang/Object");
                astore(itsOneArgArray);
            }
        }
    }
    
    private void generateEpilogue() {
        if (epilogueLabel != -1) {
            classFile.markLabel(epilogueLabel);
        }
        if (!hasVarsInRegs || !inFunction) {
            // restore caller's activation
            aload(contextLocal);
            addScriptRuntimeInvoke("popActivation",
                                   "(Lorg/mozilla/javascript/Context;)",
                                   "V");
        }
        addByteCode(ByteCode.ARETURN);
    }

    private void visitFunction(Node fn, boolean setName) {
        aload(variableObjectLocal);
        Short index = (Short) fn.getProp(Node.FUNCTION_PROP);
        aload(funObjLocal);
        classFile.add(ByteCode.GETFIELD, "org/mozilla/javascript/NativeFunction",
                "nestedFunctions", "[Lorg/mozilla/javascript/NativeFunction;");
        push(index.shortValue());
        addByteCode(ByteCode.AALOAD);
        addVirtualInvoke("java/lang/Object", "getClass", "()", "Ljava/lang/Class;");
        aload(contextLocal);
        addByteCode(ByteCode.BIPUSH, setName ? (byte)1 : (byte)0);
        
        addScriptRuntimeInvoke("createFunctionObject", 
                               "(Lorg/mozilla/javascript/Scriptable;"+
                                "Ljava/lang/Class;" +
                                "Lorg/mozilla/javascript/Context;Z)", 
                               "Lorg/mozilla/javascript/NativeFunction;");

    }

    private void visitTarget(Node node) {
        Object lblObect = node.getProp(Node.LABEL_PROP);
        if (lblObect == null) {
            int label = markLabel(acquireLabel());
            node.putProp(Node.LABEL_PROP, new Integer(label));
        }
        else {
            int label = ((Integer)lblObect).intValue();
            markLabel(label);
        }

    }

    private void visitGOTO(Node node, int type, Node child) {
        Node target = (Node)(node.getProp(Node.TARGET_PROP));
        Object lblObect = target.getProp(Node.LABEL_PROP);
        int targetLabel;
        if (lblObect == null) {
            targetLabel = acquireLabel();
            target.putProp(Node.LABEL_PROP, new Integer(targetLabel));
        }
        else
            targetLabel = ((Integer)lblObect).intValue();
        int fallThruLabel = acquireLabel();

        if ((type == TokenStream.IFEQ) || (type == TokenStream.IFNE)) {
            if (child == null) {
                // can have a null child from visitSwitch which
                // has already generated the code for the child
                // and just needs the GOTO code emitted
                addScriptRuntimeInvoke("toBoolean", "(Ljava/lang/Object;)", "Z");
                if (type == TokenStream.IFEQ)
                    addByteCode(ByteCode.IFNE, targetLabel);
                else
                    addByteCode(ByteCode.IFEQ, targetLabel);
            }
            else {
                if (type == TokenStream.IFEQ)
                    generateCodeFromNode(child, node, targetLabel, fallThruLabel);
                else
                    generateCodeFromNode(child, node, fallThruLabel, targetLabel);
                if (((child.getType() == TokenStream.UNARYOP) && (child.getInt() == TokenStream.NOT))
                        || (child.getType() == TokenStream.AND)
                        || (child.getType() == TokenStream.OR)
                        || (child.getType() == TokenStream.RELOP)
                        || (child.getType() == TokenStream.EQOP)) {
                }
                else {
                    addScriptRuntimeInvoke("toBoolean",
                                    "(Ljava/lang/Object;)", "Z");
                    if (type == TokenStream.IFEQ)
                        addByteCode(ByteCode.IFNE, targetLabel);
                    else
                        addByteCode(ByteCode.IFEQ, targetLabel);
                }
            }
        }
        else {
            while (child != null) {
                generateCodeFromNode(child, node, -1, -1);
                child = child.getNextSibling();
            }
            if (type == TokenStream.JSR)
                addByteCode(ByteCode.JSR, targetLabel);
            else
                addByteCode(ByteCode.GOTO, targetLabel);
        }
        markLabel(fallThruLabel);
    }

    private void visitEnumInit(Node node, Node child) {
        while (child != null) {
            generateCodeFromNode(child, node, -1, -1);
            child = child.getNextSibling();
        }
        aload(variableObjectLocal);
        addScriptRuntimeInvoke("initEnum",
            "(Ljava/lang/Object;Lorg/mozilla/javascript/Scriptable;)",
             "Ljava/util/Enumeration;");
        short x = getNewWordLocal();
        astore(x);
        node.putProp(Node.LOCAL_PROP, new Integer(x));
    }

    private void visitEnumNext(Node node, Node child) {
        while (child != null) {
            generateCodeFromNode(child, node, -1, -1);
            child = child.getNextSibling();
        }
        Node init = (Node) node.getProp(Node.ENUM_PROP);
        Integer local = (Integer) init.getProp(Node.LOCAL_PROP);
        aload(local.shortValue());
        addScriptRuntimeInvoke("nextEnum", "(Ljava/util/Enumeration;)", "Ljava/lang/Object;");
    }

    private void visitEnumDone(Node node, Node child) {
        while (child != null) {
            generateCodeFromNode(child, node, -1, -1);
            child = child.getNextSibling();
        }
        Node init = (Node) node.getProp(Node.ENUM_PROP);
        Integer local = (Integer) init.getProp(Node.LOCAL_PROP);
        releaseWordLocal(local.shortValue());
    }

    private void visitEnterWith(Node node, Node child) {
        while (child != null) {
            generateCodeFromNode(child, node, -1, -1);
            child = child.getNextSibling();
        }
        aload(variableObjectLocal);
        addScriptRuntimeInvoke("enterWith",
            "(Ljava/lang/Object;" +
             "Lorg/mozilla/javascript/Scriptable;)",
            "Lorg/mozilla/javascript/Scriptable;");
        astore(variableObjectLocal);
    }

    private void visitLeaveWith(Node node, Node child) {
        aload(variableObjectLocal);
        addScriptRuntimeInvoke("leaveWith",
            "(Lorg/mozilla/javascript/Scriptable;)",
            "Lorg/mozilla/javascript/Scriptable;");
        astore(variableObjectLocal);
    }

    private void resetTargets(Node node)
    {
        if (node.getType() == TokenStream.TARGET) {
            node.putProp(Node.LABEL_PROP, null);
        }
        Node child = node.getFirstChild();
        while (child != null) {
            resetTargets(child);
            child = child.getNextSibling();
        }
    }

    private void visitCall(Node node, int type, Node child) {
        /*
         * Generate code for call.
         */
                  
        Node chelsea = child;      // remember the first child for later
        OptFunctionNode target = (OptFunctionNode)node.getProp(Node.DIRECTCALL_PROP);
        if (target != null) {
            generateCodeFromNode(child, node, -1, -1);
            int regularCall = acquireLabel();

            String className = classFile.fullyQualifiedForm(target.getClassName());
            String fieldName = className.replace('/', '_');

            classFile.add(ByteCode.GETSTATIC,
                                classFile.fullyQualifiedForm(className),
                                fieldName,
                                "L" + className + ";");
            short stackHeight = classFile.getStackTop();

            addByteCode(ByteCode.DUP2);
            addByteCode(ByteCode.IF_ACMPNE, regularCall);
            addByteCode(ByteCode.SWAP);
            addByteCode(ByteCode.POP);

            addByteCode(ByteCode.DUP);
            classFile.add(ByteCode.INVOKEINTERFACE,
                                "org/mozilla/javascript/Scriptable",
                                "getParentScope",
                                "()", "Lorg/mozilla/javascript/Scriptable;");
            aload(contextLocal);
            addByteCode(ByteCode.SWAP);

            if (type == TokenStream.NEW)
                addByteCode(ByteCode.ACONST_NULL);
            else {
                child = child.getNextSibling();
                generateCodeFromNode(child, node, -1, -1);
            }
/*
    Remember that directCall parameters are paired in 1 aReg and 1 dReg
    If the argument is an incoming arg, just pass the orginal pair thru.
    Else, if the argument is known to be typed 'Number', pass Void.TYPE
    in the aReg and the number is the dReg
    Else pass the JS object in the aReg and 0.0 in the dReg.
*/
            child = child.getNextSibling();
            while (child != null) {
                boolean handled = false;
                if ((child.getType() == TokenStream.GETVAR)
                        && inDirectCallFunction) {
                    OptLocalVariable lVar
                        = (OptLocalVariable)(child.getProp(Node.VARIABLE_PROP));
                    if (lVar.isParameter()) {
                        handled = true;
                        aload(lVar.getJRegister());
                        dload((short)(lVar.getJRegister() + 1));
                    }
                }
                if (!handled) {
                    Integer childNumberFlag
                                = (Integer)(child.getProp(Node.ISNUMBER_PROP));
                    if ((childNumberFlag != null)
                            && (childNumberFlag.intValue() == Node.BOTH)) {
                        classFile.add(ByteCode.GETSTATIC,
                                "java/lang/Void",
                                "TYPE",
                                "Ljava/lang/Class;");
                        generateCodeFromNode(child, node, -1, -1);
                    }
                    else {
                        generateCodeFromNode(child, node, -1, -1);
                        push(0.0);
                    }
                }
                resetTargets(child);
                child = child.getNextSibling();
            }

            addByteCode(ByteCode.ALOAD_0);
            classFile.add(ByteCode.GETFIELD,
                                classFile.fullyQualifiedForm(this.name),
                                "EmptyArray",
                                "[Ljava/lang/Object;");

            if (type == TokenStream.NEW)
                addVirtualInvoke(target.getClassName(),
                                "constructDirect",
                                target.getDirectCallParameterSignature(),
                                "Ljava/lang/Object;");
            else
                addVirtualInvoke(target.getClassName(),
                                "callDirect",
                                target.getDirectCallParameterSignature(),
                                "Ljava/lang/Object;");

            int beyond = acquireLabel();
            addByteCode(ByteCode.GOTO, beyond);
            markLabel(regularCall, stackHeight);
            addByteCode(ByteCode.POP);

            visitRegularCall(node, type, chelsea, true);
            markLabel(beyond);
        }
        else {
            visitRegularCall(node, type, chelsea, false);
        }
   }


    private String getSimpleCallName(Node callNode)
    /*
        Find call trees that look like this :
        (they arise from simple function invocations)
    
            CALL                     <-- this is the callNode node
                GETPROP
                    NEWTEMP [USES: 1]
                        GETBASE 'name'
                    STRING 'name'
                GETTHIS
                    USETEMP [TEMP: NEWTEMP [USES: 1]]
                <-- arguments would be here
    
        and return the name found.
    
    */
    {
        Node callBase = callNode.getFirstChild();
        if (callBase.getType() == TokenStream.GETPROP) {
            Node callBaseChild = callBase.getFirstChild();
            if (callBaseChild.getType() == TokenStream.NEWTEMP) {
                Node callBaseID = callBaseChild.getNextSibling();
                Node tempChild = callBaseChild.getFirstChild();
                if (tempChild.getType() == TokenStream.GETBASE) {
                    String functionName = tempChild.getString();
                    if ((callBaseID != null) &&
                            (callBaseID.getType() == TokenStream.STRING)) {
                        if (functionName.equals(callBaseID.getString())) {
                            Node thisChild = callBase.getNextSibling();
                            if (thisChild.getType() == TokenStream.GETTHIS) {
                                Node useChild = thisChild.getFirstChild();
                                if (useChild.getType() == TokenStream.USETEMP) {
                                    Node temp = (Node)useChild.getProp(Node.TEMP_PROP);
                                    if (temp == callBaseChild) {
                                        return functionName;
                                    }
                                }                            
                            }
                        }
                    }
                }
            }
        }
        return null;
    }
    
    private void constructArgArray(int argCount)
    {
        if (argCount == 0) {
            if (itsZeroArgArray >= 0)
                aload(itsZeroArgArray);
            else {
                push(0);
                addByteCode(ByteCode.ANEWARRAY, "java/lang/Object");
            }                    
        }
        else {
            if (argCount == 1) {
                if (itsOneArgArray >= 0)
                    aload(itsOneArgArray);
                else {
                    push(1);
                    addByteCode(ByteCode.ANEWARRAY, "java/lang/Object");
                }                    
            }
            else {
                push(argCount);
                addByteCode(ByteCode.ANEWARRAY, "java/lang/Object");
            }
        }
    }

    private void visitRegularCall(Node node, int type,
                                  Node child, boolean firstArgDone)
    {
        /*
         * Generate code for call.
         *
         * push <arity>
         * anewarray Ljava/lang/Object;
         * // "initCount" instances of code from here...
         * dup
         * push <i>
         * <gen code for child>
         * aastore
         * //...to here
         * invokestatic call
         */

        OptFunctionNode target = (OptFunctionNode)node.getProp(Node.DIRECTCALL_PROP);
        Node chelsea = child;
        int childCount = 0;
        int argSkipCount = (type == TokenStream.NEW) ? 1 : 2;
        while (child != null) {
            childCount++;
            child = child.getNextSibling();
        }

        child = chelsea;    // re-start the iterator from the first child,
                    // REMIND - too bad we couldn't just back-patch the count ?

        int argIndex = -argSkipCount;
        if (firstArgDone && (child != null)) {
            child = child.getNextSibling();
            argIndex++;
            aload(contextLocal);
            addByteCode(ByteCode.SWAP);
        }
        else
            aload(contextLocal);
            
        if (firstArgDone && (type == TokenStream.NEW))
            constructArgArray(childCount - argSkipCount);
        
        boolean isSpecialCall = node.getProp(Node.SPECIALCALL_PROP) != null;
        boolean isSimpleCall = false;
        String simpleCallName = null;
        if (type != TokenStream.NEW) {
            simpleCallName = getSimpleCallName(node);
            if (simpleCallName != null && !isSpecialCall) {
                isSimpleCall = true;
                push(simpleCallName);
                aload(variableObjectLocal);
                child = child.getNextSibling().getNextSibling();
                argIndex = 0;
                push(childCount - argSkipCount);
                addByteCode(ByteCode.ANEWARRAY, "java/lang/Object");
            }
        }
        
        while (child != null) {
            if (argIndex < 0)       // not moving these arguments to the array
                generateCodeFromNode(child, node, -1, -1);
            else {
                addByteCode(ByteCode.DUP);
                push(argIndex);
                if (target != null) {
/*
    If this has also been a directCall sequence, the Number flag will
    have remained set for any parameter so that the values could be
    copied directly into the outgoing args. Here we want to force it
    to be treated as not in a Number context, so we set the flag off.
*/
                    boolean handled = false;
                    if ((child.getType() == TokenStream.GETVAR)
                            && inDirectCallFunction) {
                        OptLocalVariable lVar
                          = (OptLocalVariable)(child.getProp(Node.VARIABLE_PROP));
                        if (lVar.isParameter()) {
                            child.putProp(Node.ISNUMBER_PROP, null);
                            generateCodeFromNode(child, node, -1, -1);
                            handled = true;
                        }
                    }
                    if (!handled) {
                        Integer childNumberFlag
                                = (Integer)(child.getProp(Node.ISNUMBER_PROP));
                        if ((childNumberFlag != null)
                                && (childNumberFlag.intValue() == Node.BOTH)) {
                            addByteCode(ByteCode.NEW,"java/lang/Double");
                            addByteCode(ByteCode.DUP);
                            generateCodeFromNode(child, node, -1, -1);
                            addDoubleConstructor();
                        }
                        else
                            generateCodeFromNode(child, node, -1, -1);
                    }
                }
                else
                    generateCodeFromNode(child, node, -1, -1);
                addByteCode(ByteCode.AASTORE);
            }
            argIndex++;
            if (argIndex == 0) {
                                        // now we need to construct the array
                                        // (even if there are no args to load
                                        // into it) - REMIND could pass null
                                        // instead ?
                constructArgArray(childCount - argSkipCount);
            }
            child = child.getNextSibling();
        }

        String className;
        String methodNameNewObj;
        String methodNameCall;
        String callSignature;

        if (isSpecialCall) {
            className        = "org/mozilla/javascript/ScriptRuntime";
            methodNameNewObj = "newObjectSpecial";
            methodNameCall   = "callSpecial";
            if (type != TokenStream.NEW) {
                callSignature    = "(Lorg/mozilla/javascript/Context;" +
                                     "Ljava/lang/Object;" +
                                     "Ljava/lang/Object;" +
                                     "[Ljava/lang/Object;" +
                                     "Lorg/mozilla/javascript/Scriptable;" +
                                     "Lorg/mozilla/javascript/Scriptable;" +
                                     "Ljava/lang/String;I)";   // filename & linenumber
                aload(thisObjLocal);
                aload(variableObjectLocal);
                push(itsSourceFile == null ? "" : itsSourceFile);
                push(itsLineNumber);
            } else {
                callSignature    = "(Lorg/mozilla/javascript/Context;" +
                                    "Ljava/lang/Object;" +
                                    "[Ljava/lang/Object;" +
                                    "Lorg/mozilla/javascript/Scriptable;)";
                aload(variableObjectLocal);
            }
        } else {
            methodNameNewObj = "newObject";
            if (isSimpleCall) {
                callSignature  = "(Lorg/mozilla/javascript/Context;" +
                                  "Ljava/lang/String;" +
                                  "Lorg/mozilla/javascript/Scriptable;" +
                                  "[Ljava/lang/Object;)";
                methodNameCall = "callSimple"; 
                className      = "org/mozilla/javascript/optimizer/OptRuntime";
            } else {
                aload(variableObjectLocal);
                if (type == TokenStream.NEW) {
                    callSignature    = "(Lorg/mozilla/javascript/Context;" +
                                        "Ljava/lang/Object;" +
                                        "[Ljava/lang/Object;" +
                                        "Lorg/mozilla/javascript/Scriptable;)";
                } else {
                    callSignature    = "(Lorg/mozilla/javascript/Context;" +
                                        "Ljava/lang/Object;" +
                                        "Ljava/lang/Object;" +
                                        "[Ljava/lang/Object;" +
                                        "Lorg/mozilla/javascript/Scriptable;)";
                }
                methodNameCall   = "call"; 
                className        = "org/mozilla/javascript/ScriptRuntime";
            }
        }
        
        if (type == TokenStream.NEW) {
            addStaticInvoke(className,
                methodNameNewObj,
                callSignature,
                "Lorg/mozilla/javascript/Scriptable;");
        } else {
            addStaticInvoke(className,
                methodNameCall,
                callSignature,
                "Ljava/lang/Object;");
        }
    }

    private void visitStatement(Node node) {
        Object datum = node.getDatum();
        if (datum == null || !(datum instanceof Number))
            return;
        itsLineNumber = ((Number) datum).shortValue();
        if (itsLineNumber == -1)
            return;
        classFile.addLineNumberEntry((short)itsLineNumber);
    }


    private void visitTryCatchFinally(Node node, Node child) {
        /* Save the variable object, in case there are with statements
         * enclosed by the try block and we catch some exception.
         * We'll restore it for the catch block so that catch block
         * statements get the right scope.
         */

        // OPT we only need to do this if there are enclosed WITH
        // statements; could statically check and omit this if there aren't any.

        // XXX OPT Maybe instead do syntactic transforms to associate
        // each 'with' with a try/finally block that does the exitwith.

        // For that matter:  Why do we have leavewith?

        // XXX does Java have any kind of MOV(reg, reg)?
        short savedVariableObject = getNewWordLocal();
        aload(variableObjectLocal);
        astore(savedVariableObject);

        /*
         * Generate the code for the tree; most of the work is done in IRFactory
         * and NodeTransformer;  Codegen just adds the java handlers for the
         * javascript catch and finally clauses.  */
        // need to set the stack top to 1 to account for the incoming exception
        int startLabel = markLabel(acquireLabel(), (short)1);

        visitStatement(node);
        while (child != null) {
            generateCodeFromNode(child, node, -1, -1);
            child = child.getNextSibling();
        }

        Node catchTarget = (Node)node.getProp(Node.TARGET_PROP);
        Node finallyTarget = (Node)node.getProp(Node.FINALLY_PROP);
        
        // control flow skips the handlers
        int realEnd = acquireLabel();
        addByteCode(ByteCode.GOTO, realEnd);

        // javascript handler; unwrap exception and GOTO to javascript
        // catch area.
        if (catchTarget != null) {
            int jsHandler = classFile.markHandler(acquireLabel());

            // MS JVM gets cranky if the exception object is left on the stack
            short exceptionObject = getNewWordLocal();
            astore(exceptionObject);
            
            // reset the variable object local
            aload(savedVariableObject);
            astore(variableObjectLocal);

            aload(exceptionObject);
            releaseWordLocal(exceptionObject);

            // unwrap the exception...
            addScriptRuntimeInvoke("unwrapJavaScriptException",
                          "(Lorg/mozilla/javascript/JavaScriptException;)",
                          "Ljava/lang/Object;");

            // get the label to goto
            int catchLabel =
                ((Integer)catchTarget.getProp(Node.LABEL_PROP)).intValue();
            addByteCode(ByteCode.GOTO, catchLabel);

            // mark the handler
            classFile.addExceptionHandler
                (startLabel, catchLabel, jsHandler,
                 "org/mozilla/javascript/JavaScriptException");

            
            /*
                we also need to catch EcmaErrors and feed the 
                associated error object to the handler
            */
            jsHandler = classFile.markHandler(acquireLabel());
            exceptionObject = getNewWordLocal();
            astore(exceptionObject);
            aload(savedVariableObject);
            astore(variableObjectLocal);
            aload(exceptionObject);
            addVirtualInvoke("org/mozilla/javascript/EcmaError",
                             "getErrorObject", 
                             "()", 
                             "Lorg/mozilla/javascript/Scriptable;");
            releaseWordLocal(exceptionObject);
            addByteCode(ByteCode.GOTO, catchLabel);
            classFile.addExceptionHandler
                (startLabel, catchLabel, jsHandler,
                 "org/mozilla/javascript/EcmaError");
            
                 
        }

        // finally handler; catch all exceptions, store to a local; JSR to
        // the finally, then re-throw.
        if (finallyTarget != null) {
            int finallyHandler = classFile.markHandler(acquireLabel());

            // reset the variable object local
            aload(savedVariableObject);
            astore(variableObjectLocal);

            short exnLocal = itsLocalAllocationBase++;
            astore(exnLocal);

            // get the label to JSR to
            int finallyLabel =
                ((Integer)finallyTarget.getProp(Node.LABEL_PROP)).intValue();
            addByteCode(ByteCode.JSR, finallyLabel);

            // rethrow
            aload(exnLocal);
            addByteCode(ByteCode.ATHROW);

            // mark the handler
            classFile.addExceptionHandler(startLabel, finallyLabel,
                                          finallyHandler, null); // catch any
        }
        releaseWordLocal(savedVariableObject);
        markLabel(realEnd);
    }

    private void visitThrow(Node node, Node child) {
        visitStatement(node);
        while (child != null) {
            generateCodeFromNode(child, node, -1, -1);
            child = child.getNextSibling();
        }

        addByteCode(ByteCode.NEW,
                      "org/mozilla/javascript/JavaScriptException");
        addByteCode(ByteCode.DUP_X1);
        addByteCode(ByteCode.SWAP);
        addSpecialInvoke("org/mozilla/javascript/JavaScriptException",
                      "<init>", "(Ljava/lang/Object;)", "V");

        addByteCode(ByteCode.ATHROW);
    }

    private void visitReturn(Node node, Node child) {
        visitStatement(node);
        if (child != null) {
            do {
                generateCodeFromNode(child, node, -1, -1);
                child = child.getNextSibling();
            } while (child != null);
        } else if (inFunction) {
            pushUndefined();
        } else {
            aload(scriptResultLocal);
        }
        
        if (epilogueLabel == -1) 
            epilogueLabel = classFile.acquireLabel();
        addByteCode(ByteCode.GOTO, epilogueLabel);
    }

    private void visitSwitch(Node node, Node child) {
        visitStatement(node);
        while (child != null) {
            generateCodeFromNode(child, node, -1, -1);
            child = child.getNextSibling();
        }

        // save selector value
        short selector = getNewWordLocal();
        astore(selector);

        Vector cases = (Vector) node.getProp(Node.CASES_PROP);
        for (int i=0; i < cases.size(); i++) {
            Node thisCase = (Node) cases.elementAt(i);
            Node first = thisCase.getFirstChild();
            generateCodeFromNode(first, thisCase, -1, -1);
            aload(selector);
            addScriptRuntimeInvoke("seqB",
                                   "(Ljava/lang/Object;Ljava/lang/Object;)",
                                   "Ljava/lang/Boolean;");
            Node target = new Node(TokenStream.TARGET);
            thisCase.replaceChild(first, target);
            generateGOTO(TokenStream.IFEQ, target);
        }

        Node defaultNode = (Node) node.getProp(Node.DEFAULT_PROP);
        if (defaultNode != null) {
            Node defaultTarget = new Node(TokenStream.TARGET);
            defaultNode.getFirstChild().addChildToFront(defaultTarget);
            generateGOTO(TokenStream.GOTO, defaultTarget);
        }

        Node breakTarget = (Node) node.getProp(Node.BREAK_PROP);
        generateGOTO(TokenStream.GOTO, breakTarget);
    }

    private void generateGOTO(int type, Node target) {
        Node GOTO = new Node(type);
        GOTO.putProp(Node.TARGET_PROP, target);
        visitGOTO(GOTO, type, null);
    }

    private void visitUnary(Node node, Node child, int trueGOTO, int falseGOTO) {
        int op = node.getInt();
        switch (op) {
          case TokenStream.NOT:
          {
            if (trueGOTO != -1) {
                generateCodeFromNode(child, node, falseGOTO, trueGOTO);
                if (((child.getType() == TokenStream.UNARYOP) && (child.getInt() == TokenStream.NOT))
                        || (child.getType() == TokenStream.AND)
                        || (child.getType() == TokenStream.OR)
                        || (child.getType() == TokenStream.RELOP)
                        || (child.getType() == TokenStream.EQOP)) {
                }
                else {
                    addScriptRuntimeInvoke("toBoolean",
                                    "(Ljava/lang/Object;)", "Z");
                    addByteCode(ByteCode.IFNE, falseGOTO);
                    addByteCode(ByteCode.GOTO, trueGOTO);
                }
            }
            else {
                int trueTarget = acquireLabel();
                int falseTarget = acquireLabel();
                int beyond = acquireLabel();
                generateCodeFromNode(child, node, trueTarget, falseTarget);

                if (((child.getType() == TokenStream.UNARYOP) && (child.getInt() == TokenStream.NOT))
                        || (child.getType() == TokenStream.AND)
                        || (child.getType() == TokenStream.OR)
                        || (child.getType() == TokenStream.RELOP)
                        || (child.getType() == TokenStream.EQOP)) {
                }
                else {
                    addScriptRuntimeInvoke("toBoolean",
                                    "(Ljava/lang/Object;)", "Z");
                    addByteCode(ByteCode.IFEQ, falseTarget);
                    addByteCode(ByteCode.GOTO, trueTarget);
                }

                markLabel(trueTarget);
                classFile.add(ByteCode.GETSTATIC, "java/lang/Boolean",
                                        "FALSE", "Ljava/lang/Boolean;");
                addByteCode(ByteCode.GOTO, beyond);
                markLabel(falseTarget);
                classFile.add(ByteCode.GETSTATIC, "java/lang/Boolean",
                                        "TRUE", "Ljava/lang/Boolean;");
                markLabel(beyond);
                classFile.adjustStackTop(-1);
            }
            break;
          }

          case TokenStream.TYPEOF:
            visitTypeof(node, child);
            break;

          case TokenStream.VOID:
            generateCodeFromNode(child, node, -1, -1);
            addByteCode(ByteCode.POP);
            pushUndefined();
            break;

          case TokenStream.BITNOT:
            addByteCode(ByteCode.NEW, "java/lang/Double");
            addByteCode(ByteCode.DUP);
            generateCodeFromNode(child, node, -1, -1);
            addScriptRuntimeInvoke("toInt32",
                        "(Ljava/lang/Object;)", "I");
            push(-1);         // implement ~a as (a ^ -1)
            addByteCode(ByteCode.IXOR);
            addByteCode(ByteCode.I2D);
            addDoubleConstructor();
            break;

          case TokenStream.ADD:
          case TokenStream.SUB:
            addByteCode(ByteCode.NEW, "java/lang/Double");
            addByteCode(ByteCode.DUP);
            generateCodeFromNode(child, node, -1, -1);
            addScriptRuntimeInvoke("toNumber", "(Ljava/lang/Object;)", "D");
            if (op == TokenStream.SUB) {
                addByteCode(ByteCode.DNEG);
            }
            addDoubleConstructor();
            break;

          default:
            badTree();
        }
    }

    private void visitTypeof(Node node, Node child) {
        if (node.getType() == TokenStream.UNARYOP) {
            generateCodeFromNode(child, node, -1, -1);
            addScriptRuntimeInvoke("typeof",
                    "(Ljava/lang/Object;)", "Ljava/lang/String;");
            return;
        }
        String name = node.getString();
        if (hasVarsInRegs) {
            OptLocalVariable lVar = (OptLocalVariable) vars.getVariable(name);
            if (lVar != null) {
                if (lVar.isNumber()) {
                    push("number");
                    return;
                }
                visitGetVar(lVar, false, name);
                addScriptRuntimeInvoke("typeof",
                        "(Ljava/lang/Object;)", "Ljava/lang/String;");
                return;
            }
        }
        aload(variableObjectLocal);
        push(name);
        addScriptRuntimeInvoke("typeofName",
            "(Lorg/mozilla/javascript/Scriptable;Ljava/lang/String;)",
            "Ljava/lang/String;");
    }

    private void visitIncDec(Node node, boolean isInc)
    {
        Node child = node.getFirstChild();
        if (node.getProp(Node.ISNUMBER_PROP) != null) {
            OptLocalVariable lVar
                    = (OptLocalVariable)(child.getProp(Node.VARIABLE_PROP));
            if (lVar.getJRegister() == -1)
                lVar.assignJRegister(getNewWordPairLocal());
            dload(lVar.getJRegister());
            addByteCode(ByteCode.DUP2);
            push(1.0);
            addByteCode((isInc) ? ByteCode.DADD : ByteCode.DSUB);
            dstore(lVar.getJRegister());
        } else {
            OptLocalVariable lVar
                    = (OptLocalVariable)(child.getProp(Node.VARIABLE_PROP));
            String routine = (isInc) ? "postIncrement" : "postDecrement";
            if (hasVarsInRegs && child.getType() == TokenStream.GETVAR) {
                if (lVar == null) 
                    lVar = (OptLocalVariable) vars.getVariable(child.getString());
                if (lVar.getJRegister() == -1)
                    lVar.assignJRegister(getNewWordLocal());
                aload(lVar.getJRegister());
                addByteCode(ByteCode.DUP);
                addScriptRuntimeInvoke(routine,
                            "(Ljava/lang/Object;)", "Ljava/lang/Object;");
                astore(lVar.getJRegister());
            } else {
                if (child.getType() == TokenStream.GETPROP) {
                    Node getPropChild = child.getFirstChild();
                    generateCodeFromNode(getPropChild, node, -1, -1);
                    generateCodeFromNode(getPropChild.getNextSibling(), node, -1, -1);
                    aload(variableObjectLocal);
                    addScriptRuntimeInvoke(routine,
                            "(Ljava/lang/Object;Ljava/lang/String;" +
                            "Lorg/mozilla/javascript/Scriptable;)",
                            "Ljava/lang/Object;");
                }
                else {
                    if (child.getType() == TokenStream.GETELEM) {
                        routine += "Elem";
                        Node getPropChild = child.getFirstChild();
                        generateCodeFromNode(getPropChild, node, -1, -1);
                        generateCodeFromNode(getPropChild.getNextSibling(), node, -1, -1);
                        aload(variableObjectLocal);
                        addScriptRuntimeInvoke(routine,
                                "(Ljava/lang/Object;Ljava/lang/Object;" +
                                "Lorg/mozilla/javascript/Scriptable;)",
                                "Ljava/lang/Object;");
                    }
                    else {
                        aload(variableObjectLocal);
                        push(child.getString());          // push name
                        addScriptRuntimeInvoke(routine,
                                "(Lorg/mozilla/javascript/Scriptable;Ljava/lang/String;)",
                                "Ljava/lang/Object;");
                    }
                }
            }
        }
    }

    private boolean isArithmeticNode(Node node) {
        int type = node.getType();
        return (type == TokenStream.SUB)
                  || (type == TokenStream.MOD)
                        || (type == TokenStream.DIV)
                              || (type == TokenStream.MUL);
    }

    private void visitArithmetic(Node node, byte opCode, Node child, 
                                 Node parent) 
    {
        Integer childNumberFlag = (Integer)(node.getProp(Node.ISNUMBER_PROP));
        if (childNumberFlag != null) {
            generateCodeFromNode(child, node, -1, -1);
            generateCodeFromNode(child.getNextSibling(), node, -1, -1);
            addByteCode(opCode);
        }
        else {
            boolean childOfArithmetic = isArithmeticNode(parent);
            if (!childOfArithmetic) {
                addByteCode(ByteCode.NEW, "java/lang/Double");
                addByteCode(ByteCode.DUP);
            }
            generateCodeFromNode(child, node, -1, -1);
            if (!isArithmeticNode(child))
                addScriptRuntimeInvoke("toNumber", "(Ljava/lang/Object;)", "D");
            generateCodeFromNode(child.getNextSibling(), node, -1, -1);
            if (!isArithmeticNode(child.getNextSibling()))
                  addScriptRuntimeInvoke("toNumber", "(Ljava/lang/Object;)", "D");
            addByteCode(opCode);
            if (!childOfArithmetic) {
                addDoubleConstructor();
            }
        }
    }

    private void visitBitOp(Node node, int type, Node child) {
        Integer childNumberFlag = (Integer)(node.getProp(Node.ISNUMBER_PROP));
        if (childNumberFlag == null) {
            addByteCode(ByteCode.NEW, "java/lang/Double");
            addByteCode(ByteCode.DUP);
        }
        generateCodeFromNode(child, node, -1, -1);

        // special-case URSH; work with the target arg as a long, so
        // that we can return a 32-bit unsigned value, and call
        // toUint32 instead of toInt32.
        if (type == TokenStream.URSH) {
            addScriptRuntimeInvoke("toUint32", "(Ljava/lang/Object;)", "J");
            generateCodeFromNode(child.getNextSibling(), node, -1, -1);
            addScriptRuntimeInvoke("toInt32", "(Ljava/lang/Object;)", "I");
            // Looks like we need to explicitly mask the shift to 5 bits -
            // LUSHR takes 6 bits.
            push(31);
            addByteCode(ByteCode.IAND);
            addByteCode(ByteCode.LUSHR);
            addByteCode(ByteCode.L2D);
            addDoubleConstructor();
            return;
        }
        if (childNumberFlag == null) {
            addScriptRuntimeInvoke("toInt32", "(Ljava/lang/Object;)", "I");
            generateCodeFromNode(child.getNextSibling(), node, -1, -1);
            addScriptRuntimeInvoke("toInt32", "(Ljava/lang/Object;)", "I");
        }
        else {
            addScriptRuntimeInvoke("toInt32", "(D)", "I");
            generateCodeFromNode(child.getNextSibling(), node, -1, -1);
            addScriptRuntimeInvoke("toInt32", "(D)", "I");
        }
        switch (type) {
          case TokenStream.BITOR:
            addByteCode(ByteCode.IOR);
            break;
          case TokenStream.BITXOR:
            addByteCode(ByteCode.IXOR);
            break;
          case TokenStream.BITAND:
            addByteCode(ByteCode.IAND);
            break;
          case TokenStream.RSH:
            addByteCode(ByteCode.ISHR);
            break;
          case TokenStream.LSH:
            addByteCode(ByteCode.ISHL);
            break;
          default:
            badTree();
        }
        addByteCode(ByteCode.I2D);
        if (childNumberFlag == null) {
            addDoubleConstructor();
        }
    }

    private boolean nodeIsDirectCallParameter(Node node) {
        if (node.getType() == TokenStream.GETVAR) {
            OptLocalVariable lVar
                    = (OptLocalVariable)(node.getProp(Node.VARIABLE_PROP));
            if (lVar != null && lVar.isParameter() && inDirectCallFunction && 
                !itsForcedObjectParameters) 
            {
                return true;
            }
        }
        return false;
    }
    
    private void genSimpleCompare(int op, int trueGOTO, int falseGOTO) {
        switch (op) {
            case TokenStream.LE :
                addByteCode(ByteCode.DCMPG);
                addByteCode(ByteCode.IFLE, trueGOTO);
                break;
            case TokenStream.GE :
                addByteCode(ByteCode.DCMPL);
                addByteCode(ByteCode.IFGE, trueGOTO);
                break;
            case TokenStream.LT :
                addByteCode(ByteCode.DCMPG);
                addByteCode(ByteCode.IFLT, trueGOTO);
                break;
            case TokenStream.GT :
                addByteCode(ByteCode.DCMPL);
                addByteCode(ByteCode.IFGT, trueGOTO);
                break;
        }
        if (falseGOTO != -1)
            addByteCode(ByteCode.GOTO, falseGOTO);
    }
    
    private void visitGOTOingRelOp(Node node, Node child, Node parent,
                                   int trueGOTO, int falseGOTO)
    {
        int op = node.getInt();
        Integer childNumberFlag = (Integer)(node.getProp(Node.ISNUMBER_PROP));
        if ((childNumberFlag != null)
                    && (childNumberFlag.intValue() == Node.BOTH)) {
            generateCodeFromNode(child, node, -1, -1);
            generateCodeFromNode(child.getNextSibling(), node, -1, -1);
            genSimpleCompare(op, trueGOTO, falseGOTO);
        }
        else {
            if (op == TokenStream.INSTANCEOF) {
                aload(variableObjectLocal);
                generateCodeFromNode(child, node, -1, -1);
                generateCodeFromNode(child.getNextSibling(), node, -1, -1);
                addScriptRuntimeInvoke("instanceOf",
                              "(Lorg/mozilla/javascript/Scriptable;"+
                               "Ljava/lang/Object;Ljava/lang/Object;)", "Z");
                addByteCode(ByteCode.IFNE, trueGOTO);
                addByteCode(ByteCode.GOTO, falseGOTO);
            } else if (op == TokenStream.IN) {
                generateCodeFromNode(child, node, -1, -1);
                generateCodeFromNode(child.getNextSibling(), node, -1, -1);
                aload(variableObjectLocal);
                addScriptRuntimeInvoke("in",
                              "(Ljava/lang/Object;Ljava/lang/Object;"+
                               "Lorg/mozilla/javascript/Scriptable;)","Z");
                addByteCode(ByteCode.IFNE, trueGOTO);
                addByteCode(ByteCode.GOTO, falseGOTO);
            } else {
                Node rChild = child.getNextSibling();
                boolean leftIsDCP = nodeIsDirectCallParameter(child);
                boolean rightIsDCP = nodeIsDirectCallParameter(rChild);
                if (leftIsDCP || rightIsDCP) {
                    if (leftIsDCP) {
                        if (rightIsDCP) {
                            OptLocalVariable lVar1
                                = (OptLocalVariable)(child.getProp(Node.VARIABLE_PROP));                        
                            aload(lVar1.getJRegister());
                            classFile.add(ByteCode.GETSTATIC,
                                    "java/lang/Void",
                                    "TYPE",
                                    "Ljava/lang/Class;");
                            int notNumbersLabel = acquireLabel();
                            addByteCode(ByteCode.IF_ACMPNE, notNumbersLabel);
                            OptLocalVariable lVar2
                                = (OptLocalVariable)(rChild.getProp(Node.VARIABLE_PROP));                        
                            aload(lVar2.getJRegister());
                            classFile.add(ByteCode.GETSTATIC,
                                    "java/lang/Void",
                                    "TYPE",
                                    "Ljava/lang/Class;");
                            addByteCode(ByteCode.IF_ACMPNE, notNumbersLabel);
                            dload((short)(lVar1.getJRegister() + 1));
                            dload((short)(lVar2.getJRegister() + 1));
                            genSimpleCompare(op, trueGOTO, falseGOTO);
                            markLabel(notNumbersLabel);
                            // fall thru to generic handling
                        }
                        else {  // just the left child is a DCP, if the right child
                                // is a number it's worth testing the left
                            if ((childNumberFlag != null)
                                    && (childNumberFlag.intValue() == Node.RIGHT)) {
                                OptLocalVariable lVar1
                                    = (OptLocalVariable)(child.getProp(Node.VARIABLE_PROP));                        
                                aload(lVar1.getJRegister());
                                classFile.add(ByteCode.GETSTATIC,
                                        "java/lang/Void",
                                        "TYPE",
                                        "Ljava/lang/Class;");
                                int notNumbersLabel = acquireLabel();
                                addByteCode(ByteCode.IF_ACMPNE, notNumbersLabel);
                                dload((short)(lVar1.getJRegister() + 1));
                                generateCodeFromNode(rChild, node, -1, -1);
                                genSimpleCompare(op, trueGOTO, falseGOTO);
                                markLabel(notNumbersLabel);
                                // fall thru to generic handling
                            }
                        }
                    }
                    else {  //  just the right child is a DCP, if the left child
                            //  is a number it's worth testing the right
                        if ((childNumberFlag != null)
                                && (childNumberFlag.intValue() == Node.LEFT)) {
                            OptLocalVariable lVar2
                                = (OptLocalVariable)(rChild.getProp(Node.VARIABLE_PROP));                        
                            aload(lVar2.getJRegister());
                            classFile.add(ByteCode.GETSTATIC,
                                    "java/lang/Void",
                                    "TYPE",
                                    "Ljava/lang/Class;");
                            int notNumbersLabel = acquireLabel();
                            addByteCode(ByteCode.IF_ACMPNE, notNumbersLabel);
                            generateCodeFromNode(child, node, -1, -1);
                            dload((short)(lVar2.getJRegister() + 1));
                            genSimpleCompare(op, trueGOTO, falseGOTO);
                            markLabel(notNumbersLabel);
                            // fall thru to generic handling
                        }
                    }
                }
                generateCodeFromNode(child, node, -1, -1);
                generateCodeFromNode(rChild, node, -1, -1);
                if (childNumberFlag == null) {
                    if (op == TokenStream.GE || op == TokenStream.GT) {
                        addByteCode(ByteCode.SWAP);
                    }
                    String routine = ((op == TokenStream.LT)
                              || (op == TokenStream.GT)) ? "cmp_LT" : "cmp_LE";
                    addScriptRuntimeInvoke(routine,
                               "(Ljava/lang/Object;Ljava/lang/Object;)", "I");
                }
                else {
                    boolean doubleThenObject
                                   = (childNumberFlag.intValue() == Node.LEFT);
                    if (op == TokenStream.GE || op == TokenStream.GT) {
                        if (doubleThenObject) {
                            addByteCode(ByteCode.DUP_X2);
                            addByteCode(ByteCode.POP);
                            doubleThenObject = false;
                        }
                        else {
                            addByteCode(ByteCode.DUP2_X1);
                            addByteCode(ByteCode.POP2);
                            doubleThenObject = true;
                        }
                    }
                    String routine = ((op == TokenStream.LT)
                             || (op == TokenStream.GT)) ? "cmp_LT" : "cmp_LE";
                    if (doubleThenObject)
                        addOptRuntimeInvoke(routine,
                                 "(DLjava/lang/Object;)", "I");
                    else
                        addOptRuntimeInvoke(routine,
                              "(Ljava/lang/Object;D)", "I");
                }
                addByteCode(ByteCode.IFNE, trueGOTO);
                addByteCode(ByteCode.GOTO, falseGOTO);
            }
        }
    }

    private void visitRelOp(Node node, Node child, Node parent) {
        /*
            this is the version that returns an Object result
        */
        int op = node.getInt();
        Integer childNumberFlag = (Integer)(node.getProp(Node.ISNUMBER_PROP));
        if (((childNumberFlag != null)
                    && (childNumberFlag.intValue() == Node.BOTH))
                || (op == TokenStream.INSTANCEOF)
                || (op == TokenStream.IN)) 
        {
            if (op == TokenStream.INSTANCEOF)
                aload(variableObjectLocal);
            generateCodeFromNode(child, node, -1, -1);
            generateCodeFromNode(child.getNextSibling(), node, -1, -1);
            int trueGOTO = acquireLabel();
            int skip = acquireLabel();
            if (op == TokenStream.INSTANCEOF) {
                addScriptRuntimeInvoke("instanceOf",
                              "(Lorg/mozilla/javascript/Scriptable;"+
                               "Ljava/lang/Object;Ljava/lang/Object;)", "Z");
                addByteCode(ByteCode.IFNE, trueGOTO);
            }
            else {
                if (op == TokenStream.IN) {
                    aload(variableObjectLocal);
                    addScriptRuntimeInvoke("in",
                                  "(Ljava/lang/Object;Ljava/lang/Object;" +
                                   "Lorg/mozilla/javascript/Scriptable;)","Z");
                    addByteCode(ByteCode.IFNE, trueGOTO);
                }
                else {
                    genSimpleCompare(op, trueGOTO, -1);
                }
            }            
            classFile.add(ByteCode.GETSTATIC, "java/lang/Boolean",
                                    "FALSE", "Ljava/lang/Boolean;");
            addByteCode(ByteCode.GOTO, skip);
            markLabel(trueGOTO);
            classFile.add(ByteCode.GETSTATIC, "java/lang/Boolean",
                                    "TRUE", "Ljava/lang/Boolean;");
            markLabel(skip);
            classFile.adjustStackTop(-1);   // only have 1 of true/false
        }
        else {
            String routine = ((op == TokenStream.LT)
                     || (op == TokenStream.GT)) ? "cmp_LTB" : "cmp_LEB";
            generateCodeFromNode(child, node, -1, -1);
            generateCodeFromNode(child.getNextSibling(), node, -1, -1);
            if (childNumberFlag == null) {
                if (op == TokenStream.GE || op == TokenStream.GT) {
                    addByteCode(ByteCode.SWAP);
                }
                addScriptRuntimeInvoke(routine,
                           "(Ljava/lang/Object;Ljava/lang/Object;)",
                           "Ljava/lang/Boolean;");
            }
            else {
                boolean doubleThenObject
                               = (childNumberFlag.intValue() == Node.LEFT);
                if (op == TokenStream.GE || op == TokenStream.GT) {
                    if (doubleThenObject) {
                        addByteCode(ByteCode.DUP_X2);
                        addByteCode(ByteCode.POP);
                        doubleThenObject = false;
                    }
                    else {
                        addByteCode(ByteCode.DUP2_X1);
                        addByteCode(ByteCode.POP2);
                        doubleThenObject = true;
                    }
                }
                if (doubleThenObject)
                    addOptRuntimeInvoke(routine,
                             "(DLjava/lang/Object;)",
                             "Ljava/lang/Boolean;");
                else
                    addOptRuntimeInvoke(routine,
                          "(Ljava/lang/Object;D)",
                          "Ljava/lang/Boolean;");
            }
        }
    }
    
    private Number nodeIsConvertToObjectOfNumber(Node node)
    {
        if (node.getType() == TokenStream.CONVERT) {
            Object toType = node.getProp(Node.TYPE_PROP);
            if (toType == ScriptRuntime.ObjectClass) {
                Node convertChild = node.getFirstChild();
                if (convertChild.getType() == TokenStream.NUMBER)
                    return (Number)convertChild.getDatum();
            }
        }
        return null;
    }   

    private void visitEqOp(Node node, Node child, Node parent, int trueGOTO, int falseGOTO) {
        int op = node.getInt();
        Node rightChild = child.getNextSibling();
        if (trueGOTO == -1) {
            if (rightChild.getType() == TokenStream.PRIMARY &&
                rightChild.getInt() == TokenStream.NULL)
            {
                generateCodeFromNode(child, node, -1, -1);
                addByteCode(ByteCode.DUP);
                addByteCode(ByteCode.IFNULL, 15);
                pushUndefined();
                addByteCode(ByteCode.IF_ACMPEQ, 10);
                if ((op == TokenStream.EQ) || (op == TokenStream.SHEQ))
                    classFile.add(ByteCode.GETSTATIC, "java/lang/Boolean",
                                            "FALSE", "Ljava/lang/Boolean;");
                else
                    classFile.add(ByteCode.GETSTATIC, "java/lang/Boolean",
                                            "TRUE", "Ljava/lang/Boolean;");
                addByteCode(ByteCode.GOTO, 7);
                addByteCode(ByteCode.POP);
                if ((op == TokenStream.EQ) || (op == TokenStream.SHEQ))
                    classFile.add(ByteCode.GETSTATIC, "java/lang/Boolean",
                                            "TRUE", "Ljava/lang/Boolean;");
                else
                    classFile.add(ByteCode.GETSTATIC, "java/lang/Boolean",
                                            "FALSE", "Ljava/lang/Boolean;");
                return;
            }

            generateCodeFromNode(child, node, -1, -1);
            generateCodeFromNode(child.getNextSibling(), node, -1, -1);

            // JavaScript 1.2 uses shallow equality for == and !=
            String name;
            switch (op) {
              case TokenStream.EQ:
                name = version == Context.VERSION_1_2 ? "seqB" : "eqB";
                break;

              case TokenStream.NE:
                name = version == Context.VERSION_1_2 ? "sneB" : "neB";
                break;

              case TokenStream.SHEQ:
                name = "seqB";
                break;

              case TokenStream.SHNE:
                name = "sneB";
                break;

              default:
                name = null;
                badTree();
            }
            addScriptRuntimeInvoke(name,
                             "(Ljava/lang/Object;Ljava/lang/Object;)",
                             "Ljava/lang/Boolean;");
        }
        else {
            if (rightChild.getType() == TokenStream.PRIMARY &&
                rightChild.getInt() == TokenStream.NULL)
            {
                generateCodeFromNode(child, node, -1, -1);
                /*
                    since we have to test for null && undefined we end up having to
                    push the operand twice and so have to GOTO to a pop site if the
                    first test passes.
                    We can avoid that for operands that are 'simple' i.e. don't generate
                    a lot of code and don't have side-effects.
                    For now, 'simple' means GETVAR
                */
                boolean simpleChild = (child.getType() == TokenStream.GETVAR);
                if (!simpleChild) addByteCode(ByteCode.DUP);
                int popGOTO = acquireLabel();
                if ((op == TokenStream.EQ) || (op == TokenStream.SHEQ)) {
                    addByteCode(ByteCode.IFNULL,
                                    (simpleChild) ? trueGOTO : popGOTO);
                    short popStack = classFile.getStackTop();
                    if (simpleChild) generateCodeFromNode(child, node, -1, -1);
                    pushUndefined();
                    addByteCode(ByteCode.IF_ACMPEQ, trueGOTO);
                    addByteCode(ByteCode.GOTO, falseGOTO);
                    if (!simpleChild) {
                        markLabel(popGOTO, popStack);
                        addByteCode(ByteCode.POP);
                        addByteCode(ByteCode.GOTO, trueGOTO);
                    }
                }
                else {
                    addByteCode(ByteCode.IFNULL,
                                    (simpleChild) ? falseGOTO : popGOTO);
                    short popStack = classFile.getStackTop();
                    if (simpleChild) generateCodeFromNode(child, node, -1, -1);
                    pushUndefined();
                    addByteCode(ByteCode.IF_ACMPEQ, falseGOTO);
                    addByteCode(ByteCode.GOTO, trueGOTO);
                    if (!simpleChild) {
                        markLabel(popGOTO, popStack);
                        addByteCode(ByteCode.POP);
                        addByteCode(ByteCode.GOTO, falseGOTO);
                    }
                }
                return;
            }

            Node rChild = child.getNextSibling();
            
            Number numChild = nodeIsConvertToObjectOfNumber(rChild);
            if (nodeIsDirectCallParameter(child)
                    && (numChild != null)) {
                OptLocalVariable lVar1
                    = (OptLocalVariable)(child.getProp(Node.VARIABLE_PROP));                        
                aload(lVar1.getJRegister());
                classFile.add(ByteCode.GETSTATIC,
                        "java/lang/Void",
                        "TYPE",
                        "Ljava/lang/Class;");
                int notNumbersLabel = acquireLabel();
                addByteCode(ByteCode.IF_ACMPNE, notNumbersLabel);
                dload((short)(lVar1.getJRegister() + 1));
                push(numChild.doubleValue());
                addByteCode(ByteCode.DCMPL);
                if (op == TokenStream.EQ)
                    addByteCode(ByteCode.IFEQ, trueGOTO);
                else
                    addByteCode(ByteCode.IFNE, trueGOTO);
                addByteCode(ByteCode.GOTO, falseGOTO);
                markLabel(notNumbersLabel);
                // fall thru into generic handling
            }

            generateCodeFromNode(child, node, -1, -1);
            generateCodeFromNode(rChild, node, -1, -1);

            String name;
            switch (op) {
              case TokenStream.EQ:
                name = version == Context.VERSION_1_2 ? "shallowEq" : "eq";
                addScriptRuntimeInvoke(name,
                                "(Ljava/lang/Object;Ljava/lang/Object;)", "Z");
                break;

              case TokenStream.NE:
                name = version == Context.VERSION_1_2 ? "shallowNeq" : "neq";
                addOptRuntimeInvoke(name,
                                "(Ljava/lang/Object;Ljava/lang/Object;)", "Z");
                break;

              case TokenStream.SHEQ:
                name = "shallowEq";
                addScriptRuntimeInvoke(name,
                                "(Ljava/lang/Object;Ljava/lang/Object;)", "Z");
                break;

              case TokenStream.SHNE:
                name = "shallowNeq";
                addOptRuntimeInvoke(name,
                                "(Ljava/lang/Object;Ljava/lang/Object;)", "Z");
                break;

              default:
                name = null;
                badTree();
            }
            addByteCode(ByteCode.IFNE, trueGOTO);
            addByteCode(ByteCode.GOTO, falseGOTO);
        }
    }

    private void visitLiteral(Node node) {
        if (node.getType() == TokenStream.STRING) {
            // just load the string constant
            push(node.getString());
        } else {
       	    Number num = (Number) node.getDatum();
            if (node.getProp(Node.ISNUMBER_PROP) != null) {
                push(num.doubleValue());
            }
            else {
                    /*
                     * Generate code to create the new numeric constant
                     *
                     * new java/lang/<WrapperType>
                     * dup
                     * push <number>
                     * invokestatic java/lang/<WrapperType>/<init>(D)V
                     */
        	    String wrapperType = "";
        	    String signature = "";
        	    boolean isInteger = false;

        	    if (num instanceof Float)
        	      num = new Double(num.floatValue());

        	    // OPT: cache getClass() and compare == Type.class
        	    if (num instanceof Integer) {
            		wrapperType = "Integer";
            		signature = "I";
            		isInteger = true;
            		/*
            	    } else if (num instanceof Float) {
            		wrapperType = "Float";
            		signature = "F";
            		*/
        	    } else if (num instanceof Double) {
            		wrapperType = "Double";
            		signature = "D";
        	    } else if (num instanceof Byte) {
            		wrapperType = "Byte";
            		signature = "B";
            		isInteger = true;
        	    } else if (num instanceof Short) {
            		wrapperType = "Short";
            		signature = "S";
            		isInteger = true;
        	    } else {
            		throw Context.reportRuntimeError(
    		            "NumberNode contains unsupported Number type: " +
                        num.getClass().getName());
        	    }
        	    /*
        	        There appears to be a limit in the JVM on either the number of
        	        static fields in a class or the size of the class initializer.
        	        Either way, we can't have any more than 2000 statically init'd
        	        constants.    	    
        	    */
        	    if (itsConstantList.itsTop >= 2000) {
                    addByteCode(ByteCode.NEW, "java/lang/" + wrapperType);
                    addByteCode(ByteCode.DUP);
                    if (isInteger)
                	    push(num.longValue());
                    else
                		push(num.doubleValue());
                    addSpecialInvoke("java/lang/"
                                    + wrapperType,
                                    "<init>", 
                                    "(" + 
                                        signature
                                    + ")",
                                    "V");
        	    }
        	    else {
                    classFile.add(ByteCode.GETSTATIC,
                            classFile.fullyQualifiedForm(this.name),
                            "jsK_" 
                                  + itsConstantList.addConstant(wrapperType,
                                                                    signature,
                                                                    num,
                                                                    isInteger),
                            "Ljava/lang/" + wrapperType + ";");
                }
            }
        }
    }

    private void emitConstantDudeInitializers() {
        if (itsConstantList.itsTop == 0)
            return;
        classFile.startMethod("<clinit>", "()V",
            (short)(ClassFileWriter.ACC_STATIC + ClassFileWriter.ACC_FINAL));

        for (int i = 0; i < itsConstantList.itsTop; i++) {
            addByteCode(ByteCode.NEW,
                               "java/lang/"
                               + itsConstantList.itsList[i].itsWrapperType);
            addByteCode(ByteCode.DUP);
            if (itsConstantList.itsList[i].itsIsInteger)
        	    push(itsConstantList.itsList[i].itsLValue);
            else
        		push(itsConstantList.itsList[i].itsDValue);
            addSpecialInvoke("java/lang/"
                            + itsConstantList.itsList[i].itsWrapperType,
                            "<init>",
                            "(" +
                                itsConstantList.itsList[i].itsSignature
                            + ")",
                            "V");
            classFile.addField("jsK_" + i,
                       "Ljava/lang/"
                       + itsConstantList.itsList[i].itsWrapperType + ";",
                       ClassFileWriter.ACC_STATIC);
            classFile.add(ByteCode.PUTSTATIC,
                     classFile.fullyQualifiedForm(this.name),
                     "jsK_" + i,
                     "Ljava/lang/"
                     + itsConstantList.itsList[i].itsWrapperType + ";");
        }        
        addByteCode(ByteCode.RETURN);
        classFile.stopMethod((short)0, null);
    }

   private void visitPrimary(Node node) {
        int op = node.getInt();
        switch (op) {

          case TokenStream.THIS:
            aload(thisObjLocal);
            break;

          case TokenStream.THISFN:
            classFile.add(ByteCode.ALOAD_0);
            break;

          case TokenStream.NULL:
            addByteCode(ByteCode.ACONST_NULL);
            break;

          case TokenStream.TRUE:
            classFile.add(ByteCode.GETSTATIC, "java/lang/Boolean",
                                    "TRUE", "Ljava/lang/Boolean;");
            break;

          case TokenStream.FALSE:
            classFile.add(ByteCode.GETSTATIC, "java/lang/Boolean",
                                    "FALSE", "Ljava/lang/Boolean;");
            break;

          case TokenStream.UNDEFINED:
            pushUndefined();
            break;

          default:
            badTree();
        }
    }

    private void visitObject(Node node) {
        Node regexp = (Node) node.getProp(Node.REGEXP_PROP);
        String fieldName = (String)(regexp.getProp(Node.REGEXP_PROP));
        aload(funObjLocal);
        classFile.add(ByteCode.GETFIELD,
                        classFile.fullyQualifiedForm(this.name),
                        fieldName, "Lorg/mozilla/javascript/regexp/NativeRegExp;");
    }

    private void visitName(Node node) {
    	aload(variableObjectLocal);             // get variable object
        push(node.getString());                 // push name
    	addScriptRuntimeInvoke("name",
            "(Lorg/mozilla/javascript/Scriptable;Ljava/lang/String;)",
            "Ljava/lang/Object;");
    }

    private void visitSetName(Node node, Node child) {
        String name = node.getFirstChild().getString();
        while (child != null) {
            generateCodeFromNode(child, node, -1, -1);
            child = child.getNextSibling();
        }
        aload(variableObjectLocal);
        push(name);
        addScriptRuntimeInvoke("setName",
                "(Lorg/mozilla/javascript/Scriptable;Ljava/lang/Object;" +
                 "Lorg/mozilla/javascript/Scriptable;Ljava/lang/String;)",
                "Ljava/lang/Object;");
    }

    private void visitGetVar(OptLocalVariable lVar, boolean isNumber, 
                             String name) 
    {
        // TODO: Clean up use of lVar here and in set.
        if (hasVarsInRegs && lVar == null)
            lVar = (OptLocalVariable) vars.getVariable(name);
        if (lVar != null) {
            if (lVar.getJRegister() == -1)
                if (lVar.isNumber())
                    lVar.assignJRegister(getNewWordPairLocal());
                else
                    lVar.assignJRegister(getNewWordLocal());
            if (lVar.isParameter() && inDirectCallFunction && 
                !itsForcedObjectParameters) 
            {
/*
    Remember that here the isNumber flag means that we want to
    use the incoming parameter in a Number context, so test the
    object type and convert the value as necessary.
    
*/
                if (isNumber) {
                    aload(lVar.getJRegister());
                    classFile.add(ByteCode.GETSTATIC,
                            "java/lang/Void",
                            "TYPE",
                            "Ljava/lang/Class;");
                    int isNumberLabel = acquireLabel();
                    int beyond = acquireLabel();
                    addByteCode(ByteCode.IF_ACMPEQ, isNumberLabel);
                    aload(lVar.getJRegister());
                    addScriptRuntimeInvoke("toNumber", "(Ljava/lang/Object;)", "D");
                    addByteCode(ByteCode.GOTO, beyond);
                    markLabel(isNumberLabel);
                    dload((short)(lVar.getJRegister() + 1));
                    markLabel(beyond);
                } else {
                    aload(lVar.getJRegister());
                    classFile.add(ByteCode.GETSTATIC,
                            "java/lang/Void",
                            "TYPE",
                            "Ljava/lang/Class;");
                    int isNumberLabel = acquireLabel();
                    int beyond = acquireLabel();
                    addByteCode(ByteCode.IF_ACMPEQ, isNumberLabel);
                    aload(lVar.getJRegister());
                    addByteCode(ByteCode.GOTO, beyond);
                    markLabel(isNumberLabel);
                    addByteCode(ByteCode.NEW,"java/lang/Double");
                    addByteCode(ByteCode.DUP);
                    dload((short)(lVar.getJRegister() + 1));
                    addDoubleConstructor();
                    markLabel(beyond);
                }
            } else {
                if (lVar.isNumber())
                    dload(lVar.getJRegister());
                else
                    aload(lVar.getJRegister());
            }
            return;
        } 
    	
        aload(variableObjectLocal);
        push(name);
        aload(variableObjectLocal);
        addScriptRuntimeInvoke("getProp",
                        "(Ljava/lang/Object;Ljava/lang/String;" +
                        "Lorg/mozilla/javascript/Scriptable;)",
                        "Ljava/lang/Object;");
    }

    private void visitSetVar(Node node, Node child, boolean needValue) {
        OptLocalVariable lVar = (OptLocalVariable)(node.getProp(Node.VARIABLE_PROP));
        // XXX is this right? If so, clean up.
        if (hasVarsInRegs && lVar == null)
            lVar = (OptLocalVariable) vars.getVariable(child.getString());
        if (lVar != null) {
            generateCodeFromNode(child.getNextSibling(), node, -1, -1);
            if (lVar.getJRegister() == -1) {
                if (lVar.isNumber())
                    lVar.assignJRegister(getNewWordPairLocal());
                else
                    lVar.assignJRegister(getNewWordLocal());
            }
            if (lVar.isParameter() 
                        && inDirectCallFunction
                        && !itsForcedObjectParameters) {
                if (node.getProp(Node.ISNUMBER_PROP) != null) {
                    if (needValue) addByteCode(ByteCode.DUP2);
                    aload(lVar.getJRegister());
                    classFile.add(ByteCode.GETSTATIC,
                            "java/lang/Void",
                            "TYPE",
                            "Ljava/lang/Class;");
                    int isNumberLabel = acquireLabel();
                    int beyond = acquireLabel();
                    addByteCode(ByteCode.IF_ACMPEQ, isNumberLabel);
                    addByteCode(ByteCode.NEW,"java/lang/Double");
                    addByteCode(ByteCode.DUP);
                    addByteCode(ByteCode.DUP2_X2);
                    addByteCode(ByteCode.POP2);
                    addDoubleConstructor();
                    astore(lVar.getJRegister());
                    addByteCode(ByteCode.GOTO, beyond);
                    markLabel(isNumberLabel);
                    dstore((short)(lVar.getJRegister() + 1));
                    markLabel(beyond);
                }
                else {
                    if (needValue) addByteCode(ByteCode.DUP);
                    astore(lVar.getJRegister());
                }
            }
            else {
                if (node.getProp(Node.ISNUMBER_PROP) != null) {
                      dstore(lVar.getJRegister());
                      if (needValue) dload(lVar.getJRegister());
                }
                else {
                    astore(lVar.getJRegister());
                    if (needValue) aload(lVar.getJRegister());
                }
            }
            return;
        }
                      
        // default: just treat like any other name lookup
        child.setType(TokenStream.BINDNAME);
        node.setType(TokenStream.SETNAME);
        visitSetName(node, child);
        if (!needValue)
            addByteCode(ByteCode.POP);
    }
    

    private void visitGetProp(Node node, Node child) {
        String s = (String) node.getProp(Node.SPECIAL_PROP_PROP);
        if (s != null) {
            while (child != null) {
                generateCodeFromNode(child, node, -1, -1);
                child = child.getNextSibling();
            }
            aload(variableObjectLocal);
            String runtimeMethod = null;
            if (s.equals("__proto__")) {
                runtimeMethod = "getProto";
            } else if (s.equals("__parent__")) {
                runtimeMethod = "getParent";
            } else {
                badTree();
            }
            addScriptRuntimeInvoke(runtimeMethod,
                            "(Ljava/lang/Object;Lorg/mozilla/javascript/Scriptable;)",
                            "Lorg/mozilla/javascript/Scriptable;");
            return;
        }
        Node nameChild = child.getNextSibling();
        /*
            for 'this.foo' we call thisGet which can skip some
            casting overhead.
        
        */
        generateCodeFromNode(child, node, -1, -1);      // the object
        generateCodeFromNode(nameChild, node, -1, -1);  // the name  
        if (nameChild.getType() == TokenStream.STRING) {
            if ((child.getType() == TokenStream.PRIMARY &&
                        child.getInt() == TokenStream.THIS)
                  || ((child.getType() == TokenStream.NEWTEMP)
                        && (child.getFirstChild().getType() == TokenStream.PRIMARY)
                            && (child.getFirstChild().getInt() == TokenStream.THIS))
                        ) {
                aload(variableObjectLocal);
                addOptRuntimeInvoke("thisGet",
                                "(Lorg/mozilla/javascript/Scriptable;" +
                                "Ljava/lang/String;" +
                                "Lorg/mozilla/javascript/Scriptable;)",
                                "Ljava/lang/Object;");
            }
            else {
                aload(variableObjectLocal);
                addScriptRuntimeInvoke("getProp",
                                "(Ljava/lang/Object;Ljava/lang/String;" +
                                "Lorg/mozilla/javascript/Scriptable;)",
                                "Ljava/lang/Object;");
            }
        }
        else {
            aload(variableObjectLocal);
            addScriptRuntimeInvoke("getProp",
                            "(Ljava/lang/Object;Ljava/lang/String;" +
                            "Lorg/mozilla/javascript/Scriptable;)",
                            "Ljava/lang/Object;");
        }
    }

    private void visitSetProp(Node node, Node child) {
        String s = (String) node.getProp(Node.SPECIAL_PROP_PROP);
        if (s != null) {
            while (child != null) {
                generateCodeFromNode(child, node, -1, -1);
                child = child.getNextSibling();
            }
            aload(variableObjectLocal);
            String runtimeMethod = null;
            if (s.equals("__proto__")) {
                runtimeMethod = "setProto";
            } else if (s.equals("__parent__")) {
                runtimeMethod = "setParent";
            } else {
                badTree();
            }
            addScriptRuntimeInvoke(runtimeMethod,
                            "(Ljava/lang/Object;Ljava/lang/Object;" +
                            "Lorg/mozilla/javascript/Scriptable;)",
                            "Ljava/lang/Object;");
            return;
        }
        while (child != null) {
            generateCodeFromNode(child, node, -1, -1);
            child = child.getNextSibling();
        }
        aload(variableObjectLocal);
        addScriptRuntimeInvoke("setProp",
                        "(Ljava/lang/Object;Ljava/lang/String;" +
                        "Ljava/lang/Object;Lorg/mozilla/javascript/Scriptable;)",
                        "Ljava/lang/Object;");
    }

    private void visitBind(Node node, int type, Node child) {
        while (child != null) {
            generateCodeFromNode(child, node, -1, -1);
            child = child.getNextSibling();
        }
        // Generate code for "ScriptRuntime.bind(varObj, "s")"
        aload(variableObjectLocal);             // get variable object
        push(node.getString());                 // push name
        addScriptRuntimeInvoke(type == TokenStream.BINDNAME ? "bind" : "getBase",
                            "(Lorg/mozilla/javascript/Scriptable;Ljava/lang/String;)",
                            "Lorg/mozilla/javascript/Scriptable;");
    }

    private short getLocalFromNode(Node node) {
        Integer localProp = (Integer) node.getProp(Node.LOCAL_PROP);
        if (localProp == null) {
            // for NEWLOCAL & USELOCAL, use the next pre-allocated
            // register, otherwise for NEWTEMP & USETEMP, get the
            // next available from the pool
            short local = ((node.getType() == TokenStream.NEWLOCAL)
                                || (node.getType() == TokenStream.USELOCAL)) ?
                            itsLocalAllocationBase++ : getNewWordLocal();

            node.putProp(Node.LOCAL_PROP, new Integer(local));
            return local;
        } else {
            return localProp.shortValue();
        }
    }

    private void visitNewTemp(Node node, Node child) {
        while (child != null) {
            generateCodeFromNode(child, node, -1, -1);
            child = child.getNextSibling();
        }
        short local = getLocalFromNode(node);
        addByteCode(ByteCode.DUP);
        astore(local);
        Integer n = (Integer) node.getProp(Node.USES_PROP);
        if (n == null || n.intValue() == 0)
            releaseWordLocal(local);
    }

    private void visitUseTemp(Node node, Node child) {
        while (child != null) {
            generateCodeFromNode(child, node, -1, -1);
            child = child.getNextSibling();
        }
        Node temp = (Node) node.getProp(Node.TEMP_PROP);
        short local = getLocalFromNode(temp);

        // if the temp node has a magic TARGET property,
        // treat it as a RET to that temp.
        if (node.getProp(Node.TARGET_PROP) != null)
            addByteCode(ByteCode.RET, local);
        else
            aload(local);
        Integer n = (Integer) temp.getProp(Node.USES_PROP);
        if (n == null) {
            releaseWordLocal(local);
        } else {
            if (n.intValue() < Integer.MAX_VALUE) {
                int i = n.intValue() - 1;
                if (i == 0)
                    releaseWordLocal(local);
                temp.putProp(Node.USES_PROP, new Integer(i));
            }
        }
    }

    private void visitNewLocal(Node node, Node child) {
        while (child != null) {
            generateCodeFromNode(child, node, -1, -1);
            child = child.getNextSibling();
        }
        short local = getLocalFromNode(node);
        addByteCode(ByteCode.DUP);
        astore(local);
    }

    private void visitUseLocal(Node node, Node child) {
        while (child != null) {
            generateCodeFromNode(child, node, -1, -1);
            child = child.getNextSibling();
        }
        Node temp = (Node) node.getProp(Node.LOCAL_PROP);
        short local = getLocalFromNode(temp);

        // if the temp node has a magic TARGET property,
        // treat it as a RET to that temp.
        if (node.getProp(Node.TARGET_PROP) != null)
            addByteCode(ByteCode.RET, local);
        else
            aload(local);
    }

    private void dstore(short local) {
        xop(ByteCode.DSTORE_0, ByteCode.DSTORE, local);
    }

    private void istore(short local) {
        xop(ByteCode.ISTORE_0, ByteCode.ISTORE, local);
    }

    private void astore(short local) {
        xop(ByteCode.ASTORE_0, ByteCode.ASTORE, local);
    }

    private void xop(byte shortOp, byte op, short local) {
        switch (local) {
          case 0:
            addByteCode(shortOp);
            break;
          case 1:
            addByteCode((byte)(shortOp + 1));
            break;
          case 2:
            addByteCode((byte)(shortOp + 2));
            break;
          case 3:
            addByteCode((byte)(shortOp + 3));
            break;
          default:
            if (local < 0 || local >= Short.MAX_VALUE)
                throw new RuntimeException("bad local");
            if (local < Byte.MAX_VALUE) {
                addByteCode(op, (byte)local);
            } else {
                // Add wide opcode.
                addByteCode(ByteCode.WIDE);
                addByteCode(op);
                addByteCode((byte)(local >> 8));
                addByteCode((byte)(local & 0xff));
            }
            break;
        }
    }

    private void dload(short local) {
        xop(ByteCode.DLOAD_0, ByteCode.DLOAD, local);
    }

    private void iload(short local) {
        xop(ByteCode.ILOAD_0, ByteCode.ILOAD, local);
    }

    private void aload(short local) {
        xop(ByteCode.ALOAD_0, ByteCode.ALOAD, local);
    }

    private short getNewWordPairLocal() {
        short result = firstFreeLocal;
        while (true) {
            if (result >= (MAX_LOCALS - 1))
                break;
            if (!locals[result]
                    && !locals[result + 1])
                break;
            result++;
        }
        if (result < (MAX_LOCALS - 1)) {
            locals[result] = true;
            locals[result + 1] = true;
            if (result == firstFreeLocal) {
                for (int i = firstFreeLocal + 2; i < MAX_LOCALS; i++) {
                    if (!locals[i]) {
                        firstFreeLocal = (short) i;
                        if (localsMax < firstFreeLocal)
                            localsMax = firstFreeLocal;
                        return result;
                    }
                }
            }
            else {
                return result;
            }
        }
        throw Context.reportRuntimeError("Program too complex " +
                                         "(out of locals)");
    }

    private short reserveWordLocal(int local) {
        if (getNewWordLocal() != local)
            throw new RuntimeException("Local allocation error");
        return (short) local;
    }

    private short getNewWordLocal() {
        short result = firstFreeLocal;
        locals[result] = true;
        for (int i = firstFreeLocal + 1; i < MAX_LOCALS; i++) {
            if (!locals[i]) {
                firstFreeLocal = (short) i;
                if (localsMax < firstFreeLocal)
                    localsMax = firstFreeLocal;
                return result;
            }
        }
        throw Context.reportRuntimeError("Program too complex " +
                                         "(out of locals)");
    }

    private void releaseWordpairLocal(short local) {
        if (local < firstFreeLocal)
            firstFreeLocal = local;
        locals[local] = false;
        locals[local + 1] = false;
    }

    private void releaseWordLocal(short local) {
        if (local < firstFreeLocal)
            firstFreeLocal = local;
        locals[local] = false;
    }

    private void push(long l) {
        if (l == -1) {
            addByteCode(ByteCode.ICONST_M1);
        } else if (0 <= l && l <= 5) {
            addByteCode((byte) (ByteCode.ICONST_0 + l));
        } else if (Byte.MIN_VALUE <= l && l <= Byte.MAX_VALUE) {
            addByteCode(ByteCode.BIPUSH, (byte) l);
        } else if (Short.MIN_VALUE <= l && l <= Short.MAX_VALUE) {
            addByteCode(ByteCode.SIPUSH, (short) l);
    	} else if (Integer.MIN_VALUE <= l && l <= Integer.MAX_VALUE) {
    	    classFile.addLoadConstant((int)l);
    	} else {
            classFile.addLoadConstant((long)l);
        }
    }

    private void push(double d) {
        if (d == 0.0) {
            addByteCode(ByteCode.DCONST_0);
        } else if (d == 1.0) {
            addByteCode(ByteCode.DCONST_1);
	    /* XXX this breaks all sorts of simple math.
        } else if (Float.MIN_VALUE <= d && d <= Float.MAX_VALUE) {
	    loadWordConstant(classFile.addFloatConstant((float) d));
	    */
    	} else {
            classFile.addLoadConstant((double)d);
        }
    }

    private void push(String s) {
        classFile.addLoadConstant(s);
    }

    private void pushUndefined() {
        classFile.add(ByteCode.GETSTATIC, "org/mozilla/javascript/Undefined",
                "instance", "Lorg/mozilla/javascript/Scriptable;");
    }


    private void badTree() {
        throw new RuntimeException("Bad tree in codegen");
    }

    private static final String normalFunctionSuperClassName =
                          "org.mozilla.javascript.NativeFunction";
    private static final String normalScriptSuperClassName =
                          "org.mozilla.javascript.NativeScript";
    private static final String debugFunctionSuperClassName =
                          "org.mozilla.javascript.debug.NativeFunctionDebug";
    private static final String debugScriptSuperClassName =
                          "org.mozilla.javascript.debug.NativeScriptDebug";
    private String superClassName;
    private String superClassSlashName;
    private String name;
    private int ordinal;
    boolean inFunction;
    boolean inDirectCallFunction;
    private ClassFileWriter classFile;
    private Vector namesVector;
    private Vector classFilesVector;
    private short scriptRuntimeIndex;
    private int version;
    private OptClassNameHelper itsNameHelper;
    
    private String itsSourceFile;
    private int itsLineNumber;

    private int stackDepth;
    private int stackDepthMax;

    private static final int MAX_LOCALS = 256;
    private boolean[] locals;
    private short firstFreeLocal;
    private short localsMax;

    private ConstantList itsConstantList;

    // special known locals. If you add a new local here, be sure
    // to initialize it to -1 in startNewMethod
    private short variableObjectLocal;
    private short scriptResultLocal;
    private short contextLocal;
    private short argsLocal;
    private short thisObjLocal;
    private short funObjLocal;
    private short debug_pcLocal;
    private short debugStopSubRetLocal;
    private short itsZeroArgArray;
    private short itsOneArgArray;

    private boolean hasVarsInRegs;
    private boolean itsForcedObjectParameters;
    private boolean trivialInit;
    private short itsLocalAllocationBase;
    private OptVariableTable vars;
    private OptVariableTable debugVars;
    private int epilogueLabel;
    private int optLevel;
}

class ConstantList {

    int addConstant(String wrapperType, String signature, Number num, boolean isInteger)
    {
        long lValue = num.longValue();
        double dValue = num.doubleValue();

        for (int i = 0; i < itsTop; i++) {
            if (signature.equals(itsList[i].itsSignature)) {
                if (itsList[i].itsIsInteger) {
                    if (isInteger && (lValue == itsList[i].itsLValue)) {
                        return i;
                    }
                }
                else {
                    if (!isInteger && (dValue == itsList[i].itsDValue)) {
                        return i;
                    }
                }
            }
        }

        if (itsTop == itsList.length) {
            ConstantDude[] newList = new ConstantDude[itsList.length * 2];
            System.arraycopy(itsList, 0, newList, 0, itsList.length);
            itsList = newList;
        }
        if (isInteger) {
            itsList[itsTop] = new ConstantDude(wrapperType, signature, lValue);
        }
        else {
            itsList[itsTop] = new ConstantDude(wrapperType, signature, dValue);
        }
        return itsTop++;
    }

    ConstantDude[] itsList = new ConstantDude[128];
    int itsTop;

}

class ConstantDude {

    ConstantDude(String wrapperType, String signature, long value)
    {
        itsWrapperType = wrapperType;
        itsSignature = signature;
        itsIsInteger = true;
        itsLValue = value;
    }

    ConstantDude(String wrapperType, String signature, double value)
    {
        itsWrapperType = wrapperType;
        itsSignature = signature;
        itsIsInteger = false;
        itsDValue = value;
    }

    String itsWrapperType;
    String itsSignature;
    long itsLValue;
    double itsDValue;
    boolean itsIsInteger;

}
