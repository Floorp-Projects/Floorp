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
 * Kemal Bayram
 * Igor Bukanov
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
import java.io.IOException;
import java.lang.reflect.Constructor;

/**
 * This class generates code for a given IR tree.
 *
 * @author Norris Boyd
 * @author Roger Lawrence
 */

public class Codegen extends Interpreter
{
    public Object compile(Scriptable scope,
                          CompilerEnvirons compilerEnv,
                          ScriptOrFnNode scriptOrFn,
                          String encodedSource,
                          boolean returnFunction,
                          Object securityDomain)
    {
        int serial;
        synchronized (globalLock) {
            serial = ++globalSerialClassCounter;
        }
        String mainClassName = "org.mozilla.javascript.gen.c"+serial;

        byte[] mainClassBytes = compileToClassFile(compilerEnv, mainClassName,
                                                   scriptOrFn, encodedSource,
                                                   returnFunction);

        Exception e = null;
        Class result = null;
        GeneratedClassLoader
            loader = SecurityController.createLoader(null, securityDomain);

        try {
            result = loader.defineClass(mainClassName, mainClassBytes);
            loader.linkClass(result);
        } catch (SecurityException x) {
            e = x;
        } catch (IllegalArgumentException x) {
            e = x;
        }
        if (e != null)
            throw new RuntimeException("Malformed optimizer package " + e);

        if (returnFunction) {
            Context cx = Context.getCurrentContext();
            NativeFunction f;
            try {
                Constructor ctor = result.getConstructors()[0];
                Object[] initArgs = { scope, cx, new Integer(0) };
                f = (NativeFunction)ctor.newInstance(initArgs);
            } catch (Exception ex) {
                throw new RuntimeException
                    ("Unable to instantiate compiled class:"+ex.toString());
            }
            OptRuntime.initFunction(
                f, FunctionNode.FUNCTION_STATEMENT, scope, cx);
            return f;
        } else {
            Script script;
            try {
                script = (Script) result.newInstance();
            } catch (Exception ex) {
                throw new RuntimeException
                    ("Unable to instantiate compiled class:"+ex.toString());
            }
            return script;
        }
    }

    public void notifyDebuggerCompilationDone(Context cx,
                                              ScriptOrFnNode scriptOrFn,
                                              String debugSource)
    {
        throw new RuntimeException("NOT SUPPORTED");
    }

    byte[] compileToClassFile(CompilerEnvirons compilerEnv,
                              String mainClassName,
                              ScriptOrFnNode scriptOrFn,
                              String encodedSource,
                              boolean returnFunction)
    {
        this.compilerEnv = compilerEnv;

        transform(scriptOrFn);

        if (Token.printTrees) {
            System.out.println(scriptOrFn.toStringTree(scriptOrFn));
        }

        if (returnFunction) {
            scriptOrFn = scriptOrFn.getFunctionNode(0);
        }

        initScriptOrFnNodesData(scriptOrFn);

        this.mainClassName = mainClassName;
        mainClassSignature
            = ClassFileWriter.classNameToSignature(mainClassName);

        return generateCode(encodedSource);
    }

    private void transform(ScriptOrFnNode tree)
    {
        initOptFunctions_r(tree);

        int optLevel = compilerEnv.getOptimizationLevel();

        Hashtable possibleDirectCalls = null;
        if (optLevel > 0) {
           /*
            * Collect all of the contained functions into a hashtable
            * so that the call optimizer can access the class name & parameter
            * count for any call it encounters
            */
            if (tree.getType() == Token.SCRIPT) {
                int functionCount = tree.getFunctionCount();
                for (int i = 0; i != functionCount; ++i) {
                    OptFunctionNode ofn = OptFunctionNode.get(tree, i);
                    if (ofn.fnode.getFunctionType()
                        == FunctionNode.FUNCTION_STATEMENT)
                    {
                        String name = ofn.fnode.getFunctionName();
                        if (name.length() != 0) {
                            if (possibleDirectCalls == null) {
                                possibleDirectCalls = new Hashtable();
                            }
                            possibleDirectCalls.put(name, ofn);
                        }
                    }
                }
            }
        }

        if (possibleDirectCalls != null) {
            directCallTargets = new ObjArray();
        }

        OptTransformer ot = new OptTransformer(compilerEnv,
                                               possibleDirectCalls,
                                               directCallTargets);
        ot.transform(tree);

        if (optLevel > 0) {
            (new Optimizer()).optimize(tree, optLevel);
        }
    }

    private static void initOptFunctions_r(ScriptOrFnNode scriptOrFn)
    {
        for (int i = 0, N = scriptOrFn.getFunctionCount(); i != N; ++i) {
            FunctionNode fn = scriptOrFn.getFunctionNode(i);
            new OptFunctionNode(fn);
            initOptFunctions_r(fn);
        }
    }

    private void initScriptOrFnNodesData(ScriptOrFnNode scriptOrFn)
    {
        ObjArray x = new ObjArray();
        collectScriptOrFnNodes_r(scriptOrFn, x);

        int count = x.size();
        scriptOrFnNodes = new ScriptOrFnNode[count];
        x.toArray(scriptOrFnNodes);

        scriptOrFnIndexes = new ObjToIntMap(count);
        for (int i = 0; i != count; ++i) {
            scriptOrFnIndexes.put(scriptOrFnNodes[i], i);
        }
    }

    private static void collectScriptOrFnNodes_r(ScriptOrFnNode n,
                                                 ObjArray x)
    {
        x.add(n);
        int nestedCount = n.getFunctionCount();
        for (int i = 0; i != nestedCount; ++i) {
            collectScriptOrFnNodes_r(n.getFunctionNode(i), x);
        }
    }

    private byte[] generateCode(String encodedSource)
    {
        boolean hasScript = (scriptOrFnNodes[0].getType() == Token.SCRIPT);
        boolean hasFunctions = (scriptOrFnNodes.length > 1 || !hasScript);

        String sourceFile = null;
        if (compilerEnv.isGenerateDebugInfo()) {
            sourceFile = scriptOrFnNodes[0].getSourceName();
        }

        ClassFileWriter cfw = new ClassFileWriter(mainClassName,
                                                  SUPER_CLASS_NAME,
                                                  sourceFile);
        cfw.addField(ID_FIELD_NAME, "I",
                     ClassFileWriter.ACC_PRIVATE);
        cfw.addField(DIRECT_CALL_PARENT_FIELD, mainClassSignature,
                     ClassFileWriter.ACC_PRIVATE);
        cfw.addField(REGEXP_ARRAY_FIELD_NAME, REGEXP_ARRAY_FIELD_TYPE,
                     ClassFileWriter.ACC_PRIVATE);

        if (hasFunctions) {
            generateFunctionConstructor(cfw);
        }

        if (hasScript) {
            ScriptOrFnNode script = scriptOrFnNodes[0];
            cfw.addInterface("org/mozilla/javascript/Script");
            generateScriptCtor(cfw, script);
            generateMain(cfw);
            generateExecute(cfw, script);
        }

        generateCallMethod(cfw);
        if (encodedSource != null) {
            generateGetEncodedSource(cfw, encodedSource);
        }

        int count = scriptOrFnNodes.length;
        for (int i = 0; i != count; ++i) {
            ScriptOrFnNode n = scriptOrFnNodes[i];

            BodyCodegen bodygen = new BodyCodegen();
            bodygen.cfw = cfw;
            bodygen.codegen = this;
            bodygen.compilerEnv = compilerEnv;
            bodygen.scriptOrFn = n;

            bodygen.generateBodyCode();

            if (n.getType() == Token.FUNCTION) {
                OptFunctionNode ofn = OptFunctionNode.get(n);
                generateFunctionInit(cfw, ofn);
                if (ofn.isTargetOfDirectCall()) {
                    emitDirectConstructor(cfw, ofn);
                }
            }
        }

        if (directCallTargets != null) {
            int N = directCallTargets.size();
            for (int j = 0; j != N; ++j) {
                cfw.addField(getDirectTargetFieldName(j),
                             mainClassSignature,
                             ClassFileWriter.ACC_PRIVATE);
            }
        }

        emitRegExpInit(cfw);
        emitConstantDudeInitializers(cfw);

        return cfw.toByteArray();
    }

    private void emitDirectConstructor(ClassFileWriter cfw,
                                       OptFunctionNode ofn)
    {
/*
    we generate ..
        Scriptable directConstruct(<directCallArgs>) {
            Scriptable newInstance = createObject(cx, scope);
            Object val = <body-name>(cx, scope, newInstance, <directCallArgs>);
            if (val instanceof Scriptable && val != Undefined.instance) {
                return (Scriptable) val;
            }
            return newInstance;
        }
*/
        cfw.startMethod(getDirectCtorName(ofn.fnode),
                        getBodyMethodSignature(ofn.fnode),
                        (short)(ClassFileWriter.ACC_STATIC
                                | ClassFileWriter.ACC_PRIVATE));

        int argCount = ofn.fnode.getParamCount();
        int firstLocal = (4 + argCount * 3) + 1;

        cfw.addALoad(0); // this
        cfw.addALoad(1); // cx
        cfw.addALoad(2); // scope
        cfw.addInvoke(ByteCode.INVOKEVIRTUAL,
                      "org/mozilla/javascript/BaseFunction",
                      "createObject",
                      "(Lorg/mozilla/javascript/Context;"
                      +"Lorg/mozilla/javascript/Scriptable;"
                      +")Lorg/mozilla/javascript/Scriptable;");
        cfw.addAStore(firstLocal);

        cfw.addALoad(0);
        cfw.addALoad(1);
        cfw.addALoad(2);
        cfw.addALoad(firstLocal);
        for (int i = 0; i < argCount; i++) {
            cfw.addALoad(4 + (i * 3));
            cfw.addDLoad(5 + (i * 3));
        }
        cfw.addALoad(4 + argCount * 3);
        cfw.addInvoke(ByteCode.INVOKESTATIC,
                      mainClassName,
                      getBodyMethodName(ofn.fnode),
                      getBodyMethodSignature(ofn.fnode));
        int exitLabel = cfw.acquireLabel();
        cfw.add(ByteCode.DUP); // make a copy of direct call result
        cfw.add(ByteCode.INSTANCEOF, "org/mozilla/javascript/Scriptable");
        cfw.add(ByteCode.IFEQ, exitLabel);
        cfw.add(ByteCode.DUP); // make a copy of direct call result
        pushUndefined(cfw);
        cfw.add(ByteCode.IF_ACMPEQ, exitLabel);
        // cast direct call result
        cfw.add(ByteCode.CHECKCAST, "org/mozilla/javascript/Scriptable");
        cfw.add(ByteCode.ARETURN);
        cfw.markLabel(exitLabel);

        cfw.addALoad(firstLocal);
        cfw.add(ByteCode.ARETURN);

        cfw.stopMethod((short)(firstLocal + 1));

    }

    private void generateCallMethod(ClassFileWriter cfw)
    {
        cfw.startMethod("call",
                        "(Lorg/mozilla/javascript/Context;" +
                        "Lorg/mozilla/javascript/Scriptable;" +
                        "Lorg/mozilla/javascript/Scriptable;" +
                        "[Ljava/lang/Object;)Ljava/lang/Object;",
                        (short)(ClassFileWriter.ACC_PUBLIC
                                | ClassFileWriter.ACC_FINAL));

        cfw.addALoad(0);
        cfw.addALoad(1);
        cfw.addALoad(2);
        cfw.addALoad(3);
        cfw.addALoad(4);

        int end = scriptOrFnNodes.length;
        boolean generateSwitch = (2 <= end);

        int switchStart = 0;
        int switchStackTop = 0;
        if (generateSwitch) {
            cfw.addLoadThis();
            cfw.add(ByteCode.GETFIELD, cfw.getClassName(), ID_FIELD_NAME, "I");
            // do switch from (1,  end - 1) mapping 0 to
            // the default case
            switchStart = cfw.addTableSwitch(1, end - 1);
        }

        for (int i = 0; i != end; ++i) {
            ScriptOrFnNode n = scriptOrFnNodes[i];
            if (generateSwitch) {
                if (i == 0) {
                    cfw.markTableSwitchDefault(switchStart);
                    switchStackTop = cfw.getStackTop();
                } else {
                    cfw.markTableSwitchCase(switchStart, i - 1,
                                            switchStackTop);
                }
            }
            if (n.getType() == Token.FUNCTION) {
                OptFunctionNode ofn = OptFunctionNode.get(n);
                if (ofn.isTargetOfDirectCall()) {
                    int pcount = ofn.fnode.getParamCount();
                    if (pcount != 0) {
                        // loop invariant:
                        // stack top == arguments array from addALoad4()
                        for (int p = 0; p != pcount; ++p) {
                            cfw.add(ByteCode.ARRAYLENGTH);
                            cfw.addPush(p);
                            int undefArg = cfw.acquireLabel();
                            int beyond = cfw.acquireLabel();
                            cfw.add(ByteCode.IF_ICMPLE, undefArg);
                            // get array[p]
                            cfw.addALoad(4);
                            cfw.addPush(p);
                            cfw.add(ByteCode.AALOAD);
                            cfw.add(ByteCode.GOTO, beyond);
                            cfw.markLabel(undefArg);
                            pushUndefined(cfw);
                            cfw.markLabel(beyond);
                            // Only one push
                            cfw.adjustStackTop(-1);
                            cfw.addPush(0.0);
                            // restore invariant
                            cfw.addALoad(4);
                        }
                    }
                }
            }
            cfw.addInvoke(ByteCode.INVOKESTATIC,
                          mainClassName,
                          getBodyMethodName(n),
                          getBodyMethodSignature(n));
            cfw.add(ByteCode.ARETURN);
        }
        cfw.stopMethod((short)5);
        // 5: this, cx, scope, js this, args[]
    }

    private static void generateMain(ClassFileWriter cfw)
    {
        cfw.startMethod("main", "([Ljava/lang/String;)V",
                        (short)(ClassFileWriter.ACC_PUBLIC
                                | ClassFileWriter.ACC_STATIC));

        // load new ScriptImpl()
        cfw.add(ByteCode.NEW, cfw.getClassName());
        cfw.add(ByteCode.DUP);
        cfw.addInvoke(ByteCode.INVOKESPECIAL, cfw.getClassName(),
                      "<init>", "()V");
         // load 'args'
        cfw.add(ByteCode.ALOAD_0);
        cfw.addInvoke(ByteCode.INVOKESTATIC,
                      "org/mozilla/javascript/ScriptRuntime",
                      "main",
                      "(Lorg/mozilla/javascript/Script;[Ljava/lang/String;)V");
        cfw.add(ByteCode.RETURN);
        // 1 = String[] args
        cfw.stopMethod((short)1);
    }

    private void generateExecute(ClassFileWriter cfw, ScriptOrFnNode script)
    {
        cfw.startMethod("exec",
                        "(Lorg/mozilla/javascript/Context;"
                        +"Lorg/mozilla/javascript/Scriptable;"
                        +")Ljava/lang/Object;",
                        (short)(ClassFileWriter.ACC_PUBLIC
                                | ClassFileWriter.ACC_FINAL));

        final int CONTEXT_ARG = 1;
        final int SCOPE_ARG = 2;

        cfw.addLoadThis();
        cfw.addALoad(CONTEXT_ARG);
        cfw.addALoad(SCOPE_ARG);
        cfw.add(ByteCode.DUP);
        cfw.add(ByteCode.ACONST_NULL);
        cfw.addInvoke(ByteCode.INVOKEVIRTUAL,
                      cfw.getClassName(),
                      "call",
                      "(Lorg/mozilla/javascript/Context;"
                      +"Lorg/mozilla/javascript/Scriptable;"
                      +"Lorg/mozilla/javascript/Scriptable;"
                      +"[Ljava/lang/Object;"
                      +")Ljava/lang/Object;");

        cfw.add(ByteCode.ARETURN);
        // 3 = this + context + scope
        cfw.stopMethod((short)3);
    }

