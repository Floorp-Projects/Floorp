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
 * Igor Bukanov
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

import java.io.IOException;
import java.lang.reflect.Constructor;

import org.mozilla.javascript.*;


public class ClassCompiler extends Interpreter
{
    public Object compile(Context cx, Scriptable scope,
                          CompilerEnvirons compilerEnv,
                          ScriptOrFnNode scriptOrFn,
                          String encodedSource,
                          boolean returnFunction,
                          SecurityController securityController,
                          Object securityDomain)
    {
        OptClassNameHelper
            nameHelper = (OptClassNameHelper)ClassNameHelper.get(cx);
        Class[] interfaces = nameHelper.getTargetImplements();
        Class superClass = nameHelper.getTargetExtends();
        boolean isPrimary = (interfaces == null && superClass == null);
        String mainClassName = nameHelper.getScriptClassName(isPrimary);

        Codegen codegen = new Codegen();

        byte[] mainClassBytes
            = codegen.compileToClassFile(compilerEnv, mainClassName,
                                         scriptOrFn, encodedSource,
                                         returnFunction);

        boolean onlySave = false;
        ClassRepository repository = nameHelper.getClassRepository();
        if (repository != null) {
            try {
                if (!repository.storeClass(mainClassName, mainClassBytes,
                                           true))
                {
                    onlySave = true;
                }
            } catch (IOException iox) {
                throw Context.throwAsScriptRuntimeEx(iox);
            }

            if (!isPrimary) {
                String adapterClassName = nameHelper.getScriptClassName(true);
                int functionCount = scriptOrFn.getFunctionCount();
                ObjToIntMap functionNames = new ObjToIntMap(functionCount);
                for (int i = 0; i != functionCount; ++i) {
                    FunctionNode ofn = scriptOrFn.getFunctionNode(i);
                    String name = ofn.getFunctionName();
                    if (name != null && name.length() != 0) {
                        functionNames.put(name, ofn.getParamCount());
                    }
                }
                if (superClass == null) {
                    superClass = ScriptRuntime.ObjectClass;
                }
                byte[] classFile = JavaAdapter.createAdapterCode(
                                       functionNames, adapterClassName,
                                       superClass, interfaces,
                                       mainClassName);
                try {
                    if (!repository.storeClass(adapterClassName, classFile,
                                               true))
                    {
                        onlySave = true;
                    }
                } catch (IOException iox) {
                    throw Context.throwAsScriptRuntimeEx(iox);
                }
            }
        }

        if (onlySave) { return null; }

        Exception e = null;
        Class result = null;
        ClassLoader parentLoader = cx.getApplicationClassLoader();
        GeneratedClassLoader loader;
        if (securityController == null) {
            loader = cx.createClassLoader(parentLoader);
        } else {
            loader = securityController.createClassLoader(parentLoader,
                                                          securityDomain);
        }

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

        if (scriptOrFn.getType() == Token.FUNCTION) {
            NativeFunction f;
            try {
                Constructor ctor = result.getConstructors()[0];
                Object[] initArgs = { scope, cx, new Integer(0) };
                f = (NativeFunction)ctor.newInstance(initArgs);
            } catch (Exception ex) {
                throw new RuntimeException
                    ("Unable to instantiate compiled class:"+ex.toString());
            }
            int ftype = ((FunctionNode)scriptOrFn).getFunctionType();
            OptRuntime.initFunction(f, ftype, scope, cx);
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
        // Not supported
    }
}

