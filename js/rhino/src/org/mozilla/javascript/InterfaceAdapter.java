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
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
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

package org.mozilla.javascript;

import org.mozilla.classfile.*;
import java.lang.reflect.*;
import java.util.*;

/**
 * Base class for classes that runtime will generate to allow for
 * JS function to implement Java interfaces with single method
 * or multiple methods with the same signature.
 */
public class InterfaceAdapter implements Cloneable, Callable
{
    private Function function;
    private Class nonPrimitiveResultClass;
    private int[] argsToConvert;

    /**
     * Make glue object implementing single-method interface cl that will
     * call the supplied JS function when called.
     */
    public static InterfaceAdapter create(Class cl, Function f)
    {
        ClassCache cache = ClassCache.get(f);
        InterfaceAdapter master;
        master = (InterfaceAdapter)cache.getInterfaceAdapter(cl);
        if (master == null) {
            if (!cl.isInterface())
                return null;
            Method[] methods = cl.getMethods();
            if (methods.length == 0) { return null; }
            Class returnType = methods[0].getReturnType();
            Class[] argTypes = methods[0].getParameterTypes();
            // check that the rest of methods has the same signature
            for (int i = 1; i != methods.length; ++i) {
                if (returnType != methods[i].getReturnType()) { return null; }
                Class[] types2 = methods[i].getParameterTypes();
                if (types2.length != argTypes.length) { return null; }
                for (int j = 0; j != argTypes.length; ++j) {
                    if (types2[j] != argTypes[j]) { return null; }
                }
            }

            String className = "iadapter"+cache.newClassSerialNumber();
            byte[] code = createCode(cl, methods, returnType, argTypes,
                                     className);

            Class iadapterClass = JavaAdapter.loadAdapterClass(className, code);
            try {
                master = (InterfaceAdapter)iadapterClass.newInstance();
            } catch (Exception ex) {
                throw Context.throwAsScriptRuntimeEx(ex);
            }
            master.initMaster(returnType, argTypes);
            cache.cacheInterfaceAdapter(cl, master);
        }
        return master.wrap(f);
    }

    private static byte[] createCode(Class interfaceClass,
                                     Method[] methods,
                                     Class returnType,
                                     Class[] argTypes,
                                     String className)
    {
        String superName = "org.mozilla.javascript.InterfaceAdapter";
        ClassFileWriter cfw = new ClassFileWriter(className,
                                                  superName,
                                                  "<ifglue>");
        cfw.addInterface(interfaceClass.getName());

        // Generate empty constructor
        cfw.startMethod("<init>", "()V", ClassFileWriter.ACC_PUBLIC);
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.addInvoke(ByteCode.INVOKESPECIAL,
                      superName, "<init>", "()V");
        cfw.add(ByteCode.RETURN);
        cfw.stopMethod((short)1); // 1: single this argument

        for (int i = 0; i != methods.length; ++i) {
            Method method = methods[i];
            StringBuffer sb = new StringBuffer();
            int localsEnd = JavaAdapter.appendMethodSignature(argTypes,
                                                              returnType, sb);
            String methodSignature = sb.toString();
            cfw.startMethod(method.getName(), methodSignature,
                            ClassFileWriter.ACC_PUBLIC);
            cfw.addLoadThis();
            JavaAdapter.generatePushWrappedArgs(cfw, argTypes,
                                                argTypes.length + 1);
            // add method name as the last JS parameter
            cfw.add(ByteCode.DUP); // duplicate array reference
            cfw.addPush(argTypes.length);
            cfw.addPush(method.getName());
            cfw.add(ByteCode.AASTORE);

            cfw.addInvoke(ByteCode.INVOKESPECIAL, superName, "doCall",
                          "([Ljava/lang/Object;)Ljava/lang/Object;");
            JavaAdapter.generateReturnResult(cfw, returnType, false);

            cfw.stopMethod((short)localsEnd);
        }

        return cfw.toByteArray();
    }

    private void initMaster(Class returnType, Class[] argTypes)
    {
        // Can only be called on master
        if (this.function != null) Kit.codeBug();
        if (!returnType.isPrimitive()) {
            nonPrimitiveResultClass = returnType;
        }
        this.argsToConvert = JavaAdapter.getArgsToConvert(argTypes);
    }

    private InterfaceAdapter wrap(Function function)
    {
        // Arguments can not be null
        if (function == null)
            Kit.codeBug();
        // Can only be called on master
        if (this.function != null) Kit.codeBug();
        InterfaceAdapter copy;
        try {
            copy = (InterfaceAdapter)clone();
        } catch (CloneNotSupportedException ex) {
            // Should not happen
            copy = null;
        }
        copy.function = function;
        return copy;
    }

    protected final Object doCall(Object[] args)
    {
        Scriptable scope = function.getParentScope();
        Scriptable thisObj = scope;
        Object result = Context.call(null, this, scope, thisObj, args);
        if (nonPrimitiveResultClass != null) {
            if (result == Undefined.instance) {
                // Avoid an error for an undefined value; return null instead.
                result = null;
            } else {
                result = NativeJavaObject.coerceType(nonPrimitiveResultClass,
                                                     result, true);
            }
        }
        return result;
    }

    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
    {
        if (argsToConvert != null) {
            WrapFactory wf = cx.getWrapFactory();
            for (int i = 0, N = argsToConvert.length; i != N; ++i) {
                int index = argsToConvert[i];
                Object arg = args[index];
                if (arg != null && !(arg instanceof Scriptable)) {
                    args[index] = wf.wrap(cx, scope, arg, null);
                }
            }
        }
        return function.call(cx, scope, thisObj, args);
    }


}