    private void generateScriptCtor(ClassFileWriter cfw,
                                    ScriptOrFnNode script)
    {
        cfw.startMethod("<init>", "()V", ClassFileWriter.ACC_PUBLIC);

        cfw.addLoadThis();
        cfw.addInvoke(ByteCode.INVOKESPECIAL, SUPER_CLASS_NAME,
                      "<init>", "()V");
        // set id to 0
        cfw.addLoadThis();
        cfw.addPush(0);
        cfw.add(ByteCode.PUTFIELD, cfw.getClassName(), ID_FIELD_NAME, "I");

        // Call
        // NativeFunction.initScriptFunction(version, "", varNamesArray, 0)

        cfw.addLoadThis();
        cfw.addPush(compilerEnv.getLanguageVersion());
        cfw.addPush(""); // Function name
        pushParamNamesArray(cfw, script);
        cfw.addPush(0); // No parameters, only varnames
        cfw.addInvoke(ByteCode.INVOKEVIRTUAL,
                    "org/mozilla/javascript/NativeFunction",
                    "initScriptFunction",
                    "(ILjava/lang/String;[Ljava/lang/String;I)V");

        cfw.add(ByteCode.RETURN);
        // 1 parameter = this
        cfw.stopMethod((short)1);
    }

    private void generateFunctionConstructor(ClassFileWriter cfw)
    {
        final byte SCOPE_ARG = 1;
        final byte CONTEXT_ARG = 2;
        final byte ID_ARG = 3;

        cfw.startMethod("<init>", FUNCTION_CONSTRUCTOR_SIGNATURE,
                        ClassFileWriter.ACC_PUBLIC);
        cfw.addALoad(0);
        cfw.addInvoke(ByteCode.INVOKESPECIAL, SUPER_CLASS_NAME,
                      "<init>", "()V");

        cfw.addLoadThis();
        cfw.addILoad(ID_ARG);
        cfw.add(ByteCode.PUTFIELD, cfw.getClassName(), ID_FIELD_NAME, "I");

        cfw.addLoadThis();
        cfw.addALoad(CONTEXT_ARG);
        cfw.addALoad(SCOPE_ARG);

        int start = (scriptOrFnNodes[0].getType() == Token.SCRIPT) ? 1 : 0;
        int end = scriptOrFnNodes.length;
        if (start == end) throw badTree();
        boolean generateSwitch = (2 <= end - start);

        int switchStart = 0;
        int switchStackTop = 0;
        if (generateSwitch) {
            cfw.addILoad(ID_ARG);
            // do switch from (start + 1,  end - 1) mapping start to
            // the default case
            switchStart = cfw.addTableSwitch(start + 1, end - 1);
        }

        for (int i = start; i != end; ++i) {
            if (generateSwitch) {
                if (i == start) {
                    cfw.markTableSwitchDefault(switchStart);
                    switchStackTop = cfw.getStackTop();
                } else {
                    cfw.markTableSwitchCase(switchStart, i - 1 - start,
                                            switchStackTop);
                }
            }
            OptFunctionNode ofn = OptFunctionNode.get(scriptOrFnNodes[i]);
            cfw.addInvoke(ByteCode.INVOKEVIRTUAL,
                          mainClassName,
                          getFunctionInitMethodName(ofn),
                          FUNCTION_INIT_SIGNATURE);
            cfw.add(ByteCode.RETURN);
        }

        // 4 = this + scope + context + id
        cfw.stopMethod((short)4);
    }

    private void generateFunctionInit(ClassFileWriter cfw,
                                      OptFunctionNode ofn)
    {
        final int CONTEXT_ARG = 1;
        final int SCOPE_ARG = 2;
        cfw.startMethod(getFunctionInitMethodName(ofn),
                        FUNCTION_INIT_SIGNATURE,
                        (short)(ClassFileWriter.ACC_PRIVATE
                                | ClassFileWriter.ACC_FINAL));

        // Call NativeFunction.initScriptFunction
        cfw.addLoadThis();
        cfw.addPush(compilerEnv.getLanguageVersion());
        cfw.addPush(ofn.fnode.getFunctionName());
        pushParamNamesArray(cfw, ofn.fnode);
        cfw.addPush(ofn.fnode.getParamCount());
        cfw.addInvoke(ByteCode.INVOKEVIRTUAL,
                      "org/mozilla/javascript/NativeFunction",
                      "initScriptFunction",
                      "(ILjava/lang/String;[Ljava/lang/String;I)V");

        cfw.addLoadThis();
        cfw.addALoad(SCOPE_ARG);
        cfw.addInvoke(ByteCode.INVOKEVIRTUAL,
                      "org/mozilla/javascript/ScriptableObject",
                      "setParentScope",
                      "(Lorg/mozilla/javascript/Scriptable;)V");

        // precompile all regexp literals
        int regexpCount = ofn.fnode.getRegexpCount();
        if (regexpCount != 0) {
            cfw.addLoadThis();
            pushRegExpArray(cfw, ofn.fnode, CONTEXT_ARG, SCOPE_ARG);
            cfw.add(ByteCode.PUTFIELD, mainClassName,
                    REGEXP_ARRAY_FIELD_NAME, REGEXP_ARRAY_FIELD_TYPE);
        }

        cfw.add(ByteCode.RETURN);
        // 3 = (scriptThis/functionRef) + scope + context
        cfw.stopMethod((short)3);
    }

    private void generateGetEncodedSource(ClassFileWriter cfw,
                                          String encodedSource)
    {
        // Override NativeFunction.getEncodedSourceg() with
        // public String getEncodedSource()
        // {
        //     int start, end;
        //     switch (id) {
        //       case 1: start, end = embedded_constants_for_function_1;
        //       case 2: start, end = embedded_constants_for_function_2;
        //       ...
        //       default: start, end = embedded_constants_for_function_0;
        //     }
        //     return ENCODED.substring(start, end);
        // }
        cfw.startMethod("getEncodedSource", "()Ljava/lang/String;",
                        ClassFileWriter.ACC_PUBLIC);

        cfw.addPush(encodedSource);

        int count = scriptOrFnNodes.length;
        if (count == 1) {
            // do not generate switch in this case
            ScriptOrFnNode n = scriptOrFnNodes[0];
            cfw.addPush(n.getEncodedSourceStart());
            cfw.addPush(n.getEncodedSourceEnd());
        } else {
            cfw.addLoadThis();
            cfw.add(ByteCode.GETFIELD, cfw.getClassName(), ID_FIELD_NAME, "I");

            // do switch from 1 .. count - 1 mapping 0 to the default case
            int switchStart = cfw.addTableSwitch(1, count - 1);
            int afterSwitch = cfw.acquireLabel();
            int switchStackTop = 0;
            for (int i = 0; i != count; ++i) {
                ScriptOrFnNode n = scriptOrFnNodes[i];
                if (i == 0) {
                    cfw.markTableSwitchDefault(switchStart);
                    switchStackTop = cfw.getStackTop();
                } else {
                    cfw.markTableSwitchCase(switchStart, i - 1,
                                            switchStackTop);
                }
                cfw.addPush(n.getEncodedSourceStart());
                cfw.addPush(n.getEncodedSourceEnd());
                // Add goto past switch code unless the last statement
                if (i + 1 != count) {
                    cfw.add(ByteCode.GOTO, afterSwitch);
                }
            }
            cfw.markLabel(afterSwitch);
        }

        cfw.addInvoke(ByteCode.INVOKEVIRTUAL,
                      "java/lang/String",
                      "substring",
                      "(II)Ljava/lang/String;");
        cfw.add(ByteCode.ARETURN);

        // 1: this and no argument or locals
        cfw.stopMethod((short)1);
    }

    private void emitRegExpInit(ClassFileWriter cfw)
    {
        // precompile all regexp literals

        int totalRegCount = 0;
        for (int i = 0; i != scriptOrFnNodes.length; ++i) {
            totalRegCount += scriptOrFnNodes[i].getRegexpCount();
        }
        if (totalRegCount == 0) {
            return;
        }

        cfw.startMethod(REGEXP_INIT_METHOD_NAME, REGEXP_INIT_METHOD_SIGNATURE,
            (short)(ClassFileWriter.ACC_STATIC | ClassFileWriter.ACC_PRIVATE
                    | ClassFileWriter.ACC_SYNCHRONIZED));
        cfw.addField("_reInitDone", "Z",
                     (short)(ClassFileWriter.ACC_STATIC
                             | ClassFileWriter.ACC_PRIVATE));
        cfw.add(ByteCode.GETSTATIC, mainClassName, "_reInitDone", "Z");
        int doInit = cfw.acquireLabel();
        cfw.add(ByteCode.IFEQ, doInit);
        cfw.add(ByteCode.RETURN);
        cfw.markLabel(doInit);

        for (int i = 0; i != scriptOrFnNodes.length; ++i) {
            ScriptOrFnNode n = scriptOrFnNodes[i];
            int regCount = n.getRegexpCount();
            for (int j = 0; j != regCount; ++j) {
                String reFieldName = getCompiledRegexpName(n, j);
                String reFieldType = "Ljava/lang/Object;";
                String reString = n.getRegexpString(j);
                String reFlags = n.getRegexpFlags(j);
                cfw.addField(reFieldName, reFieldType,
                             (short)(ClassFileWriter.ACC_STATIC
                                     | ClassFileWriter.ACC_PRIVATE));
                cfw.addALoad(0); // proxy
                cfw.addALoad(1); // context
                cfw.addPush(reString);
                if (reFlags == null) {
                    cfw.add(ByteCode.ACONST_NULL);
                } else {
                    cfw.addPush(reFlags);
                }
                cfw.addInvoke(ByteCode.INVOKEINTERFACE,
                              "org/mozilla/javascript/RegExpProxy",
                              "compileRegExp",
                              "(Lorg/mozilla/javascript/Context;"
                              +"Ljava/lang/String;Ljava/lang/String;"
                              +")Ljava/lang/Object;");
                cfw.add(ByteCode.PUTSTATIC, mainClassName,
                        reFieldName, reFieldType);
            }
        }

        cfw.addPush(1);
        cfw.add(ByteCode.PUTSTATIC, mainClassName, "_reInitDone", "Z");
        cfw.add(ByteCode.RETURN);
        cfw.stopMethod((short)2);
    }

    private void emitConstantDudeInitializers(ClassFileWriter cfw)
    {
        int N = itsConstantListSize;
        if (N == 0)
            return;

        cfw.startMethod("<clinit>", "()V",
            (short)(ClassFileWriter.ACC_STATIC | ClassFileWriter.ACC_FINAL));

        double[] array = itsConstantList;
        for (int i = 0; i != N; ++i) {
            double num = array[i];
            String constantName = "_k" + i;
            String constantType = getStaticConstantWrapperType(num);
            cfw.addField(constantName, constantType,
                         (short)(ClassFileWriter.ACC_STATIC
                                 | ClassFileWriter.ACC_PRIVATE));
            int inum = (int)num;
            if (inum == num) {
                cfw.add(ByteCode.NEW, "java/lang/Integer");
                cfw.add(ByteCode.DUP);
                cfw.addPush(inum);
                cfw.addInvoke(ByteCode.INVOKESPECIAL, "java/lang/Integer",
                              "<init>", "(I)V");
            } else {
                cfw.addPush(num);
                addDoubleWrap(cfw);
            }
            cfw.add(ByteCode.PUTSTATIC, mainClassName,
                    constantName, constantType);
        }

        cfw.add(ByteCode.RETURN);
        cfw.stopMethod((short)0);
    }

    private static void pushParamNamesArray(ClassFileWriter cfw,
                                            ScriptOrFnNode n)
    {
        // Push string array with the names of the parameters and the vars.
        int paramAndVarCount = n.getParamAndVarCount();
        if (paramAndVarCount == 0) {
            cfw.add(ByteCode.GETSTATIC,
                    "org/mozilla/javascript/ScriptRuntime",
                    "emptyStrings", "[Ljava/lang/String;");
        } else {
            cfw.addPush(paramAndVarCount);
            cfw.add(ByteCode.ANEWARRAY, "java/lang/String");
            for (int i = 0; i != paramAndVarCount; ++i) {
                cfw.add(ByteCode.DUP);
                cfw.addPush(i);
                cfw.addPush(n.getParamOrVarName(i));
                cfw.add(ByteCode.AASTORE);
            }
        }
    }

    void pushRegExpArray(ClassFileWriter cfw, ScriptOrFnNode n,
                         int contextArg, int scopeArg)
    {
        int regexpCount = n.getRegexpCount();
        if (regexpCount == 0) throw badTree();

        cfw.addPush(regexpCount);
        cfw.add(ByteCode.ANEWARRAY, "java/lang/Object");

        cfw.addALoad(contextArg);
        cfw.addInvoke(ByteCode.INVOKESTATIC,
                      "org/mozilla/javascript/ScriptRuntime",
                      "checkRegExpProxy",
                      "(Lorg/mozilla/javascript/Context;"
                      +")Lorg/mozilla/javascript/RegExpProxy;");
        // Stack: proxy, array
        cfw.add(ByteCode.DUP);
        cfw.addALoad(contextArg);
        cfw.addInvoke(ByteCode.INVOKESTATIC, mainClassName,
                      REGEXP_INIT_METHOD_NAME, REGEXP_INIT_METHOD_SIGNATURE);
        for (int i = 0; i != regexpCount; ++i) {
            // Stack: proxy, array
            cfw.add(ByteCode.DUP2);
            cfw.addALoad(contextArg);
            cfw.addALoad(scopeArg);
            cfw.add(ByteCode.GETSTATIC, mainClassName,
                    getCompiledRegexpName(n, i), "Ljava/lang/Object;");
            // Stack: compiledRegExp, scope, cx, proxy, array, proxy, array
            cfw.addInvoke(ByteCode.INVOKEINTERFACE,
                          "org/mozilla/javascript/RegExpProxy",
                          "wrapRegExp",
                          "(Lorg/mozilla/javascript/Context;"
                          +"Lorg/mozilla/javascript/Scriptable;"
                          +"Ljava/lang/Object;"
                          +")Lorg/mozilla/javascript/Scriptable;");
            // Stack: wrappedRegExp, array, proxy, array
            cfw.addPush(i);
            cfw.add(ByteCode.SWAP);
            cfw.add(ByteCode.AASTORE);
            // Stack: proxy, array
        }
        // remove proxy
        cfw.add(ByteCode.POP);
    }

    void pushNumberAsObject(ClassFileWriter cfw, double num)
    {
        if (num == 0.0) {
            if (1 / num > 0) {
                // +0.0
                cfw.add(ByteCode.GETSTATIC,
                        "org/mozilla/javascript/optimizer/OptRuntime",
                        "zeroObj", "Ljava/lang/Double;");
            } else {
                cfw.addPush(num);
                addDoubleWrap(cfw);
            }

        } else if (num == 1.0) {
            cfw.add(ByteCode.GETSTATIC,
                    "org/mozilla/javascript/optimizer/OptRuntime",
                    "oneObj", "Ljava/lang/Double;");
            return;

        } else if (num == -1.0) {
            cfw.add(ByteCode.GETSTATIC,
                    "org/mozilla/javascript/optimizer/OptRuntime",
                    "minusOneObj", "Ljava/lang/Double;");

        } else if (num != num) {
            cfw.add(ByteCode.GETSTATIC,
                    "org/mozilla/javascript/ScriptRuntime",
                    "NaNobj", "Ljava/lang/Double;");

        } else if (itsConstantListSize >= 2000) {
            // There appears to be a limit in the JVM on either the number
            // of static fields in a class or the size of the class
            // initializer. Either way, we can't have any more than 2000
            // statically init'd constants.
            cfw.addPush(num);
            addDoubleWrap(cfw);

        } else {
            int N = itsConstantListSize;
            int index = 0;
            if (N == 0) {
                itsConstantList = new double[64];
            } else {
                double[] array = itsConstantList;
                while (index != N && array[index] != num) {
                    ++index;
                }
                if (N == array.length) {
                    array = new double[N * 2];
                    System.arraycopy(itsConstantList, 0, array, 0, N);
                    itsConstantList = array;
                }
            }
            if (index == N) {
                itsConstantList[N] = num;
                itsConstantListSize = N + 1;
            }
            String constantName = "_k" + index;
            String constantType = getStaticConstantWrapperType(num);
            cfw.add(ByteCode.GETSTATIC, mainClassName,
                    constantName, constantType);
        }
    }

    private static void addDoubleWrap(ClassFileWriter cfw)
    {
        cfw.addInvoke(ByteCode.INVOKESTATIC,
                      "org/mozilla/javascript/optimizer/OptRuntime",
                      "wrapDouble", "(D)Ljava/lang/Double;");
    }

    private static String getStaticConstantWrapperType(double num)
    {
        String constantType;
        int inum = (int)num;
        if (inum == num) {
            return "Ljava/lang/Integer;";
        } else {
            return "Ljava/lang/Double;";
        }
    }
    static void pushUndefined(ClassFileWriter cfw)
    {
        cfw.add(ByteCode.GETSTATIC, "org/mozilla/javascript/Undefined",
                "instance", "Lorg/mozilla/javascript/Scriptable;");
    }

    int getIndex(ScriptOrFnNode n)
    {
        return scriptOrFnIndexes.getExisting(n);
    }

    static String getDirectTargetFieldName(int i)
    {
        return "_dt" + i;
    }

    String getDirectCtorName(ScriptOrFnNode n)
    {
        return "_n"+getIndex(n);
    }

    String getBodyMethodName(ScriptOrFnNode n)
    {
        return "_c"+getIndex(n);
    }

    String getBodyMethodSignature(ScriptOrFnNode n)
    {
        StringBuffer sb = new StringBuffer();
        sb.append('(');
        sb.append(mainClassSignature);
        sb.append("Lorg/mozilla/javascript/Context;"
                  +"Lorg/mozilla/javascript/Scriptable;"
                  +"Lorg/mozilla/javascript/Scriptable;");
        if (n.getType() == Token.FUNCTION) {
            OptFunctionNode ofn = OptFunctionNode.get(n);
            if (ofn.isTargetOfDirectCall()) {
                int pCount = ofn.fnode.getParamCount();
                for (int i = 0; i != pCount; i++) {
                    sb.append("Ljava/lang/Object;D");
                }
            }
        }
        sb.append("[Ljava/lang/Object;)Ljava/lang/Object;");
        return sb.toString();
    }

    String getFunctionInitMethodName(OptFunctionNode ofn)
    {
        return "_i"+getIndex(ofn.fnode);
    }

    String getCompiledRegexpName(ScriptOrFnNode n, int regexpIndex)
    {
        return "_re"+getIndex(n)+"_"+regexpIndex;
    }

    static RuntimeException badTree()
    {
        throw new RuntimeException("Bad tree in codegen");
    }

    private static final String SUPER_CLASS_NAME
        = "org.mozilla.javascript.NativeFunction";

    static final String DIRECT_CALL_PARENT_FIELD = "_dcp";

    private static final String ID_FIELD_NAME = "_id";

    private static final String REGEXP_INIT_METHOD_NAME = "_reInit";
    private static final String REGEXP_INIT_METHOD_SIGNATURE
        =  "(Lorg/mozilla/javascript/RegExpProxy;"
           +"Lorg/mozilla/javascript/Context;"
           +")V";
    static final String REGEXP_ARRAY_FIELD_NAME = "_re";
    static final String REGEXP_ARRAY_FIELD_TYPE = "[Ljava/lang/Object;";

    static final String FUNCTION_INIT_SIGNATURE
        =  "(Lorg/mozilla/javascript/Context;"
           +"Lorg/mozilla/javascript/Scriptable;"
           +")V";

   static final String FUNCTION_CONSTRUCTOR_SIGNATURE
        = "(Lorg/mozilla/javascript/Scriptable;"
          +"Lorg/mozilla/javascript/Context;I)V";

    private static final Object globalLock = new Object();
    private static int globalSerialClassCounter;

    private CompilerEnvirons compilerEnv;

    private ObjArray directCallTargets;
    ScriptOrFnNode[] scriptOrFnNodes;
    private ObjToIntMap scriptOrFnIndexes;

    String mainClassName;
    String mainClassSignature;

    boolean itsUseDynamicScope;
    int languageVersion;

    private double[] itsConstantList;
    private int itsConstantListSize;
}


class BodyCodegen
{
    void generateBodyCode()
    {
        initBodyGeneration();

        cfw.startMethod(codegen.getBodyMethodName(scriptOrFn),
                        codegen.getBodyMethodSignature(scriptOrFn),
                        (short)(ClassFileWriter.ACC_STATIC
                                | ClassFileWriter.ACC_PRIVATE));

        generatePrologue();

        Node treeTop;
        if (fnCurrent != null) {
            treeTop = scriptOrFn.getLastChild();
        } else {
            treeTop = scriptOrFn;
        }
        generateCodeFromNode(treeTop, null);

        generateEpilogue();

        cfw.stopMethod((short)(localsMax + 1));
    }

    private void initBodyGeneration()
    {
        isTopLevel = (scriptOrFn == codegen.scriptOrFnNodes[0]);

        if (scriptOrFn.getType() == Token.FUNCTION) {
            fnCurrent = OptFunctionNode.get(scriptOrFn);
            hasVarsInRegs = !fnCurrent.fnode.requiresActivation();
            inDirectCallFunction = fnCurrent.isTargetOfDirectCall();
            if (inDirectCallFunction && !hasVarsInRegs) Codegen.badTree();
        } else {
            fnCurrent = null;
            hasVarsInRegs = false;
            inDirectCallFunction = false;
        }

        locals = new boolean[MAX_LOCALS];

        funObjLocal = 0;
        contextLocal = 1;
        variableObjectLocal = 2;
        thisObjLocal = 3;
        localsMax = (short) 4;  // number of parms + "this"
        firstFreeLocal = 4;

        popvLocal = -1;
        argsLocal = -1;
        itsZeroArgArray = -1;
        itsOneArgArray = -1;
        scriptRegexpLocal = -1;
        epilogueLabel = -1;
    }

    /**
     * Generate the prologue for a function or script.
     */
    private void generatePrologue()
    {
        if (inDirectCallFunction) {
            int directParameterCount = scriptOrFn.getParamCount();
            // 0 is reserved for function Object 'this'
            // 1 is reserved for context
            // 2 is reserved for parentScope
            // 3 is reserved for script 'this'
            if (firstFreeLocal != 4) Kit.codeBug();
            for (int i = 0; i != directParameterCount; ++i) {
                OptLocalVariable lVar = fnCurrent.getVar(i);
                lVar.assignJRegister(firstFreeLocal);
                // 3 is 1 for Object parm and 2 for double parm
                firstFreeLocal += 3;
            }
            if (!fnCurrent.getParameterNumberContext()) {
                // make sure that all parameters are objects
                itsForcedObjectParameters = true;
                for (int i = 0; i != directParameterCount; ++i) {
                    OptLocalVariable lVar = fnCurrent.getVar(i);
                    short reg = lVar.getJRegister();
                    cfw.addALoad(reg);
                    cfw.add(ByteCode.GETSTATIC,
                            "java/lang/Void",
                            "TYPE",
                            "Ljava/lang/Class;");
                    int isObjectLabel = cfw.acquireLabel();
                    cfw.add(ByteCode.IF_ACMPNE, isObjectLabel);
                    cfw.addDLoad(reg + 1);
                    addDoubleWrap();
                    cfw.addAStore(reg);
                    cfw.markLabel(isObjectLabel);
                }
            }
        }

        if (fnCurrent != null && !inDirectCallFunction
            && (!compilerEnv.isUseDynamicScope()
                || fnCurrent.fnode.getIgnoreDynamicScope()))
        {
            // Unless we're either in a direct call or using dynamic scope,
            // use the enclosing scope of the function as our variable object.
            cfw.addALoad(funObjLocal);
            cfw.addInvoke(ByteCode.INVOKEINTERFACE,
                          "org/mozilla/javascript/Scriptable",
                          "getParentScope",
                          "()Lorg/mozilla/javascript/Scriptable;");
            cfw.addAStore(variableObjectLocal);
        }

        // reserve 'args[]'
        argsLocal = firstFreeLocal++;
        localsMax = firstFreeLocal;

        if (fnCurrent == null) {
            // See comments in visitRegexp
            if (scriptOrFn.getRegexpCount() != 0) {
                scriptRegexpLocal = getNewWordLocal();
                codegen.pushRegExpArray(cfw, scriptOrFn, contextLocal,
                                        variableObjectLocal);
                cfw.addAStore(scriptRegexpLocal);
            }
        }

        if (fnCurrent != null && fnCurrent.fnode.getCheckThis()) {
            // Nested functions must check their 'this' value to
            //  insure it is not an activation object:
            //  see 10.1.6 Activation Object
            cfw.addALoad(thisObjLocal);
            addScriptRuntimeInvoke("getThis",
                                   "(Lorg/mozilla/javascript/Scriptable;"
                                   +")Lorg/mozilla/javascript/Scriptable;");
            cfw.addAStore(thisObjLocal);
        }

        if (hasVarsInRegs) {
            // No need to create activation. Pad arguments if need be.
            int parmCount = scriptOrFn.getParamCount();
            if (parmCount > 0 && !inDirectCallFunction) {
                // Set up args array
                // check length of arguments, pad if need be
                cfw.addALoad(argsLocal);
                cfw.add(ByteCode.ARRAYLENGTH);
                cfw.addPush(parmCount);
                int label = cfw.acquireLabel();
                cfw.add(ByteCode.IF_ICMPGE, label);
                cfw.addALoad(argsLocal);
                cfw.addPush(parmCount);
                addScriptRuntimeInvoke("padArguments",
                                       "([Ljava/lang/Object;I"
                                       +")[Ljava/lang/Object;");
                cfw.addAStore(argsLocal);
                cfw.markLabel(label);
            }

            int paramCount = fnCurrent.fnode.getParamCount();
            int varCount = fnCurrent.fnode.getParamAndVarCount();

            // REMIND - only need to initialize the vars that don't get a value
            // before the next call and are used in the function
            short firstUndefVar = -1;
            for (int i = 0; i != varCount; ++i) {
                OptLocalVariable lVar = fnCurrent.getVar(i);
                short reg = -1;
                if (i < paramCount) {
                    if (!inDirectCallFunction) {
                        reg = getNewWordLocal();
                        cfw.addALoad(argsLocal);
                        cfw.addPush(i);
                        cfw.add(ByteCode.AALOAD);
                        cfw.addAStore(reg);
                    }
                } else if (lVar.isNumber()) {
                    reg = getNewWordPairLocal();
                    cfw.addPush(0.0);
                    cfw.addDStore(reg);
                } else {
                    reg = getNewWordLocal();
                    if (firstUndefVar == -1) {
                        Codegen.pushUndefined(cfw);
                        firstUndefVar = reg;
                    } else {
                        cfw.addALoad(firstUndefVar);
                    }
                    cfw.addAStore(reg);
                }
                if (reg >= 0) {
                    lVar.assignJRegister(reg);
                }

                // Add debug table enry if we're generating debug info
                if (compilerEnv.isGenerateDebugInfo()) {
                    String name = fnCurrent.fnode.getParamOrVarName(i);
                    String type = lVar.isNumber() ? "D" : "Ljava/lang/Object;";
                    int startPC = cfw.getCurrentCodeOffset();
                    if (reg < 0) {
                        reg = lVar.getJRegister();
                    }
                    cfw.addVariableDescriptor(name, type, startPC, reg);
                }
            }

            // Skip creating activation object.
            return;
        }

        String debugVariableName;
        if (fnCurrent != null) {
            cfw.addALoad(contextLocal);
            cfw.addALoad(variableObjectLocal);
            cfw.addALoad(funObjLocal);
            cfw.addALoad(thisObjLocal);
            cfw.addALoad(argsLocal);
            addScriptRuntimeInvoke("initVarObj",
                                   "(Lorg/mozilla/javascript/Context;"
                                   +"Lorg/mozilla/javascript/Scriptable;"
                                   +"Lorg/mozilla/javascript/NativeFunction;"
                                   +"Lorg/mozilla/javascript/Scriptable;"
                                   +"[Ljava/lang/Object;"
                                   +")Lorg/mozilla/javascript/Scriptable;");
            cfw.addAStore(variableObjectLocal);
            debugVariableName = "activation";
        } else {
            cfw.addALoad(contextLocal);
            cfw.addALoad(variableObjectLocal);
            cfw.addALoad(funObjLocal);
            cfw.addALoad(thisObjLocal);
            cfw.addPush(0);
            addScriptRuntimeInvoke("initScript",
                                   "(Lorg/mozilla/javascript/Context;"
                                   +"Lorg/mozilla/javascript/Scriptable;"
                                   +"Lorg/mozilla/javascript/NativeFunction;"
                                   +"Lorg/mozilla/javascript/Scriptable;"
                                   +"Z"
                                   +")V");
            debugVariableName = "global";
        }

        int functionCount = scriptOrFn.getFunctionCount();
        for (int i = 0; i != functionCount; i++) {
            OptFunctionNode ofn = OptFunctionNode.get(scriptOrFn, i);
            if (ofn.fnode.getFunctionType()
                    == FunctionNode.FUNCTION_STATEMENT)
            {
                visitFunction(ofn, FunctionNode.FUNCTION_STATEMENT);
            }
        }

        // default is to generate debug info
        if (compilerEnv.isGenerateDebugInfo()) {
            cfw.addVariableDescriptor(debugVariableName, "Lorg/mozilla/javascript/Scriptable;", cfw.getCurrentCodeOffset(), variableObjectLocal);
        }

        if (fnCurrent == null) {
            // OPT: use dataflow to prove that this assignment is dead
            popvLocal = getNewWordLocal();
            Codegen.pushUndefined(cfw);
            cfw.addAStore(popvLocal);

            int linenum = scriptOrFn.getEndLineno();
            if (linenum != -1)
              cfw.addLineNumberEntry((short)linenum);

        } else {
            if (fnCurrent.itsContainsCalls0) {
                itsZeroArgArray = getNewWordLocal();
                cfw.add(ByteCode.GETSTATIC,
                        "org/mozilla/javascript/ScriptRuntime",
                        "emptyArgs", "[Ljava/lang/Object;");
                cfw.addAStore(itsZeroArgArray);
            }
            if (fnCurrent.itsContainsCalls1) {
                itsOneArgArray = getNewWordLocal();
                cfw.addPush(1);
                cfw.add(ByteCode.ANEWARRAY, "java/lang/Object");
                cfw.addAStore(itsOneArgArray);
            }
        }

    }

    private void generateEpilogue() {
        if (epilogueLabel != -1) {
            cfw.markLabel(epilogueLabel);
        }
        if (fnCurrent == null || !hasVarsInRegs) {
            // restore caller's activation
            cfw.addALoad(contextLocal);
            addScriptRuntimeInvoke("popActivation",
                                   "(Lorg/mozilla/javascript/Context;)V");
            if (fnCurrent == null) {
                cfw.addALoad(popvLocal);
            }
        }
        cfw.add(ByteCode.ARETURN);
    }

    private void generateCodeFromNode(Node node, Node parent)
    {
        // System.out.println("gen code for " + node.toString());

        int type = node.getType();
        Node child = node.getFirstChild();
        switch (type) {
              case Token.LOOP:
              case Token.LABEL:
                visitStatement(node);
                while (child != null) {
                    generateCodeFromNode(child, node);
                    child = child.getNext();
                }
                break;

              case Token.WITH:
                ++withNesting;
                visitStatement(node);
                while (child != null) {
                    generateCodeFromNode(child, node);
                    child = child.getNext();
                }
                --withNesting;
                break;

              case Token.CASE:
              case Token.DEFAULT:
                // XXX shouldn't these be StatementNodes?

              case Token.SCRIPT:
              case Token.BLOCK:
              case Token.EMPTY:
                // no-ops.
                visitStatement(node);
                while (child != null) {
                    generateCodeFromNode(child, node);
                    child = child.getNext();
                }
                break;

              case Token.LOCAL_BLOCK: {
                visitStatement(node);
                int local = getNewWordLocal();
                node.putIntProp(Node.LOCAL_PROP, local);
                while (child != null) {
                    generateCodeFromNode(child, node);
                    child = child.getNext();
                }
                releaseWordLocal((short)local);
                node.removeProp(Node.LOCAL_PROP);
                break;
              }

              case Token.USE_STACK:
                break;

              case Token.FUNCTION:
                if (fnCurrent != null || parent.getType() != Token.SCRIPT) {
                    int fnIndex = node.getExistingIntProp(Node.FUNCTION_PROP);
                    OptFunctionNode ofn = OptFunctionNode.get(scriptOrFn,
                                                             fnIndex);
                    int t = ofn.fnode.getFunctionType();
                    if (t != FunctionNode.FUNCTION_STATEMENT) {
                        visitFunction(ofn, t);
                    }
                }
                break;

              case Token.NAME:
                visitName(node);
                break;

              case Token.CALL:
              case Token.NEW:
                visitCall(node, type, child);
                break;

              case Token.REF_CALL:
                visitRefCall(node, type, child);
                break;

              case Token.NUMBER:
                visitNumber(node);
                break;

              case Token.STRING:
                cfw.addPush(node.getString());
                break;

              case Token.THIS:
                cfw.addALoad(thisObjLocal);
                break;

              case Token.THISFN:
                cfw.add(ByteCode.ALOAD_0);
                break;

              case Token.NULL:
                cfw.add(ByteCode.ACONST_NULL);
                break;

              case Token.TRUE:
                cfw.add(ByteCode.GETSTATIC, "java/lang/Boolean",
                        "TRUE", "Ljava/lang/Boolean;");
                break;

              case Token.FALSE:
                cfw.add(ByteCode.GETSTATIC, "java/lang/Boolean",
                        "FALSE", "Ljava/lang/Boolean;");
                break;

              case Token.UNDEFINED:
                Codegen.pushUndefined(cfw);
                break;

              case Token.REGEXP:
                visitRegexp(node);
                break;

              case Token.TRY:
                visitTryCatchFinally((Node.Jump)node, child);
                break;

              case Token.THROW:
                visitThrow(node, child);
                break;

              case Token.RETURN_POPV:
                if (fnCurrent == null) throw Codegen.badTree();
                // fallthrough
              case Token.RETURN:
                visitStatement(node);
                if (child != null) {
                    do {
                        generateCodeFromNode(child, node);
                        child = child.getNext();
                    } while (child != null);
                } else if (fnCurrent != null && type == Token.RETURN) {
                    Codegen.pushUndefined(cfw);
                } else {
                    if (popvLocal < 0) throw Codegen.badTree();
                    cfw.addALoad(popvLocal);
                }
                if (epilogueLabel == -1)
                    epilogueLabel = cfw.acquireLabel();
                cfw.add(ByteCode.GOTO, epilogueLabel);
                break;

              case Token.SWITCH:
                visitSwitch((Node.Jump)node, child);
                break;

              case Token.COMMA: {
                Node next = child.getNext();
                while (next != null) {
                    generateCodeFromNode(child, node);
                    cfw.add(ByteCode.POP);
                    child = next;
                    next = next.getNext();
                }
                generateCodeFromNode(child, node);
                break;
              }

              case Token.CATCH_SCOPE:
                cfw.addPush(node.getString());
                generateCodeFromNode(child, node);
                addScriptRuntimeInvoke("newCatchScope",
                                       "(Ljava/lang/String;Ljava/lang/Object;"
                                       +")Lorg/mozilla/javascript/Scriptable;");
                break;

              case Token.ENTERWITH:
                visitEnterWith(node, child);
                break;

              case Token.LEAVEWITH:
                visitLeaveWith(node, child);
                break;

              case Token.ENUM_INIT: {
                generateCodeFromNode(child, node);
                cfw.addALoad(variableObjectLocal);
                addScriptRuntimeInvoke("enumInit",
                                       "(Ljava/lang/Object;"
                                       +"Lorg/mozilla/javascript/Scriptable;"
                                       +")Ljava/lang/Object;");
                int local = getLocalBlockRegister(node);
                cfw.addAStore(local);
                break;
              }

              case Token.ENUM_NEXT:
              case Token.ENUM_ID: {
                int local = getLocalBlockRegister(node);
                cfw.addALoad(local);
                if (type == Token.ENUM_NEXT) {
                    addScriptRuntimeInvoke(
                        "enumNext", "(Ljava/lang/Object;)Ljava/lang/Boolean;");
                } else {
                    addScriptRuntimeInvoke(
                        "enumId", "(Ljava/lang/Object;)Ljava/lang/String;");
                }
                break;
              }

              case Token.POP:
                visitStatement(node);
                if (child.getType() == Token.SETVAR) {
                    /* special case this so as to avoid unnecessary
                    load's & pop's */
                    visitSetVar(child, child.getFirstChild(), false);
                }
                else {
                    while (child != null) {
                        generateCodeFromNode(child, node);
                        child = child.getNext();
                    }
                    if (node.getIntProp(Node.ISNUMBER_PROP, -1) != -1)
                        cfw.add(ByteCode.POP2);
                    else
                        cfw.add(ByteCode.POP);
                }
                break;

              case Token.POPV:
                visitStatement(node);
                generateCodeFromNode(child, node);
                if (popvLocal < 0) {
                    popvLocal = getNewWordLocal();
                }
                cfw.addAStore(popvLocal);
                break;

              case Token.TARGET:
                visitTarget((Node.Target)node);
                break;

              case Token.JSR:
              case Token.GOTO:
              case Token.IFEQ:
              case Token.IFNE:
                visitGOTO((Node.Jump)node, type, child);
                break;

              case Token.FINALLY:
                visitFinally(node, child);
                break;

              case Token.ARRAYLIT:
                visitArrayLiteral(node, child);
                break;

              case Token.OBJECTLIT:
                visitObjectLiteral(node, child);
                break;

              case Token.NOT: {
                int trueTarget = cfw.acquireLabel();
                int falseTarget = cfw.acquireLabel();
                int beyond = cfw.acquireLabel();
                generateIfJump(child, node, trueTarget, falseTarget);

                cfw.markLabel(trueTarget);
                cfw.add(ByteCode.GETSTATIC, "java/lang/Boolean",
                                        "FALSE", "Ljava/lang/Boolean;");
                cfw.add(ByteCode.GOTO, beyond);
                cfw.markLabel(falseTarget);
                cfw.add(ByteCode.GETSTATIC, "java/lang/Boolean",
                                        "TRUE", "Ljava/lang/Boolean;");
                cfw.markLabel(beyond);
                cfw.adjustStackTop(-1);
                break;
              }

              case Token.BITNOT:
                generateCodeFromNode(child, node);
                addScriptRuntimeInvoke("toInt32", "(Ljava/lang/Object;)I");
                cfw.addPush(-1);         // implement ~a as (a ^ -1)
                cfw.add(ByteCode.IXOR);
                cfw.add(ByteCode.I2D);
                addDoubleWrap();
                break;

              case Token.VOID:
                generateCodeFromNode(child, node);
                cfw.add(ByteCode.POP);
                Codegen.pushUndefined(cfw);
                break;

              case Token.TYPEOF:
                generateCodeFromNode(child, node);
                addScriptRuntimeInvoke("typeof",
                                       "(Ljava/lang/Object;"
                                       +")Ljava/lang/String;");
                break;

              case Token.TYPEOFNAME:
                visitTypeofname(node);
                break;

              case Token.INC:
              case Token.DEC:
                visitIncDec(node, false);
                break;

              case Token.OR:
              case Token.AND: {
                    generateCodeFromNode(child, node);
                    cfw.add(ByteCode.DUP);
                    addScriptRuntimeInvoke("toBoolean",
                                           "(Ljava/lang/Object;)Z");
                    int falseTarget = cfw.acquireLabel();
                    if (type == Token.AND)
                        cfw.add(ByteCode.IFEQ, falseTarget);
                    else
                        cfw.add(ByteCode.IFNE, falseTarget);
                    cfw.add(ByteCode.POP);
                    generateCodeFromNode(child.getNext(), node);
                    cfw.markLabel(falseTarget);
                }
                break;

              case Token.HOOK : {
                    Node ifThen = child.getNext();
                    Node ifElse = ifThen.getNext();
                    generateCodeFromNode(child, node);
                    addScriptRuntimeInvoke("toBoolean",
                                           "(Ljava/lang/Object;)Z");
                    int elseTarget = cfw.acquireLabel();
                    cfw.add(ByteCode.IFEQ, elseTarget);
                    short stack = cfw.getStackTop();
                    generateCodeFromNode(ifThen, node);
                    int afterHook = cfw.acquireLabel();
                    cfw.add(ByteCode.GOTO, afterHook);
                    cfw.markLabel(elseTarget, stack);
                    generateCodeFromNode(ifElse, node);
                    cfw.markLabel(afterHook);
                }
                break;

              case Token.ADD: {
                    generateCodeFromNode(child, node);
                    generateCodeFromNode(child.getNext(), node);
                    switch (node.getIntProp(Node.ISNUMBER_PROP, -1)) {
                        case Node.BOTH:
                            cfw.add(ByteCode.DADD);
                            break;
                        case Node.LEFT:
                            addOptRuntimeInvoke("add",
                                "(DLjava/lang/Object;)Ljava/lang/Object;");
                            break;
                        case Node.RIGHT:
                            addOptRuntimeInvoke("add",
                                "(Ljava/lang/Object;D)Ljava/lang/Object;");
                            break;
                        default:
                        addScriptRuntimeInvoke("add",
                                               "(Ljava/lang/Object;"
                                               +"Ljava/lang/Object;"
                                               +")Ljava/lang/Object;");
                    }
                }
                break;

              case Token.MUL:
                visitArithmetic(node, ByteCode.DMUL, child, parent);
                break;

              case Token.SUB:
                visitArithmetic(node, ByteCode.DSUB, child, parent);
                break;

              case Token.DIV:
              case Token.MOD:
                visitArithmetic(node, type == Token.DIV
                                      ? ByteCode.DDIV
                                      : ByteCode.DREM, child, parent);
                break;

              case Token.BITOR:
              case Token.BITXOR:
              case Token.BITAND:
              case Token.LSH:
              case Token.RSH:
              case Token.URSH:
                visitBitOp(node, type, child);
                break;

              case Token.POS:
              case Token.NEG:
                generateCodeFromNode(child, node);
                addObjectToDouble();
                if (type == Token.NEG) {
                    cfw.add(ByteCode.DNEG);
                }
                addDoubleWrap();
                break;

              case Optimizer.TO_DOUBLE:
                // cnvt to double (not Double)
                generateCodeFromNode(child, node);
                addObjectToDouble();
                break;

              case Optimizer.TO_OBJECT: {
                // convert from double
                int prop = -1;
                if (child.getType() == Token.NUMBER) {
                    prop = child.getIntProp(Node.ISNUMBER_PROP, -1);
                }
                if (prop != -1) {
                    child.removeProp(Node.ISNUMBER_PROP);
                    generateCodeFromNode(child, node);
                    child.putIntProp(Node.ISNUMBER_PROP, prop);
                } else {
                    generateCodeFromNode(child, node);
                    addDoubleWrap();
                }
                break;
              }

              case Token.IN:
              case Token.INSTANCEOF:
              case Token.LE:
              case Token.LT:
              case Token.GE:
              case Token.GT: {
                int trueGOTO = cfw.acquireLabel();
                int falseGOTO = cfw.acquireLabel();
                visitIfJumpRelOp(node, child, trueGOTO, falseGOTO);
                addJumpedBooleanWrap(trueGOTO, falseGOTO);
                break;
              }

              case Token.EQ:
              case Token.NE:
              case Token.SHEQ:
              case Token.SHNE: {
                int trueGOTO = cfw.acquireLabel();
                int falseGOTO = cfw.acquireLabel();
                visitIfJumpEqOp(node, child, trueGOTO, falseGOTO);
                addJumpedBooleanWrap(trueGOTO, falseGOTO);
                break;
              }

              case Token.GETPROP:
                visitGetProp(node, child, false);
                break;

              case Token.GETELEM:
                visitGetElem(node, child, false);
                break;

              case Token.GET_REF:
                visitGetRef(node, child);
                break;

              case Token.GETVAR:
                visitGetVar(node);
                break;

              case Token.SETVAR:
                visitSetVar(node, child, true);
                break;

              case Token.SETNAME:
                visitSetName(node, child);
                break;

              case Token.SETPROP:
              case Token.SETPROP_OP:
                visitSetProp(type, node, child);
                break;

              case Token.SETELEM:
              case Token.SETELEM_OP:
                visitSetElem(type, node, child);
                break;

              case Token.SET_REF:
              case Token.SET_REF_OP:
                visitSetRef(type, node, child);
                break;

              case Token.DELPROP:
                cfw.addALoad(contextLocal);
                cfw.addALoad(variableObjectLocal);
                while (child != null) {
                    generateCodeFromNode(child, node);
                    child = child.getNext();
                }
                addScriptRuntimeInvoke("delete",
                                       "(Lorg/mozilla/javascript/Context;"
                                       +"Lorg/mozilla/javascript/Scriptable;"
                                       +"Ljava/lang/Object;"
                                       +"Ljava/lang/Object;"
                                       +")Ljava/lang/Object;");
                break;

              case Token.BINDNAME:
                visitBind(node, child);
                break;

              case Token.LOCAL_LOAD:
                cfw.addALoad(getLocalBlockRegister(node));
                break;

              case Token.SPECIAL_REF:
                visitSpecialRef(node, child);
                break;

              default:
                throw new RuntimeException("Unexpected node type "+type);
        }

    }

    private void generateIfJump(Node node, Node parent,
                                int trueLabel, int falseLabel)
    {
        // System.out.println("gen code for " + node.toString());

        int type = node.getType();
        Node child = node.getFirstChild();

        switch (type) {
          case Token.NOT:
            generateIfJump(child, node, falseLabel, trueLabel);
            break;

          case Token.OR:
          case Token.AND: {
            int interLabel = cfw.acquireLabel();
            if (type == Token.AND) {
                generateIfJump(child, node, interLabel, falseLabel);
            }
            else {
                generateIfJump(child, node, trueLabel, interLabel);
            }
            cfw.markLabel(interLabel);
            child = child.getNext();
            generateIfJump(child, node, trueLabel, falseLabel);
            break;
          }

          case Token.IN:
          case Token.INSTANCEOF:
          case Token.LE:
          case Token.LT:
          case Token.GE:
          case Token.GT:
            visitIfJumpRelOp(node, child, trueLabel, falseLabel);
            break;

          case Token.EQ:
          case Token.NE:
          case Token.SHEQ:
          case Token.SHNE:
            visitIfJumpEqOp(node, child, trueLabel, falseLabel);
            break;

          default:
            // Generate generic code for non-optimized jump
            generateCodeFromNode(node, parent);
            addScriptRuntimeInvoke("toBoolean", "(Ljava/lang/Object;)Z");
            cfw.add(ByteCode.IFNE, trueLabel);
            cfw.add(ByteCode.GOTO, falseLabel);
        }
    }

    private void visitFunction(OptFunctionNode ofn, int functionType)
    {
        int fnIndex = codegen.getIndex(ofn.fnode);
        cfw.add(ByteCode.NEW, codegen.mainClassName);
        // Call function constructor
        cfw.add(ByteCode.DUP);
        cfw.addALoad(variableObjectLocal);
        cfw.addALoad(contextLocal);           // load 'cx'
        cfw.addPush(fnIndex);
        cfw.addInvoke(ByteCode.INVOKESPECIAL, codegen.mainClassName,
                      "<init>", Codegen.FUNCTION_CONSTRUCTOR_SIGNATURE);

        // Init mainScript field;
        cfw.add(ByteCode.DUP);
        if (isTopLevel) {
            cfw.add(ByteCode.ALOAD_0);
        } else {
            cfw.add(ByteCode.ALOAD_0);
            cfw.add(ByteCode.GETFIELD,
                    codegen.mainClassName,
                    Codegen.DIRECT_CALL_PARENT_FIELD,
                    codegen.mainClassSignature);
        }
        cfw.add(ByteCode.PUTFIELD,
                codegen.mainClassName,
                Codegen.DIRECT_CALL_PARENT_FIELD,
                codegen.mainClassSignature);

        int directTargetIndex = ofn.getDirectTargetIndex();
        if (directTargetIndex >= 0) {
            cfw.add(ByteCode.DUP);
            if (isTopLevel) {
                cfw.add(ByteCode.ALOAD_0);
            } else {
                cfw.add(ByteCode.ALOAD_0);
                cfw.add(ByteCode.GETFIELD,
                        codegen.mainClassName,
                        Codegen.DIRECT_CALL_PARENT_FIELD,
                        codegen.mainClassSignature);
            }
            cfw.add(ByteCode.SWAP);
            cfw.add(ByteCode.PUTFIELD,
                    codegen.mainClassName,
                    Codegen.getDirectTargetFieldName(directTargetIndex),
                    codegen.mainClassSignature);
        }

        // Dup function reference for function expressions to have it
        // on top of the stack when initFunction returns
        if (functionType != FunctionNode.FUNCTION_STATEMENT) {
            cfw.add(ByteCode.DUP);
        }
        cfw.addPush(functionType);
        cfw.addALoad(variableObjectLocal);
        cfw.addALoad(contextLocal);           // load 'cx'
        addOptRuntimeInvoke("initFunction",
                            "(Lorg/mozilla/javascript/NativeFunction;"
                            +"I"
                            +"Lorg/mozilla/javascript/Scriptable;"
                            +"Lorg/mozilla/javascript/Context;"
                            +")V");
    }

    private void acquireTargetLabel(Node.Target target)
    {
        if (target.labelId == -1) {
            target.labelId = cfw.acquireLabel();
        }
    }

    private void visitTarget(Node.Target target)
    {
        acquireTargetLabel(target);
        cfw.markLabel(target.labelId);
    }

    private void visitGOTO(Node.Jump node, int type, Node child)
    {
        Node.Target target = node.target;
        acquireTargetLabel(target);
        int targetLabel = target.labelId;
        if (type == Token.IFEQ || type == Token.IFNE) {
            if (child == null) throw Codegen.badTree();
            int fallThruLabel = cfw.acquireLabel();
            if (type == Token.IFEQ)
                generateIfJump(child, node, targetLabel, fallThruLabel);
            else
                generateIfJump(child, node, fallThruLabel, targetLabel);
            cfw.markLabel(fallThruLabel);
        }
        else {
            while (child != null) {
                generateCodeFromNode(child, node);
                child = child.getNext();
            }
            if (type == Token.JSR)
                cfw.add(ByteCode.JSR, targetLabel);
            else
                cfw.add(ByteCode.GOTO, targetLabel);
        }
    }

    private void visitFinally(Node node, Node child)
    {
        //Save return address in a new local where
        int finallyRegister = getNewWordLocal();
        cfw.addAStore(finallyRegister);
        while (child != null) {
            generateCodeFromNode(child, node);
            child = child.getNext();
        }
        cfw.add(ByteCode.RET, finallyRegister);
        releaseWordLocal((short)finallyRegister);
    }

    private void visitEnterWith(Node node, Node child)
    {
        while (child != null) {
            generateCodeFromNode(child, node);
            child = child.getNext();
        }
        cfw.addALoad(variableObjectLocal);
        addScriptRuntimeInvoke("enterWith",
                               "(Ljava/lang/Object;"
                               +"Lorg/mozilla/javascript/Scriptable;"
                               +")Lorg/mozilla/javascript/Scriptable;");
        cfw.addAStore(variableObjectLocal);
    }

    private void visitLeaveWith(Node node, Node child)
    {
        cfw.addALoad(variableObjectLocal);
        addScriptRuntimeInvoke("leaveWith",
                               "(Lorg/mozilla/javascript/Scriptable;"
                               +")Lorg/mozilla/javascript/Scriptable;");
        cfw.addAStore(variableObjectLocal);
    }

    private void visitArrayLiteral(Node node, Node child)
    {
        int count = 0;
        for (Node cursor = child; cursor != null; cursor = cursor.getNext()) {
            ++count;
        }
        // load array to store array literal objects
        addNewObjectArray(count);
        for (int i = 0; i != count; ++i) {
            cfw.add(ByteCode.DUP);
            cfw.addPush(i);
            generateCodeFromNode(child, node);
            cfw.add(ByteCode.AASTORE);
            child = child.getNext();
        }
        int[] skipIndexes = (int[])node.getProp(Node.SKIP_INDEXES_PROP);
        if (skipIndexes == null) {
            cfw.add(ByteCode.ACONST_NULL);
            cfw.add(ByteCode.ICONST_0);
        } else {
            cfw.addPush(OptRuntime.encodeIntArray(skipIndexes));
            cfw.addPush(skipIndexes.length);
        }
        cfw.addALoad(contextLocal);
        cfw.addALoad(variableObjectLocal);
        addOptRuntimeInvoke("newArrayLiteral",
             "([Ljava/lang/Object;"
             +"Ljava/lang/String;"
             +"I"
             +"Lorg/mozilla/javascript/Context;"
             +"Lorg/mozilla/javascript/Scriptable;"
             +")Lorg/mozilla/javascript/Scriptable;");
    }

    private void visitObjectLiteral(Node node, Node child)
    {
        Object[] properties = (Object[])node.getProp(Node.OBJECT_IDS_PROP);
        int count = properties.length;

        // load array with property ids
        addNewObjectArray(count);
        for (int i = 0; i != count; ++i) {
            cfw.add(ByteCode.DUP);
            cfw.addPush(i);
            Object id = properties[i];
            if (id instanceof String) {
                cfw.addPush((String)id);
            } else {
                cfw.addPush(((Integer)id).intValue());
                addScriptRuntimeInvoke("wrapInt", "(I)Ljava/lang/Integer;");
            }
            cfw.add(ByteCode.AASTORE);
        }
        // load array with property values
        addNewObjectArray(count);
        for (int i = 0; i != count; ++i) {
            cfw.add(ByteCode.DUP);
            cfw.addPush(i);
            generateCodeFromNode(child, node);
            cfw.add(ByteCode.AASTORE);
            child = child.getNext();
        }

        cfw.addALoad(contextLocal);
        cfw.addALoad(variableObjectLocal);
        addScriptRuntimeInvoke("newObjectLiteral",
             "([Ljava/lang/Object;"
             +"[Ljava/lang/Object;"
             +"Lorg/mozilla/javascript/Context;"
             +"Lorg/mozilla/javascript/Scriptable;"
             +")Lorg/mozilla/javascript/Scriptable;");
    }

    private void visitCall(Node node, int type, Node child)
    {
        /*
         * Generate code for call.
         */

        OptFunctionNode
            target = (OptFunctionNode)node.getProp(Node.DIRECTCALL_PROP);
        int callType = (target == null)
            ? node.getIntProp(Node.SPECIALCALL_PROP, Node.NON_SPECIALCALL)
            : Node.NON_SPECIALCALL;

        cfw.addALoad(contextLocal);

        if (type == Token.CALL && child.getType() == Token.NAME
            && target == null && callType == Node.NON_SPECIALCALL)
        {
            // Optimize common case of name(arguments) calls
            String simpleCallName = child.getString();
            cfw.addPush(simpleCallName);
            cfw.addALoad(variableObjectLocal);
            generateCallArgArray(node, child.getNext(), false);
            addOptRuntimeInvoke("callSimple",
                 "(Lorg/mozilla/javascript/Context;"
                 +"Ljava/lang/String;"
                 +"Lorg/mozilla/javascript/Scriptable;"
                 +"[Ljava/lang/Object;"
                 +")Ljava/lang/Object;");
            return;
        }

        if (type == Token.NEW) {
            generateCodeFromNode(child, node);
            // stack: ... cx functionObj
        } else {
            generateFunctionAndThisObj(child, node);
            // stack: ... cx functionObj thisObj
        }
        child = child.getNext();

        int beyond = 0;
        if (target == null) {
            generateCallArgArray(node, child, false);
        } else {
            beyond = cfw.acquireLabel();

            short thisObjLocal = 0;
            if (type != Token.NEW) {
                thisObjLocal = getNewWordLocal();
                cfw.addAStore(thisObjLocal);
            }
            // stack: ... cx functionObj

            int directTargetIndex = target.getDirectTargetIndex();
            if (isTopLevel) {
                cfw.add(ByteCode.ALOAD_0);
            } else {
                cfw.add(ByteCode.ALOAD_0);
                cfw.add(ByteCode.GETFIELD, codegen.mainClassName,
                        Codegen.DIRECT_CALL_PARENT_FIELD,
                        codegen.mainClassSignature);
            }
            cfw.add(ByteCode.GETFIELD, codegen.mainClassName,
                    Codegen.getDirectTargetFieldName(directTargetIndex),
                    codegen.mainClassSignature);

            cfw.add(ByteCode.DUP2);
            // stack: ... cx functionObj directFunct functionObj directFunct

            int regularCall = cfw.acquireLabel();
            cfw.add(ByteCode.IF_ACMPNE, regularCall);

            short stackHeight = cfw.getStackTop();
            cfw.add(ByteCode.SWAP);
            cfw.add(ByteCode.POP);
            // stack: ... cx directFunct
            if (compilerEnv.isUseDynamicScope()) {
                cfw.add(ByteCode.SWAP);
                cfw.addALoad(variableObjectLocal);
            } else {
                cfw.add(ByteCode.DUP_X1);
                // stack: ... directFunct cx directFunct
                cfw.addInvoke(ByteCode.INVOKEINTERFACE,
                              "org/mozilla/javascript/Scriptable",
                              "getParentScope",
                              "()Lorg/mozilla/javascript/Scriptable;");
            }
            // stack: ... directFunc cx scope

            if (type == Token.NEW) {
                cfw.add(ByteCode.ACONST_NULL);
            } else {
                cfw.addALoad(thisObjLocal);
            }
            // stack: ... directFunc cx scope thisObj
/*
    Remember that directCall parameters are paired in 1 aReg and 1 dReg
    If the argument is an incoming arg, just pass the orginal pair thru.
    Else, if the argument is known to be typed 'Number', pass Void.TYPE
    in the aReg and the number is the dReg
    Else pass the JS object in the aReg and 0.0 in the dReg.
*/
            Node firstArgChild = child;
            while (child != null) {
                int dcp_register = nodeIsDirectCallParameter(child);
                if (dcp_register >= 0) {
                    cfw.addALoad(dcp_register);
                    cfw.addDLoad(dcp_register + 1);
                } else if (child.getIntProp(Node.ISNUMBER_PROP, -1)
                           == Node.BOTH)
                {
                    cfw.add(ByteCode.GETSTATIC,
                            "java/lang/Void",
                            "TYPE",
                            "Ljava/lang/Class;");
                    generateCodeFromNode(child, node);
                } else {
                    generateCodeFromNode(child, node);
                    cfw.addPush(0.0);
                }
                child = child.getNext();
            }

            cfw.add(ByteCode.GETSTATIC,
                    "org/mozilla/javascript/ScriptRuntime",
                    "emptyArgs", "[Ljava/lang/Object;");
            cfw.addInvoke(ByteCode.INVOKESTATIC,
                          codegen.mainClassName,
                          (type == Token.NEW)
                              ? codegen.getDirectCtorName(target.fnode)
                              : codegen.getBodyMethodName(target.fnode),
                          codegen.getBodyMethodSignature(target.fnode));

            cfw.add(ByteCode.GOTO, beyond);

            cfw.markLabel(regularCall, stackHeight);
            cfw.add(ByteCode.POP);
            // stack: functionObj, cx
            if (type != Token.NEW) {
                cfw.addALoad(thisObjLocal);
                releaseWordLocal(thisObjLocal);
            }
            // XXX: this will generate code for the child array the second time,
            // so the code better not to alter tree structure...
            generateCallArgArray(node, firstArgChild, true);
        }

        String className;
        String methodName;
        String callSignature;

        if (callType != Node.NON_SPECIALCALL) {
            className = "org/mozilla/javascript/optimizer/OptRuntime";
            if (type == Token.NEW) {
                methodName = "newObjectSpecial";
                callSignature = "(Lorg/mozilla/javascript/Context;"
                                +"Ljava/lang/Object;"
                                +"[Ljava/lang/Object;"
                                +"Lorg/mozilla/javascript/Scriptable;"
                                +"Lorg/mozilla/javascript/Scriptable;"
                                +"I" // call type
                                +")Ljava/lang/Object;";
                cfw.addALoad(variableObjectLocal);
                cfw.addALoad(thisObjLocal);
                cfw.addPush(callType);
            } else {
                methodName = "callSpecial";
                callSignature = "(Lorg/mozilla/javascript/Context;"
                                +"Ljava/lang/Object;"
                                +"Ljava/lang/Object;"
                                +"[Ljava/lang/Object;"
                                +"Lorg/mozilla/javascript/Scriptable;"
                                +"Lorg/mozilla/javascript/Scriptable;"
                                +"I" // call type
                                +"Ljava/lang/String;I"  // filename, linenumber
                                +")Ljava/lang/Object;";
                cfw.addALoad(variableObjectLocal);
                cfw.addALoad(thisObjLocal);
                cfw.addPush(callType);
                String sourceName = scriptOrFn.getSourceName();
                cfw.addPush(sourceName == null ? "" : sourceName);
                cfw.addPush(itsLineNumber);
            }
        } else {
            className = "org/mozilla/javascript/ScriptRuntime";
            cfw.addALoad(variableObjectLocal);
            if (type == Token.NEW) {
                methodName = "newObject";
                callSignature = "(Lorg/mozilla/javascript/Context;"
                                +"Ljava/lang/Object;"
                                +"[Ljava/lang/Object;"
                                +"Lorg/mozilla/javascript/Scriptable;"
                                +")Lorg/mozilla/javascript/Scriptable;";
            } else {
                methodName = "call";
                callSignature = "(Lorg/mozilla/javascript/Context;"
                                 +"Ljava/lang/Object;"
                                 +"Ljava/lang/Object;"
                                 +"[Ljava/lang/Object;"
                                 +"Lorg/mozilla/javascript/Scriptable;"
                                 +")Ljava/lang/Object;";
            }
        }

        cfw.addInvoke(ByteCode.INVOKESTATIC,
                      className, methodName, callSignature);

        if (target != null) {
            cfw.markLabel(beyond);
        }
    }

    private void visitRefCall(Node node, int type, Node child)
    {
        generateFunctionAndThisObj(child, node);
        // stack: ... functionObj thisObj
        child = child.getNext();
        generateCallArgArray(node, child, false);
        cfw.addALoad(contextLocal);
        cfw.addALoad(variableObjectLocal);
        addScriptRuntimeInvoke("referenceCall",
                               "(Ljava/lang/Object;"
                               +"Ljava/lang/Object;"
                               +"[Ljava/lang/Object;"
                               +"Lorg/mozilla/javascript/Context;"
                               +"Lorg/mozilla/javascript/Scriptable;"
                               +")Ljava/lang/Object;");
    }

    private void generateCallArgArray(Node node, Node argChild, boolean directCall)
    {
        int argCount = 0;
        for (Node child = argChild; child != null; child = child.getNext()) {
            ++argCount;
        }
        // load array object to set arguments
        if (argCount == 1 && itsOneArgArray >= 0) {
            cfw.addALoad(itsOneArgArray);
        } else {
            addNewObjectArray(argCount);
        }
        // Copy arguments into it
        for (int i = 0; i != argCount; ++i) {
            cfw.add(ByteCode.DUP);
            cfw.addPush(i);
            if (!directCall) {
                generateCodeFromNode(argChild, node);
            } else {
                // If this has also been a directCall sequence, the Number
                // flag will have remained set for any parameter so that
                // the values could be copied directly into the outgoing
                // args. Here we want to force it to be treated as not in
                // a Number context, so we set the flag off.
                int dcp_register = nodeIsDirectCallParameter(argChild);
                if (dcp_register >= 0) {
                    dcpLoadAsObject(dcp_register);
                } else {
                    generateCodeFromNode(argChild, node);
                    int childNumberFlag
                            = argChild.getIntProp(Node.ISNUMBER_PROP, -1);
                    if (childNumberFlag == Node.BOTH) {
                        addDoubleWrap();
                    }
                }
            }
            cfw.add(ByteCode.AASTORE);
            argChild = argChild.getNext();
        }
    }

    private void generateFunctionAndThisObj(Node node, Node parent)
    {
        // Place on stack (function object, function this) pair
        switch (node.getType()) {
          case Token.GETPROP:
            // x.y(...)
            //  -> tmp = x, (tmp.y, tmp)(...)
            visitGetProp(node, node.getFirstChild(), true);
            cfw.add(ByteCode.SWAP);
            break;

          case Token.GETELEM:
            // x[y](...)
            //  -> tmp = x, (tmp[y], tmp)(...)
            visitGetElem(node, node.getFirstChild(), true);
            cfw.add(ByteCode.SWAP);
            break;

          case Token.NAME: {
            // name()(...)
            //  -> base = getBase("name"), (base.name, getThis(base))(...)
            String name = node.getString();
            cfw.addALoad(variableObjectLocal);
            cfw.addPush(name);
            addScriptRuntimeInvoke("getBase",
                                   "(Lorg/mozilla/javascript/Scriptable;"
                                   +"Ljava/lang/String;"
                                   +")Lorg/mozilla/javascript/Scriptable;");
            cfw.add(ByteCode.DUP);
            cfw.addPush(name);
            addOptRuntimeInvoke(
                "getPropScriptable",
                "(Lorg/mozilla/javascript/Scriptable;"
                +"Ljava/lang/String;"
                +")Ljava/lang/Object;");
            // swap property and base to call getThis(base)
            cfw.add(ByteCode.SWAP);

            // Conditionally call getThis.
            // The getThis entry in the runtime will take a
            // Scriptable object intended to be used as a 'this'
            // and make sure that it is neither a With object or
            // an activation object.
            // Executing getThis requires at least two instanceof
            // tests, so we only include it if we are currently
            // inside a 'with' statement, or if we are executing
            // a script (to protect against an eval inside a with).
            if (withNesting != 0
                || (fnCurrent == null && compilerEnv.isFromEval()))
            {
                addScriptRuntimeInvoke("getThis",
                                       "(Lorg/mozilla/javascript/Scriptable;"
                                       +")Lorg/mozilla/javascript/Scriptable;");
            }
            break;
          }

          default: // including GETVAR
            // something(...)
            //  -> tmp = something, (tmp, getParent(tmp))(...)
            generateCodeFromNode(node, parent);
            cfw.add(ByteCode.DUP);
            addScriptRuntimeInvoke("getParent",
                                   "(Ljava/lang/Object;"
                                    +")Lorg/mozilla/javascript/Scriptable;");
            break;
        }
    }

    private void visitStatement(Node node)
    {
        itsLineNumber = node.getLineno();
        if (itsLineNumber == -1)
            return;
        cfw.addLineNumberEntry((short)itsLineNumber);
    }


    private void visitTryCatchFinally(Node.Jump node, Node child)
    {
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
        cfw.addALoad(variableObjectLocal);
        cfw.addAStore(savedVariableObject);

        /*
         * Generate the code for the tree; most of the work is done in IRFactory
         * and NodeTransformer;  Codegen just adds the java handlers for the
         * javascript catch and finally clauses.  */
        // need to set the stack top to 1 to account for the incoming exception
        int startLabel = cfw.acquireLabel();
        cfw.markLabel(startLabel, (short)1);

        visitStatement(node);
        while (child != null) {
            generateCodeFromNode(child, node);
            child = child.getNext();
        }

        Node.Target catchTarget = node.target;
        Node.Target finallyTarget = node.getFinally();

        // control flow skips the handlers
        int realEnd = cfw.acquireLabel();
        cfw.add(ByteCode.GOTO, realEnd);

        int exceptionLocal = getLocalBlockRegister(node);
        // javascript handler; unwrap exception and GOTO to javascript
        // catch area.
        if (catchTarget != null) {
            // get the label to goto
            int catchLabel = catchTarget.labelId;

            generateCatchBlock(JAVASCRIPT_EXCEPTION, savedVariableObject,
                               catchLabel, startLabel, exceptionLocal);
            /*
             * catch WrappedExceptions, see if they are wrapped
             * JavaScriptExceptions. Otherwise, rethrow.
             */
            generateCatchBlock(EVALUATOR_EXCEPTION, savedVariableObject,
                               catchLabel, startLabel, exceptionLocal);

            /*
                we also need to catch EcmaErrors and feed the
                associated error object to the handler
            */
            generateCatchBlock(ECMAERROR_EXCEPTION, savedVariableObject,
                               catchLabel, startLabel, exceptionLocal);
        }

        // finally handler; catch all exceptions, store to a local; JSR to
        // the finally, then re-throw.
        if (finallyTarget != null) {
            int finallyHandler = cfw.acquireLabel();
            cfw.markHandler(finallyHandler);
            cfw.addAStore(exceptionLocal);

            // reset the variable object local
            cfw.addALoad(savedVariableObject);
            cfw.addAStore(variableObjectLocal);

            // get the label to JSR to
            int finallyLabel = finallyTarget.labelId;
            cfw.add(ByteCode.JSR, finallyLabel);

            // rethrow
            cfw.addALoad(exceptionLocal);
            cfw.add(ByteCode.ATHROW);

            // mark the handler
            cfw.addExceptionHandler(startLabel, finallyLabel,
                                    finallyHandler, null); // catch any
        }
        releaseWordLocal(savedVariableObject);
        cfw.markLabel(realEnd);
    }

    private final int JAVASCRIPT_EXCEPTION  = 0;
    private final int EVALUATOR_EXCEPTION   = 1;
    private final int ECMAERROR_EXCEPTION   = 2;

    private void generateCatchBlock(int exceptionType,
                                    short savedVariableObject,
                                    int catchLabel, int startLabel,
                                    int exceptionLocal)
    {
        int handler = cfw.acquireLabel();
        cfw.markHandler(handler);

        // MS JVM gets cranky if the exception object is left on the stack
        // XXX: is it possible to use on MS JVM exceptionLocal to store it?
        short exceptionObject = getNewWordLocal();
        cfw.addAStore(exceptionObject);

        // reset the variable object local
        cfw.addALoad(savedVariableObject);
        cfw.addAStore(variableObjectLocal);

        cfw.addALoad(contextLocal);
        cfw.addALoad(variableObjectLocal);
        cfw.addALoad(exceptionObject);
        releaseWordLocal(exceptionObject);

        // unwrap the exception...
        addScriptRuntimeInvoke(
            "getCatchObject",
            "(Lorg/mozilla/javascript/Context;"
            +"Lorg/mozilla/javascript/Scriptable;"
            +"Ljava/lang/Throwable;"
            +")Ljava/lang/Object;");

        cfw.addAStore(exceptionLocal);
        String exceptionName;

        if (exceptionType == JAVASCRIPT_EXCEPTION) {
            exceptionName = "org/mozilla/javascript/JavaScriptException";
        } else if (exceptionType == EVALUATOR_EXCEPTION) {
            exceptionName = "org/mozilla/javascript/EvaluatorException";
        } else {
            if (exceptionType != ECMAERROR_EXCEPTION) Kit.codeBug();
            exceptionName = "org/mozilla/javascript/EcmaError";
        }

        // mark the handler
        cfw.addExceptionHandler(startLabel, catchLabel, handler,
                                exceptionName);

        cfw.add(ByteCode.GOTO, catchLabel);
    }

    private void visitThrow(Node node, Node child)
    {
        visitStatement(node);
        while (child != null) {
            generateCodeFromNode(child, node);
            child = child.getNext();
        }

        cfw.add(ByteCode.NEW,
                      "org/mozilla/javascript/JavaScriptException");
        cfw.add(ByteCode.DUP_X1);
        cfw.add(ByteCode.SWAP);
        cfw.addPush(scriptOrFn.getSourceName());
        cfw.addPush(itsLineNumber);
        cfw.addInvoke(ByteCode.INVOKESPECIAL,
                      "org/mozilla/javascript/JavaScriptException",
                      "<init>",
                      "(Ljava/lang/Object;Ljava/lang/String;I)V");

        cfw.add(ByteCode.ATHROW);
    }

    private void visitSwitch(Node.Jump node, Node child)
    {
        visitStatement(node);
        while (child != null) {
            generateCodeFromNode(child, node);
            child = child.getNext();
        }

        // save selector value
        short selector = getNewWordLocal();
        cfw.addAStore(selector);

        ObjArray cases = (ObjArray) node.getProp(Node.CASES_PROP);
        for (int i=0; i < cases.size(); i++) {
            Node thisCase = (Node) cases.get(i);
            Node first = thisCase.getFirstChild();
            generateCodeFromNode(first, thisCase);
            cfw.addALoad(selector);
            addScriptRuntimeInvoke("shallowEq",
                                   "(Ljava/lang/Object;"
                                   +"Ljava/lang/Object;"
                                   +")Z");
            Node.Target target = new Node.Target();
            thisCase.replaceChild(first, target);
            acquireTargetLabel(target);
            cfw.add(ByteCode.IFNE, target.labelId);
        }
        releaseWordLocal(selector);

        Node defaultNode = (Node) node.getProp(Node.DEFAULT_PROP);
        if (defaultNode != null) {
            Node.Target defaultTarget = new Node.Target();
            defaultNode.getFirstChild().addChildToFront(defaultTarget);
            acquireTargetLabel(defaultTarget);
            cfw.add(ByteCode.GOTO, defaultTarget.labelId);
        }

        Node.Target breakTarget = node.target;
        acquireTargetLabel(breakTarget);
        cfw.add(ByteCode.GOTO, breakTarget.labelId);
    }

    private void visitTypeofname(Node node)
    {
        String name = node.getString();
        if (hasVarsInRegs) {
            OptLocalVariable lVar = fnCurrent.getVar(name);
            if (lVar != null) {
                if (lVar.isNumber()) {
                    cfw.addPush("number");
                } else if (varIsDirectCallParameter(lVar)) {
                    int dcp_register = lVar.getJRegister();
                    cfw.addALoad(dcp_register);
                    cfw.add(ByteCode.GETSTATIC, "java/lang/Void", "TYPE",
                            "Ljava/lang/Class;");
                    int isNumberLabel = cfw.acquireLabel();
                    cfw.add(ByteCode.IF_ACMPEQ, isNumberLabel);
                    short stack = cfw.getStackTop();
                    cfw.addALoad(dcp_register);
                    addScriptRuntimeInvoke("typeof",
                                           "(Ljava/lang/Object;"
                                           +")Ljava/lang/String;");
                    int beyond = cfw.acquireLabel();
                    cfw.add(ByteCode.GOTO, beyond);
                    cfw.markLabel(isNumberLabel, stack);
                    cfw.addPush("number");
                    cfw.markLabel(beyond);
                } else {
                    cfw.addALoad(lVar.getJRegister());
                    addScriptRuntimeInvoke("typeof",
                                           "(Ljava/lang/Object;"
                                           +")Ljava/lang/String;");
                }
                return;
            }
        }
        cfw.addALoad(variableObjectLocal);
        cfw.addPush(name);
        addScriptRuntimeInvoke("typeofName",
                               "(Lorg/mozilla/javascript/Scriptable;"
                               +"Ljava/lang/String;"
                               +")Ljava/lang/String;");
    }

    private void visitIncDec(Node node, boolean isInc)
    {
        int incrDecrMask = node.getExistingIntProp(Node.INCRDECR_PROP);
        Node child = node.getFirstChild();
        switch (child.getType()) {
          case Token.GETVAR:
            if (node.getIntProp(Node.ISNUMBER_PROP, -1) != -1) {
                boolean post = ((incrDecrMask & Node.POST_FLAG) != 0);
                OptLocalVariable lVar = OptLocalVariable.get(child);
                short reg = lVar.getJRegister();
                cfw.addDLoad(reg);
                if (post) {
                    cfw.add(ByteCode.DUP2);
                }
                cfw.addPush(1.0);
                if ((incrDecrMask & Node.DECR_FLAG) == 0) {
                    cfw.add(ByteCode.DADD);
                } else {
                    cfw.add(ByteCode.DSUB);
                }
                if (!post) {
                    cfw.add(ByteCode.DUP2);
                }
                cfw.addDStore(reg);
                break;
            } else if (hasVarsInRegs) {
                boolean post = ((incrDecrMask & Node.POST_FLAG) != 0);
                OptLocalVariable lVar = OptLocalVariable.get(child);
                if (lVar == null)
                    lVar = fnCurrent.getVar(child.getString());
                short reg = lVar.getJRegister();
                cfw.addALoad(reg);
                if (post) {
                    cfw.add(ByteCode.DUP);
                }
                addObjectToDouble();
                cfw.addPush(1.0);
                if ((incrDecrMask & Node.DECR_FLAG) == 0) {
                    cfw.add(ByteCode.DADD);
                } else {
                    cfw.add(ByteCode.DSUB);
                }
                addDoubleWrap();
                if (!post) {
                    cfw.add(ByteCode.DUP);
                }
                cfw.addAStore(reg);
                break;
            }
            // fallthrough
          case Token.NAME:
            cfw.addALoad(variableObjectLocal);
            cfw.addPush(child.getString());          // push name
            cfw.addPush(incrDecrMask);
            addScriptRuntimeInvoke("nameIncrDecr",
                "(Lorg/mozilla/javascript/Scriptable;"
                +"Ljava/lang/String;"
                +"I)Ljava/lang/Object;");
            break;
          case Token.GETPROP: {
            Node getPropChild = child.getFirstChild();
            generateCodeFromNode(getPropChild, node);
            generateCodeFromNode(getPropChild.getNext(), node);
            cfw.addALoad(variableObjectLocal);
            cfw.addPush(incrDecrMask);
            addScriptRuntimeInvoke("propIncrDecr",
                                   "(Ljava/lang/Object;"
                                   +"Ljava/lang/String;"
                                   +"Lorg/mozilla/javascript/Scriptable;"
                                   +"I)Ljava/lang/Object;");
            break;
          }
          case Token.GETELEM: {
            Node getElemChild = child.getFirstChild();
            generateCodeFromNode(getElemChild, node);
            generateCodeFromNode(getElemChild.getNext(), node);
            cfw.addALoad(variableObjectLocal);
            cfw.addPush(incrDecrMask);
            addScriptRuntimeInvoke("elemIncrDecr",
                                   "(Ljava/lang/Object;"
                                   +"Ljava/lang/Object;"
                                   +"Lorg/mozilla/javascript/Scriptable;"
                                   +"I)Ljava/lang/Object;");
            break;
          }
          case Token.GET_REF: {
            Node refChild = child.getFirstChild();
            generateCodeFromNode(refChild, node);
            cfw.addPush(incrDecrMask);
            addScriptRuntimeInvoke(
                "referenceIncrDecr", "(Ljava/lang/Object;I)Ljava/lang/Object;");
            break;
          }
          default:
            Codegen.badTree();
        }
    }

    private static boolean isArithmeticNode(Node node)
    {
        int type = node.getType();
        return (type == Token.SUB)
                  || (type == Token.MOD)
                        || (type == Token.DIV)
                              || (type == Token.MUL);
    }

    private void visitArithmetic(Node node, byte opCode, Node child,
                                 Node parent)
    {
        int childNumberFlag = node.getIntProp(Node.ISNUMBER_PROP, -1);
        if (childNumberFlag != -1) {
            generateCodeFromNode(child, node);
            generateCodeFromNode(child.getNext(), node);
            cfw.add(opCode);
        }
        else {
            boolean childOfArithmetic = isArithmeticNode(parent);
            generateCodeFromNode(child, node);
            if (!isArithmeticNode(child))
                addObjectToDouble();
            generateCodeFromNode(child.getNext(), node);
            if (!isArithmeticNode(child.getNext()))
                  addObjectToDouble();
            cfw.add(opCode);
            if (!childOfArithmetic) {
                addDoubleWrap();
            }
        }
    }

    private void visitBitOp(Node node, int type, Node child)
    {
        int childNumberFlag = node.getIntProp(Node.ISNUMBER_PROP, -1);
        generateCodeFromNode(child, node);

        // special-case URSH; work with the target arg as a long, so
        // that we can return a 32-bit unsigned value, and call
        // toUint32 instead of toInt32.
        if (type == Token.URSH) {
            addScriptRuntimeInvoke("toUint32", "(Ljava/lang/Object;)J");
            generateCodeFromNode(child.getNext(), node);
            addScriptRuntimeInvoke("toInt32", "(Ljava/lang/Object;)I");
            // Looks like we need to explicitly mask the shift to 5 bits -
            // LUSHR takes 6 bits.
            cfw.addPush(31);
            cfw.add(ByteCode.IAND);
            cfw.add(ByteCode.LUSHR);
            cfw.add(ByteCode.L2D);
            addDoubleWrap();
            return;
        }
        if (childNumberFlag == -1) {
            addScriptRuntimeInvoke("toInt32", "(Ljava/lang/Object;)I");
            generateCodeFromNode(child.getNext(), node);
            addScriptRuntimeInvoke("toInt32", "(Ljava/lang/Object;)I");
        }
        else {
            addScriptRuntimeInvoke("toInt32", "(D)I");
            generateCodeFromNode(child.getNext(), node);
            addScriptRuntimeInvoke("toInt32", "(D)I");
        }
        switch (type) {
          case Token.BITOR:
            cfw.add(ByteCode.IOR);
            break;
          case Token.BITXOR:
            cfw.add(ByteCode.IXOR);
            break;
          case Token.BITAND:
            cfw.add(ByteCode.IAND);
            break;
          case Token.RSH:
            cfw.add(ByteCode.ISHR);
            break;
          case Token.LSH:
            cfw.add(ByteCode.ISHL);
            break;
          default:
            throw Codegen.badTree();
        }
        cfw.add(ByteCode.I2D);
        if (childNumberFlag == -1) {
            addDoubleWrap();
        }
    }

    private int nodeIsDirectCallParameter(Node node)
    {
        if (node.getType() == Token.GETVAR
            && inDirectCallFunction && !itsForcedObjectParameters)
        {
            OptLocalVariable lVar = OptLocalVariable.get(node);
            if (lVar.isParameter()) {
                return lVar.getJRegister();
            }
        }
        return -1;
    }

    private boolean varIsDirectCallParameter(OptLocalVariable lVar)
    {
        return lVar.isParameter()
            && inDirectCallFunction && !itsForcedObjectParameters;
    }

    private void genSimpleCompare(int type, int trueGOTO, int falseGOTO)
    {
        if (trueGOTO == -1) throw Codegen.badTree();
        switch (type) {
            case Token.LE :
                cfw.add(ByteCode.DCMPG);
                cfw.add(ByteCode.IFLE, trueGOTO);
                break;
            case Token.GE :
                cfw.add(ByteCode.DCMPL);
                cfw.add(ByteCode.IFGE, trueGOTO);
                break;
            case Token.LT :
                cfw.add(ByteCode.DCMPG);
                cfw.add(ByteCode.IFLT, trueGOTO);
                break;
            case Token.GT :
                cfw.add(ByteCode.DCMPL);
                cfw.add(ByteCode.IFGT, trueGOTO);
                break;
            default :
                throw Codegen.badTree();

        }
        if (falseGOTO != -1)
            cfw.add(ByteCode.GOTO, falseGOTO);
    }

    private void visitIfJumpRelOp(Node node, Node child,
                                  int trueGOTO, int falseGOTO)
    {
        if (trueGOTO == -1 || falseGOTO == -1) throw Codegen.badTree();
        int type = node.getType();
        Node rChild = child.getNext();
        if (type == Token.INSTANCEOF || type == Token.IN) {
            generateCodeFromNode(child, node);
            generateCodeFromNode(rChild, node);
            cfw.addALoad(variableObjectLocal);
            addScriptRuntimeInvoke(
                (type == Token.INSTANCEOF) ? "instanceOf" : "in",
                "(Ljava/lang/Object;"
                +"Ljava/lang/Object;"
                +"Lorg/mozilla/javascript/Scriptable;"
                +")Z");
            cfw.add(ByteCode.IFNE, trueGOTO);
            cfw.add(ByteCode.GOTO, falseGOTO);
            return;
        }
        int childNumberFlag = node.getIntProp(Node.ISNUMBER_PROP, -1);
        int left_dcp_register = nodeIsDirectCallParameter(child);
        int right_dcp_register = nodeIsDirectCallParameter(rChild);
        if (childNumberFlag != -1) {
            // Force numeric context on both parameters and optimize
            // direct call case as Optimizer currently does not handle it

            if (childNumberFlag != Node.RIGHT) {
                // Left already has number content
                generateCodeFromNode(child, node);
            } else if (left_dcp_register != -1) {
                dcpLoadAsNumber(left_dcp_register);
            } else {
                generateCodeFromNode(child, node);
                addObjectToDouble();
            }

            if (childNumberFlag != Node.LEFT) {
                // Right already has number content
                generateCodeFromNode(rChild, node);
            } else if (right_dcp_register != -1) {
                dcpLoadAsNumber(right_dcp_register);
            } else {
                generateCodeFromNode(rChild, node);
                addObjectToDouble();
            }

            genSimpleCompare(type, trueGOTO, falseGOTO);

        } else {
            if (left_dcp_register != -1 && right_dcp_register != -1) {
                // Generate code to dynamically check for number content
                // if both operands are dcp
                short stack = cfw.getStackTop();
                int leftIsNotNumber = cfw.acquireLabel();
                cfw.addALoad(left_dcp_register);
                cfw.add(ByteCode.GETSTATIC,
                        "java/lang/Void",
                        "TYPE",
                        "Ljava/lang/Class;");
                cfw.add(ByteCode.IF_ACMPNE, leftIsNotNumber);
                cfw.addDLoad(left_dcp_register + 1);
                dcpLoadAsNumber(right_dcp_register);
                genSimpleCompare(type, trueGOTO, falseGOTO);
                if (stack != cfw.getStackTop()) throw Codegen.badTree();

                cfw.markLabel(leftIsNotNumber);
                int rightIsNotNumber = cfw.acquireLabel();
                cfw.addALoad(right_dcp_register);
                cfw.add(ByteCode.GETSTATIC,
                        "java/lang/Void",
                        "TYPE",
                        "Ljava/lang/Class;");
                cfw.add(ByteCode.IF_ACMPNE, rightIsNotNumber);
                cfw.addALoad(left_dcp_register);
                addObjectToDouble();
                cfw.addDLoad(right_dcp_register + 1);
                genSimpleCompare(type, trueGOTO, falseGOTO);
                if (stack != cfw.getStackTop()) throw Codegen.badTree();

                cfw.markLabel(rightIsNotNumber);
                // Load both register as objects to call generic cmp_*
                cfw.addALoad(left_dcp_register);
                cfw.addALoad(right_dcp_register);

            } else {
                generateCodeFromNode(child, node);
                generateCodeFromNode(rChild, node);
            }

            if (type == Token.GE || type == Token.GT) {
                cfw.add(ByteCode.SWAP);
            }
            String routine = ((type == Token.LT)
                      || (type == Token.GT)) ? "cmp_LT" : "cmp_LE";
            addScriptRuntimeInvoke(routine,
                                   "(Ljava/lang/Object;"
                                   +"Ljava/lang/Object;"
                                   +")Z");
            cfw.add(ByteCode.IFNE, trueGOTO);
            cfw.add(ByteCode.GOTO, falseGOTO);
        }
    }

    private void visitIfJumpEqOp(Node node, Node child,
                                 int trueGOTO, int falseGOTO)
    {
        if (trueGOTO == -1 || falseGOTO == -1) throw Codegen.badTree();

        short stackInitial = cfw.getStackTop();
        int type = node.getType();
        Node rChild = child.getNext();

        // Optimize if one of operands is null
        if (child.getType() == Token.NULL || rChild.getType() == Token.NULL) {
            // eq is symmetric in this case
            if (child.getType() == Token.NULL) {
                child = rChild;
            }
            generateCodeFromNode(child, node);
            if (type == Token.SHEQ || type == Token.SHNE) {
                byte testCode = (type == Token.SHEQ)
                                ? ByteCode.IFNULL : ByteCode.IFNONNULL;
                cfw.add(testCode, trueGOTO);
            } else {
                if (type != Token.EQ) {
                    // swap false/true targets for !=
                    if (type != Token.NE) throw Codegen.badTree();
                    int tmp = trueGOTO;
                    trueGOTO = falseGOTO;
                    falseGOTO = tmp;
                }
                cfw.add(ByteCode.DUP);
                int undefCheckLabel = cfw.acquireLabel();
                cfw.add(ByteCode.IFNONNULL, undefCheckLabel);
                short stack = cfw.getStackTop();
                cfw.add(ByteCode.POP);
                cfw.add(ByteCode.GOTO, trueGOTO);
                cfw.markLabel(undefCheckLabel, stack);
                Codegen.pushUndefined(cfw);
                cfw.add(ByteCode.IF_ACMPEQ, trueGOTO);
            }
            cfw.add(ByteCode.GOTO, falseGOTO);
        } else {
            int child_dcp_register = nodeIsDirectCallParameter(child);
            if (child_dcp_register != -1
                && rChild.getType() == Optimizer.TO_OBJECT)
            {
                Node convertChild = rChild.getFirstChild();
                if (convertChild.getType() == Token.NUMBER) {
                    cfw.addALoad(child_dcp_register);
                    cfw.add(ByteCode.GETSTATIC,
                            "java/lang/Void",
                            "TYPE",
                            "Ljava/lang/Class;");
                    int notNumbersLabel = cfw.acquireLabel();
                    cfw.add(ByteCode.IF_ACMPNE, notNumbersLabel);
                    cfw.addDLoad(child_dcp_register + 1);
                    cfw.addPush(convertChild.getDouble());
                    cfw.add(ByteCode.DCMPL);
                    if (type == Token.EQ)
                        cfw.add(ByteCode.IFEQ, trueGOTO);
                    else
                        cfw.add(ByteCode.IFNE, trueGOTO);
                    cfw.add(ByteCode.GOTO, falseGOTO);
                    cfw.markLabel(notNumbersLabel);
                    // fall thru into generic handling
                }
            }

            generateCodeFromNode(child, node);
            generateCodeFromNode(rChild, node);

            String name;
            byte testCode;
            switch (type) {
              case Token.EQ:
                name = "eq";
                testCode = ByteCode.IFNE;
                break;
              case Token.NE:
                name = "eq";
                testCode = ByteCode.IFEQ;
                break;
              case Token.SHEQ:
                name = "shallowEq";
                testCode = ByteCode.IFNE;
                break;
              case Token.SHNE:
                name = "shallowEq";
                testCode = ByteCode.IFEQ;
                break;
              default:
                throw Codegen.badTree();
            }
            addScriptRuntimeInvoke(name,
                                   "(Ljava/lang/Object;"
                                   +"Ljava/lang/Object;"
                                   +")Z");
            cfw.add(testCode, trueGOTO);
            cfw.add(ByteCode.GOTO, falseGOTO);
        }
        if (stackInitial != cfw.getStackTop()) throw Codegen.badTree();
    }

    private void visitNumber(Node node)
    {
        double num = node.getDouble();
        if (node.getIntProp(Node.ISNUMBER_PROP, -1) != -1) {
            cfw.addPush(num);
        } else {
            codegen.pushNumberAsObject(cfw, num);
        }
    }

    private void visitRegexp(Node node)
    {
        int i = node.getExistingIntProp(Node.REGEXP_PROP);
        // Scripts can not use REGEXP_ARRAY_FIELD_NAME since
        // it it will make script.exec non-reentrant so they
        // store regexp array in a local variable while
        // functions always access precomputed REGEXP_ARRAY_FIELD_NAME
        // not to consume locals
        if (fnCurrent == null) {
            cfw.addALoad(scriptRegexpLocal);
        } else {
            cfw.addALoad(funObjLocal);
            cfw.add(ByteCode.GETFIELD, codegen.mainClassName,
                    Codegen.REGEXP_ARRAY_FIELD_NAME,
                    Codegen.REGEXP_ARRAY_FIELD_TYPE);
        }
        cfw.addPush(i);
        cfw.add(ByteCode.AALOAD);
    }

    private void visitName(Node node)
    {
        cfw.addALoad(variableObjectLocal);             // get variable object
        cfw.addPush(node.getString());                 // push name
        addScriptRuntimeInvoke(
            "name",
            "(Lorg/mozilla/javascript/Scriptable;"
            +"Ljava/lang/String;"
            +")Ljava/lang/Object;");
    }

    private void visitSetName(Node node, Node child)
    {
        String name = node.getFirstChild().getString();
        while (child != null) {
            generateCodeFromNode(child, node);
            child = child.getNext();
        }
        cfw.addALoad(variableObjectLocal);
        cfw.addPush(name);
        addScriptRuntimeInvoke(
            "setName",
            "(Lorg/mozilla/javascript/Scriptable;"
            +"Ljava/lang/Object;"
            +"Lorg/mozilla/javascript/Scriptable;"
            +"Ljava/lang/String;"
            +")Ljava/lang/Object;");
    }

    private void visitGetVar(Node node)
    {
        if (hasVarsInRegs) {
            OptLocalVariable lVar = OptLocalVariable.get(node);
            if (lVar == null) {
                lVar = fnCurrent.getVar(node.getString());
            }
            short reg = lVar.getJRegister();
            if (varIsDirectCallParameter(lVar)) {
                // Remember that here the isNumber flag means that we
                // want to use the incoming parameter in a Number
                // context, so test the object type and convert the
                //  value as necessary.
                if (node.getIntProp(Node.ISNUMBER_PROP, -1) != -1) {
                    dcpLoadAsNumber(reg);
                } else {
                    dcpLoadAsObject(reg);
                }
            } else if (lVar.isNumber()) {
                cfw.addDLoad(reg);
            } else {
                cfw.addALoad(reg);
            }
        } else {
            cfw.addALoad(variableObjectLocal);
            cfw.addPush(node.getString());
            cfw.addALoad(variableObjectLocal);
            addScriptRuntimeInvoke(
                "getProp",
                "(Ljava/lang/Object;"
                +"Ljava/lang/String;"
                +"Lorg/mozilla/javascript/Scriptable;"
                +")Ljava/lang/Object;");
        }
    }

    private void visitSetVar(Node node, Node child, boolean needValue)
    {
        if (hasVarsInRegs) {
            OptLocalVariable lVar = OptLocalVariable.get(node);
            if (lVar == null) {
                lVar = fnCurrent.getVar(child.getString());
            }
            generateCodeFromNode(child.getNext(), node);
            boolean isNumber = (node.getIntProp(Node.ISNUMBER_PROP, -1) != -1);
            short reg = lVar.getJRegister();
            if (varIsDirectCallParameter(lVar)) {
                if (isNumber) {
                    if (needValue) cfw.add(ByteCode.DUP2);
                    cfw.addALoad(reg);
                    cfw.add(ByteCode.GETSTATIC,
                            "java/lang/Void",
                            "TYPE",
                            "Ljava/lang/Class;");
                    int isNumberLabel = cfw.acquireLabel();
                    int beyond = cfw.acquireLabel();
                    cfw.add(ByteCode.IF_ACMPEQ, isNumberLabel);
                    short stack = cfw.getStackTop();
                    addDoubleWrap();
                    cfw.addAStore(reg);
                    cfw.add(ByteCode.GOTO, beyond);
                    cfw.markLabel(isNumberLabel, stack);
                    cfw.addDStore(reg + 1);
                    cfw.markLabel(beyond);
                }
                else {
                    if (needValue) cfw.add(ByteCode.DUP);
                    cfw.addAStore(reg);
                }
            } else {
                if (isNumber) {
                      cfw.addDStore(reg);
                      if (needValue) cfw.addDLoad(reg);
                }
                else {
                    cfw.addAStore(reg);
                    if (needValue) cfw.addALoad(reg);
                }
            }
            return;
        }

        // default: just treat like any other name lookup
        child.setType(Token.BINDNAME);
        node.setType(Token.SETNAME);
        visitSetName(node, child);
        if (!needValue)
            cfw.add(ByteCode.POP);
    }

    private void visitGetProp(Node node, Node child, boolean dupObject)
    {
        generateCodeFromNode(child, node); //object
        if (dupObject) {
            cfw.add(ByteCode.DUP);
        }
        Node nameChild = child.getNext();
        generateCodeFromNode(nameChild, node);  // the name
        /*
            for 'this.foo' we call getPropScriptable which can skip some
            casting overhead.

        */
        int childType = child.getType();
        if (childType == Token.THIS && nameChild.getType() == Token.STRING) {
            addOptRuntimeInvoke(
                "getPropScriptable",
                "(Lorg/mozilla/javascript/Scriptable;"
                +"Ljava/lang/String;"
                +")Ljava/lang/Object;");
        } else {
            cfw.addALoad(variableObjectLocal);
            addScriptRuntimeInvoke(
                "getProp",
                "(Ljava/lang/Object;"
                +"Ljava/lang/String;"
                +"Lorg/mozilla/javascript/Scriptable;"
                +")Ljava/lang/Object;");
        }
    }

    private void visitGetElem(Node node, Node child, boolean dupObject)
    {
        generateCodeFromNode(child, node); // object
        if (dupObject) {
            cfw.add(ByteCode.DUP);
        }
        generateCodeFromNode(child.getNext(), node);  // id
        cfw.addALoad(variableObjectLocal);
        if (node.getIntProp(Node.ISNUMBER_PROP, -1) != -1) {
            addOptRuntimeInvoke(
                "getElem",
                "(Ljava/lang/Object;D"
                +"Lorg/mozilla/javascript/Scriptable;"
                +")Ljava/lang/Object;");
        }
        else {
            addScriptRuntimeInvoke(
                "getElem",
                "(Ljava/lang/Object;"
                +"Ljava/lang/Object;"
                +"Lorg/mozilla/javascript/Scriptable;"
                +")Ljava/lang/Object;");
        }
    }

    private void visitGetRef(Node node, Node child)
    {
        generateCodeFromNode(child, node); // reference
        addScriptRuntimeInvoke(
            "getReference", "(Ljava/lang/Object;)Ljava/lang/Object;");
    }

    private void visitSetProp(int type, Node node, Node child)
    {
        Node objectChild = child;
        generateCodeFromNode(child, node);
        child = child.getNext();
        if (type == Token.SETPROP_OP) {
            cfw.add(ByteCode.DUP);
        }
        Node nameChild = child;
        generateCodeFromNode(child, node);
        child = child.getNext();
        if (type == Token.SETPROP_OP) {
            // stack: ... object object name -> ... object name object name
            cfw.add(ByteCode.DUP_X1);
            cfw.addALoad(variableObjectLocal);
            //for 'this.foo += ...' we call thisGet which can skip some
            //casting overhead.
            if (objectChild.getType() == Token.THIS
                && nameChild.getType() == Token.STRING)
            {
                addOptRuntimeInvoke(
                    "thisGet",
                    "(Lorg/mozilla/javascript/Scriptable;"
                    +"Ljava/lang/String;"
                    +"Lorg/mozilla/javascript/Scriptable;"
                    +")Ljava/lang/Object;");
            } else {
                addScriptRuntimeInvoke(
                    "getProp",
                    "(Ljava/lang/Object;"
                    +"Ljava/lang/String;"
                    +"Lorg/mozilla/javascript/Scriptable;"
                    +")Ljava/lang/Object;");
            }
        }
        generateCodeFromNode(child, node);
        cfw.addALoad(variableObjectLocal);
        addScriptRuntimeInvoke(
            "setProp",
            "(Ljava/lang/Object;"
            +"Ljava/lang/String;"
            +"Ljava/lang/Object;"
            +"Lorg/mozilla/javascript/Scriptable;"
            +")Ljava/lang/Object;");
    }

    private void visitSetElem(int type, Node node, Node child)
    {
        generateCodeFromNode(child, node);
        child = child.getNext();
        if (type == Token.SETELEM_OP) {
            cfw.add(ByteCode.DUP);
        }
        generateCodeFromNode(child, node);
        child = child.getNext();
        boolean indexIsNumber = (node.getIntProp(Node.ISNUMBER_PROP, -1) != -1);
        if (type == Token.SETELEM_OP) {
            if (indexIsNumber) {
                // stack: ... object object number
                //        -> ... object number object number
                cfw.add(ByteCode.DUP2_X1);
                cfw.addALoad(variableObjectLocal);
                addOptRuntimeInvoke(
                    "getElem",
                    "(Ljava/lang/Object;D"
                    +"Lorg/mozilla/javascript/Scriptable;"
                    +")Ljava/lang/Object;");
            } else {
                // stack: ... object object indexObject
                //        -> ... object indexObject object indexObject
                cfw.add(ByteCode.DUP_X1);
                cfw.addALoad(variableObjectLocal);
                addScriptRuntimeInvoke(
                    "getElem",
                    "(Ljava/lang/Object;"
                    +"Ljava/lang/Object;"
                    +"Lorg/mozilla/javascript/Scriptable;"
                    +")Ljava/lang/Object;");
            }
        }
        generateCodeFromNode(child, node);
        cfw.addALoad(variableObjectLocal);
        if (indexIsNumber) {
            addOptRuntimeInvoke(
                "setElem",
                "(Ljava/lang/Object;"
                +"D"
                +"Ljava/lang/Object;"
                +"Lorg/mozilla/javascript/Scriptable;"
                +")Ljava/lang/Object;");
        } else {
            addScriptRuntimeInvoke(
                "setElem",
                "(Ljava/lang/Object;"
                +"Ljava/lang/Object;"
                +"Ljava/lang/Object;"
                +"Lorg/mozilla/javascript/Scriptable;"
                +")Ljava/lang/Object;");
        }
    }

    private void visitSetRef(int type, Node node, Node child)
    {
        generateCodeFromNode(child, node);
        child = child.getNext();
        if (type == Token.SET_REF_OP) {
            cfw.add(ByteCode.DUP);
            addScriptRuntimeInvoke(
                "getReference", "(Ljava/lang/Object;)Ljava/lang/Object;");
        }
        generateCodeFromNode(child, node);
        cfw.add(ByteCode.DUP_X1);
        // stack: value ref value
        addScriptRuntimeInvoke(
            "setReference", "(Ljava/lang/Object;Ljava/lang/Object;)V");
        // stack: value
    }

    private void visitBind(Node node, Node child)
    {
        while (child != null) {
            generateCodeFromNode(child, node);
            child = child.getNext();
        }
        // Generate code for "ScriptRuntime.bind(varObj, "s")"
        cfw.addALoad(variableObjectLocal);             // get variable object
        cfw.addPush(node.getString());                 // push name
        addScriptRuntimeInvoke("bind",
            "(Lorg/mozilla/javascript/Scriptable;"
            +"Ljava/lang/String;"
            +")Lorg/mozilla/javascript/Scriptable;");
    }

    private void visitSpecialRef(Node node, Node child)
    {
        int special = node.getExistingIntProp(Node.SPECIAL_PROP_PROP);
        generateCodeFromNode(child, node);
        cfw.addALoad(variableObjectLocal);             // get variable object
        cfw.addPush(special);                 // push name
        addScriptRuntimeInvoke("specialReference",
            "(Ljava/lang/Object;"
            +"Lorg/mozilla/javascript/Scriptable;"
            +"I)Ljava/lang/Object;");
    }

    private int getLocalBlockRegister(Node node)
    {
        Node localBlock = (Node)node.getProp(Node.LOCAL_BLOCK_PROP);
        int localSlot = localBlock.getExistingIntProp(Node.LOCAL_PROP);
        return localSlot;
    }

    private void dcpLoadAsNumber(int dcp_register)
    {
        cfw.addALoad(dcp_register);
        cfw.add(ByteCode.GETSTATIC,
                "java/lang/Void",
                "TYPE",
                "Ljava/lang/Class;");
        int isNumberLabel = cfw.acquireLabel();
        cfw.add(ByteCode.IF_ACMPEQ, isNumberLabel);
        short stack = cfw.getStackTop();
        cfw.addALoad(dcp_register);
        addObjectToDouble();
        int beyond = cfw.acquireLabel();
        cfw.add(ByteCode.GOTO, beyond);
        cfw.markLabel(isNumberLabel, stack);
        cfw.addDLoad(dcp_register + 1);
        cfw.markLabel(beyond);
    }

    private void dcpLoadAsObject(int dcp_register)
    {
        cfw.addALoad(dcp_register);
        cfw.add(ByteCode.GETSTATIC,
                "java/lang/Void",
                "TYPE",
                "Ljava/lang/Class;");
        int isNumberLabel = cfw.acquireLabel();
        cfw.add(ByteCode.IF_ACMPEQ, isNumberLabel);
        short stack = cfw.getStackTop();
        cfw.addALoad(dcp_register);
        int beyond = cfw.acquireLabel();
        cfw.add(ByteCode.GOTO, beyond);
        cfw.markLabel(isNumberLabel, stack);
        cfw.addDLoad(dcp_register + 1);
        addDoubleWrap();
        cfw.markLabel(beyond);
    }

    private void addObjectToDouble()
    {
        addScriptRuntimeInvoke("toNumber", "(Ljava/lang/Object;)D");
    }

    private void addNewObjectArray(int size)
    {
        if (size == 0) {
            if (itsZeroArgArray >= 0) {
                cfw.addALoad(itsZeroArgArray);
            } else {
                cfw.add(ByteCode.GETSTATIC,
                        "org/mozilla/javascript/ScriptRuntime",
                        "emptyArgs", "[Ljava/lang/Object;");
            }
        } else {
            cfw.addPush(size);
            cfw.add(ByteCode.ANEWARRAY, "java/lang/Object");
        }
    }

    private void addScriptRuntimeInvoke(String methodName,
                                        String methodSignature)
    {
        cfw.addInvoke(ByteCode.INVOKESTATIC,
                      "org/mozilla/javascript/ScriptRuntime",
                      methodName,
                      methodSignature);
    }

    private void addOptRuntimeInvoke(String methodName,
                                     String methodSignature)
    {
        cfw.addInvoke(ByteCode.INVOKESTATIC,
                      "org/mozilla/javascript/optimizer/OptRuntime",
                      methodName,
                      methodSignature);
    }

    private void addJumpedBooleanWrap(int trueLabel, int falseLabel)
    {
        cfw.markLabel(falseLabel);
        int skip = cfw.acquireLabel();
        cfw.add(ByteCode.GETSTATIC, "java/lang/Boolean",
                                "FALSE", "Ljava/lang/Boolean;");
        cfw.add(ByteCode.GOTO, skip);
        cfw.markLabel(trueLabel);
        cfw.add(ByteCode.GETSTATIC, "java/lang/Boolean",
                                "TRUE", "Ljava/lang/Boolean;");
        cfw.markLabel(skip);
        cfw.adjustStackTop(-1);   // only have 1 of true/false
    }

    private void addDoubleWrap()
    {
        addOptRuntimeInvoke("wrapDouble", "(D)Ljava/lang/Double;");
    }

    private short getNewWordPairLocal()
    {
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

    private short getNewWordLocal()
    {
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

    private void releaseWordpairLocal(short local)
    {
        if (local < firstFreeLocal)
            firstFreeLocal = local;
        locals[local] = false;
        locals[local + 1] = false;
    }

    private void releaseWordLocal(short local)
    {
        if (local < firstFreeLocal)
            firstFreeLocal = local;
        locals[local] = false;
    }

    ClassFileWriter cfw;
    Codegen codegen;
    CompilerEnvirons compilerEnv;
    ScriptOrFnNode scriptOrFn;

    private OptFunctionNode fnCurrent;
    private boolean isTopLevel;

    private static final int MAX_LOCALS = 256;
    private boolean[] locals;
    private short firstFreeLocal;
    private short localsMax;

    private int itsLineNumber;

    private boolean hasVarsInRegs;
    private boolean inDirectCallFunction;
    private boolean itsForcedObjectParameters;
    private int epilogueLabel;

    private int withNesting = 0;

    // special known locals. If you add a new local here, be sure
    // to initialize it to -1 in initBodyGeneration
    private short variableObjectLocal;
    private short popvLocal;
    private short contextLocal;
    private short argsLocal;
    private short thisObjLocal;
    private short funObjLocal;
    private short itsZeroArgArray;
    private short itsOneArgArray;
    private short scriptRegexpLocal;
}
