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
 * Patrick Beard
 * Norris Boyd
 * Igor Bukanov
 * Mike McCabe
 * Matthias Radestock
 * Andi Vajda
 * Andrew Wason
 * Kemal Bayram
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
import java.io.*;
import java.util.*;

public final class JavaAdapter implements IdFunctionCall
{
    /**
     * Base class for interface with single method to function glue classes
     */
    public static class IFGlue implements Cloneable, Callable
    {
        final IFGlue ifglue_make(Function function)
        {
            // Arguments can not be null
            if (function == null)
                Kit.codeBug();
            // Can only be called on master
            if (this.function != null) Kit.codeBug();
            IFGlue copy;
            try {
                copy = (IFGlue)clone();
            } catch (CloneNotSupportedException ex) {
                // Should not happen
                copy = null;
            }
            copy.function = function;
            return copy;
        }

        final void ifglue_initMaster(int[] argsToConvert)
        {
            // Can only be called on master
            if (this.function != null) Kit.codeBug();
            this.argsToConvert = argsToConvert;
        }

        protected final Object ifglue_call(Object[] args)
            throws JavaScriptException
        {
            Scriptable scope = function.getParentScope();
            Scriptable thisObj = scope;
            return Context.call(null, this, scope, thisObj, args);
        }

        public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                           Object[] args)
            throws JavaScriptException
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


        private Function function;
        private int[] argsToConvert;
    }

    private static final String IFGLUE_BASE = IFGlue.class.getName();


    /**
     * Provides a key with which to distinguish previously generated
     * adapter classes stored in a hash table.
     */
    static class JavaAdapterSignature
    {
        Class superClass;
        Class[] interfaces;
        ObjToIntMap names;

        JavaAdapterSignature(Class superClass, Class[] interfaces,
                             ObjToIntMap names)
        {
            this.superClass = superClass;
            this.interfaces = interfaces;
            this.names = names;;
        }

        public boolean equals(Object obj)
        {
            if (!(obj instanceof JavaAdapterSignature))
                return false;
            JavaAdapterSignature sig = (JavaAdapterSignature) obj;
            if (superClass != sig.superClass)
                return false;
            if (interfaces != sig.interfaces) {
                if (interfaces.length != sig.interfaces.length)
                    return false;
                for (int i=0; i < interfaces.length; i++)
                    if (interfaces[i] != sig.interfaces[i])
                        return false;
            }
            if (names.size() != sig.names.size())
                return false;
            ObjToIntMap.Iterator iter = new ObjToIntMap.Iterator(names);
            for (iter.start(); !iter.done(); iter.next()) {
                String name = (String)iter.getKey();
                int arity = iter.getValue();
                if (arity != names.get(name, arity + 1))
                    return false;
            }
            return true;
        }

        public int hashCode()
        {
            return superClass.hashCode()
                | (0x9e3779b9 * (names.size() | (interfaces.length << 16)));
        }
    }

    public static void init(Context cx, Scriptable scope, boolean sealed)
    {
        JavaAdapter obj = new JavaAdapter();
        IdFunctionObject ctor = new IdFunctionObject(obj, FTAG, Id_JavaAdapter,
                                                     "JavaAdapter", 1, scope);
        ctor.markAsConstructor(null);
        if (sealed) {
            ctor.sealObject();
        }
        ctor.exportAsScopeProperty();
    }

    public Object execIdCall(IdFunctionObject f, Context cx, Scriptable scope,
                             Scriptable thisObj, Object[] args)
    {
        if (f.hasTag(FTAG)) {
            if (f.methodId() == Id_JavaAdapter) {
                return js_createAdpter(cx, scope, args);
            }
        }
        throw f.unknown();
    }

    public static Object convertResult(Object result, Class c)
    {
        if (result == Undefined.instance &&
            (c != ScriptRuntime.ObjectClass &&
             c != ScriptRuntime.StringClass))
        {
            // Avoid an error for an undefined value; return null instead.
            return null;
        }
        return NativeJavaObject.coerceType(c, result, true);
    }

    public static Scriptable createAdapterWrapper(Scriptable obj,
                                                  Object adapter)
    {
        Scriptable scope = ScriptableObject.getTopLevelScope(obj);
        NativeJavaObject res = new NativeJavaObject(scope, adapter, null);
        res.setPrototype(obj);
        return res;
    }

    public static Object getAdapterSelf(Class adapterClass, Object adapter)
        throws NoSuchFieldException, IllegalAccessException
    {
        Field self = adapterClass.getDeclaredField("self");
        return self.get(adapter);
    }

    static Object js_createAdpter(Context cx, Scriptable scope, Object[] args)
    {
        int N = args.length;
        if (N == 0) {
            throw ScriptRuntime.typeError0("msg.adapter.zero.args");
        }

        Class superClass = null;
        Class[] intfs = new Class[N - 1];
        int interfaceCount = 0;
        for (int i = 0; i != N - 1; ++i) {
            Object arg = args[i];
            if (!(arg instanceof NativeJavaClass)) {
                throw ScriptRuntime.typeError2("msg.not.java.class.arg",
                                               String.valueOf(i),
                                               ScriptRuntime.toString(arg));
            }
            Class c = ((NativeJavaClass) arg).getClassObject();
            if (!c.isInterface()) {
                if (superClass != null) {
                    throw ScriptRuntime.typeError2("msg.only.one.super",
                              superClass.getName(), c.getName());
                }
                superClass = c;
            } else {
                intfs[interfaceCount++] = c;
            }
        }

        if (superClass == null)
            superClass = ScriptRuntime.ObjectClass;

        Class[] interfaces = new Class[interfaceCount];
        System.arraycopy(intfs, 0, interfaces, 0, interfaceCount);
        Scriptable obj = ScriptRuntime.toObject(cx, scope, args[N - 1]);

        Class adapterClass = getAdapterClass(scope, superClass, interfaces,
                                             obj);

        Class[] ctorParms = { ScriptRuntime.ScriptableClass };
        Object[] ctorArgs = { obj };
        try {
            Object adapter = adapterClass.getConstructor(ctorParms).
                                 newInstance(ctorArgs);
            return getAdapterSelf(adapterClass, adapter);
        } catch (Exception ex) {
            throw Context.throwAsScriptRuntimeEx(ex);
        }
    }

    // Needed by NativeJavaObject serializer
    public static void writeAdapterObject(Object javaObject,
                                          ObjectOutputStream out)
        throws IOException
    {
        Class cl = javaObject.getClass();
        out.writeObject(cl.getSuperclass().getName());

        Class[] interfaces = cl.getInterfaces();
        String[] interfaceNames = new String[interfaces.length];

        for (int i=0; i < interfaces.length; i++)
            interfaceNames[i] = interfaces[i].getName();

        out.writeObject(interfaceNames);

        try {
            Object delegee = cl.getField("delegee").get(javaObject);
            out.writeObject(delegee);
            return;
        } catch (IllegalAccessException e) {
        } catch (NoSuchFieldException e) {
        }
        throw new IOException();
    }

    // Needed by NativeJavaObject de-serializer
    public static Object readAdapterObject(Scriptable self,
                                           ObjectInputStream in)
        throws IOException, ClassNotFoundException
    {
        Class superClass = Class.forName((String)in.readObject());

        String[] interfaceNames = (String[])in.readObject();
        Class[] interfaces = new Class[interfaceNames.length];

        for (int i=0; i < interfaceNames.length; i++)
            interfaces[i] = Class.forName(interfaceNames[i]);

        Scriptable delegee = (Scriptable)in.readObject();

        Class adapterClass = getAdapterClass(self, superClass, interfaces,
                                             delegee);

        Class[] ctorParms = {
            ScriptRuntime.ScriptableClass,
            ScriptRuntime.ScriptableClass
        };
        Object[] ctorArgs = { delegee, self };
        try {
            return adapterClass.getConstructor(ctorParms).newInstance(ctorArgs);
        } catch(InstantiationException e) {
        } catch(IllegalAccessException e) {
        } catch(InvocationTargetException e) {
        } catch(NoSuchMethodException e) {
        }

        throw new ClassNotFoundException("adapter");
    }

    private static ObjToIntMap getObjectFunctionNames(Scriptable obj)
    {
        Object[] ids = ScriptableObject.getPropertyIds(obj);
        ObjToIntMap map = new ObjToIntMap(ids.length);
        for (int i = 0; i != ids.length; ++i) {
            if (!(ids[i] instanceof String))
                continue;
            String id = (String) ids[i];
            Object value = ScriptableObject.getProperty(obj, id);
            if (value instanceof Function) {
                Function f = (Function)value;
                int length = ScriptRuntime.toInt32(
                                 ScriptableObject.getProperty(f, "length"));
                if (length < 0) {
                    length = 0;
                }
                map.put(id, length);
            }
        }
        return map;
    }

    private static Class getAdapterClass(Scriptable scope, Class superClass,
                                         Class[] interfaces, Scriptable obj)
    {
        ClassCache cache = ClassCache.get(scope);
        Hashtable generated = cache.javaAdapterGeneratedClasses;

        ObjToIntMap names = getObjectFunctionNames(obj);
        JavaAdapterSignature sig;
        sig = new JavaAdapterSignature(superClass, interfaces, names);
        Class adapterClass = (Class) generated.get(sig);
        if (adapterClass == null) {
            String adapterName = "adapter"
                                 + cache.newClassSerialNumber();
            byte[] code = createAdapterCode(names, adapterName,
                                            superClass, interfaces, null);

            adapterClass = loadAdapterClass(adapterName, code);
            if (cache.isCachingEnabled()) {
                generated.put(sig, adapterClass);
            }
        }
        return adapterClass;
    }

    public static byte[] createAdapterCode(ObjToIntMap functionNames,
                                           String adapterName,
                                           Class superClass,
                                           Class[] interfaces,
                                           String scriptClassName)
    {
        ClassFileWriter cfw = new ClassFileWriter(adapterName,
                                                  superClass.getName(),
                                                  "<adapter>");
        cfw.addField("delegee", "Lorg/mozilla/javascript/Scriptable;",
                     (short) (ClassFileWriter.ACC_PUBLIC |
                              ClassFileWriter.ACC_FINAL));
        cfw.addField("self", "Lorg/mozilla/javascript/Scriptable;",
                     (short) (ClassFileWriter.ACC_PUBLIC |
                              ClassFileWriter.ACC_FINAL));
        int interfacesCount = interfaces == null ? 0 : interfaces.length;
        for (int i=0; i < interfacesCount; i++) {
            if (interfaces[i] != null)
                cfw.addInterface(interfaces[i].getName());
        }

        String superName = superClass.getName().replace('.', '/');
        generateCtor(cfw, adapterName, superName);
        generateSerialCtor(cfw, adapterName, superName);
        if (scriptClassName != null)
            generateEmptyCtor(cfw, adapterName, superName, scriptClassName);

        ObjToIntMap generatedOverrides = new ObjToIntMap();
        ObjToIntMap generatedMethods = new ObjToIntMap();

        // generate methods to satisfy all specified interfaces.
        for (int i = 0; i < interfacesCount; i++) {
            Method[] methods = interfaces[i].getMethods();
            for (int j = 0; j < methods.length; j++) {
                Method method = methods[j];
                int mods = method.getModifiers();
                if (Modifier.isStatic(mods) || Modifier.isFinal(mods)) {
                    continue;
                }
                String methodName = method.getName();
                Class[] argTypes = method.getParameterTypes();
                if (!functionNames.has(methodName)) {
                    try {
                        superClass.getMethod(methodName, argTypes);
                        // The class we're extending implements this method and
                        // the JavaScript object doesn't have an override. See
                        // bug 61226.
                        continue;
                    } catch (NoSuchMethodException e) {
                        // Not implemented by superclass; fall through
                    }
                }
                // make sure to generate only one instance of a particular
                // method/signature.
                String methodSignature = getMethodSignature(method, argTypes);
                String methodKey = methodName + methodSignature;
                if (! generatedOverrides.has(methodKey)) {
                    generateMethod(cfw, adapterName, methodName,
                                   argTypes, method.getReturnType());
                    generatedOverrides.put(methodKey, 0);
                    generatedMethods.put(methodName, 0);
                }
            }
        }

        // Now, go through the superclasses methods, checking for abstract
        // methods or additional methods to override.

        // generate any additional overrides that the object might contain.
        Method[] methods = superClass.getMethods();
        for (int j = 0; j < methods.length; j++) {
            Method method = methods[j];
            int mods = method.getModifiers();
            if (Modifier.isStatic(mods) || Modifier.isFinal(mods))
                continue;
            // if a method is marked abstract, must implement it or the
            // resulting class won't be instantiable. otherwise, if the object
            // has a property of the same name, then an override is intended.
            boolean isAbstractMethod = Modifier.isAbstract(mods);
            String methodName = method.getName();
            if (isAbstractMethod || functionNames.has(methodName)) {
                // make sure to generate only one instance of a particular
                // method/signature.
                Class[] argTypes = method.getParameterTypes();
                String methodSignature = getMethodSignature(method, argTypes);
                String methodKey = methodName + methodSignature;
                if (! generatedOverrides.has(methodKey)) {
                    generateMethod(cfw, adapterName, methodName,
                                   argTypes, method.getReturnType());
                    generatedOverrides.put(methodKey, 0);
                    generatedMethods.put(methodName, 0);
                }
                // if a method was overridden, generate a "super$method"
                // which lets the delegate call the superclass' version.
                if (!isAbstractMethod) {
                    generateSuper(cfw, adapterName, superName,
                                  methodName, methodSignature,
                                  argTypes, method.getReturnType());
                }
            }
        }

        // Generate Java methods for remaining properties that are not
        // overrides.
        ObjToIntMap.Iterator iter = new ObjToIntMap.Iterator(functionNames);
        for (iter.start(); !iter.done(); iter.next()) {
            String functionName = (String)iter.getKey();
            if (generatedMethods.has(functionName))
                continue;
            int length = iter.getValue();
            Class[] parms = new Class[length];
            for (int k=0; k < length; k++)
                parms[k] = ScriptRuntime.ObjectClass;
            generateMethod(cfw, adapterName, functionName, parms,
                           ScriptRuntime.ObjectClass);
        }
        return cfw.toByteArray();
    }

    /**
     * Make glue object implementing single-method interface cl that will
     * call the supplied JS function when called.
     */
    public static IFGlue makeIFGlue(Class cl, Function f)
    {
        ClassCache cache = ClassCache.get(f);
        Hashtable masters = cache.javaAdapterIFGlueMasters;
        IFGlue master = (IFGlue)masters.get(cl);
        if (master == null) {
            if (!cl.isInterface())
                return null;
            Method[] methods = cl.getMethods();
            if (methods.length == 0) { return null; }
            Class[] argTypes = methods[0].getParameterTypes();
            // check that the rest of methods has the same signature
            for (int i = 1; i != methods.length; ++i) {
                Class[] types2 = methods[i].getParameterTypes();
                if (types2.length != argTypes.length) { return null; }
                for (int j = 0; j != argTypes.length; ++j) {
                    if (types2[j] != argTypes[j]) { return null; }
                }
            }

            String glueName = "ifglue"+cache.newClassSerialNumber();
            byte[] code = createIFGlueCode(cl, methods, argTypes, glueName);
            Class glueClass = loadAdapterClass(glueName, code);
            try {
                master = (IFGlue)glueClass.newInstance();
            } catch (Exception ex) {
                throw Context.throwAsScriptRuntimeEx(ex);
            }
            int[] argsToConvert = getArgsToConvert(argTypes);
            master.ifglue_initMaster(argsToConvert);
            if (cache.isCachingEnabled()) {
                masters.put(cl, master);
            }
        }
        return master.ifglue_make(f);
    }

    private static byte[] createIFGlueCode(Class interfaceClass,
                                           Method[] methods,
                                           Class[] argTypes,
                                           String className)
    {
        String superName = IFGLUE_BASE;
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
            Class returnType = method.getReturnType();
            StringBuffer sb = new StringBuffer();
            int localsEnd = appendMethodSignature(argTypes, returnType, sb);
            String methodSignature = sb.toString();
            cfw.startMethod(method.getName(), methodSignature,
                            ClassFileWriter.ACC_PUBLIC);
            cfw.addLoadThis();
            generatePushWrappedArgs(cfw, argTypes, argTypes.length + 1);
            // add method name as the last JS parameter
            cfw.add(ByteCode.DUP); // duplicate array reference
            cfw.addPush(argTypes.length);
            cfw.addPush(method.getName());
            cfw.add(ByteCode.AASTORE);

            cfw.addInvoke(ByteCode.INVOKESPECIAL, superName, "ifglue_call",
                          "([Ljava/lang/Object;)Ljava/lang/Object;");
            generateReturnResult(cfw, returnType);

            cfw.stopMethod((short)localsEnd);
        }

        return cfw.toByteArray();
    }

    private static Class loadAdapterClass(String className, byte[] classBytes)
    {
        GeneratedClassLoader loader
            = SecurityController.createLoader(null, null);
        Class result = loader.defineClass(className, classBytes);
        loader.linkClass(result);
        return result;
    }

    public static Function getFunction(Scriptable obj, String functionName)
    {
        Object x = ScriptableObject.getProperty(obj, functionName);
        if (x == Scriptable.NOT_FOUND) {
            // This method used to swallow the exception from calling
            // an undefined method. People have come to depend on this
            // somewhat dubious behavior. It allows people to avoid
            // implementing listener methods that they don't care about,
            // for instance.
            return null;
        }
        if (!(x instanceof Function))
            throw ScriptRuntime.notFunctionError(x, functionName);

        return (Function)x;
    }

    /**
     * Utility method which dynamically binds a Context to the current thread,
     * if none already exists.
     */
    public static Object callMethod(final Scriptable thisObj,
                                    final Function f, final Object[] args,
                                    final long argsToWrap)
    {
        if (f == null) {
            // See comments in getFunction
            return Undefined.instance;
        }
        final Scriptable scope = f.getParentScope();
        if (argsToWrap == 0) {
            return Context.call(null, f, scope, thisObj, args);
        }

        Context cx = Context.getCurrentContext();
        if (cx != null) {
            return doCall(cx, scope, thisObj, f, args, argsToWrap);
        } else {
            ContextFactory factory = ScriptRuntime.getContextFactory(scope);
            return factory.call(new ContextAction() {
                public Object run(Context cx)
                {
                    return doCall(cx, scope, thisObj, f, args, argsToWrap);
                }
            });
        }
    }

    private static Object doCall(Context cx, Scriptable scope,
                                 Scriptable thisObj, Function f,
                                 Object[] args, long argsToWrap)
    {
        // Wrap the rest of objects
        for (int i = 0; i != args.length; ++i) {
            if (0 != (argsToWrap & (1 << i))) {
                Object arg = args[i];
                if (!(arg instanceof Scriptable)) {
                    args[i] = cx.getWrapFactory().wrap(cx, scope, arg,
                                                       null);
                }
            }
        }
        return f.call(cx, scope, thisObj, args);
    }

    public static Scriptable runScript(final Script script)
    {
        return (Scriptable)Context.call(new ContextAction() {
            public Object run(Context cx)
            {
                ScriptableObject global = ScriptRuntime.getGlobal(cx);
                script.exec(cx, global);
                return global;
            }
        });
    }

    private static void generateCtor(ClassFileWriter cfw, String adapterName,
                                     String superName)
    {
        cfw.startMethod("<init>",
                        "(Lorg/mozilla/javascript/Scriptable;)V",
                        ClassFileWriter.ACC_PUBLIC);

        // Invoke base class constructor
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.addInvoke(ByteCode.INVOKESPECIAL, superName, "<init>", "()V");

        // Save parameter in instance variable "delegee"
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.add(ByteCode.ALOAD_1);  // first arg
        cfw.add(ByteCode.PUTFIELD, adapterName, "delegee",
                "Lorg/mozilla/javascript/Scriptable;");

        cfw.add(ByteCode.ALOAD_0);  // this for the following PUTFIELD for self
        // create a wrapper object to be used as "this" in method calls
        cfw.add(ByteCode.ALOAD_1);  // the Scriptable
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.addInvoke(ByteCode.INVOKESTATIC,
                      "org/mozilla/javascript/JavaAdapter",
                      "createAdapterWrapper",
                      "(Lorg/mozilla/javascript/Scriptable;"
                      +"Ljava/lang/Object;"
                      +")Lorg/mozilla/javascript/Scriptable;");
        cfw.add(ByteCode.PUTFIELD, adapterName, "self",
                "Lorg/mozilla/javascript/Scriptable;");

        cfw.add(ByteCode.RETURN);
        cfw.stopMethod((short)3); // 2: this + delegee
    }

    private static void generateSerialCtor(ClassFileWriter cfw,
                                           String adapterName,
                                           String superName)
    {
        cfw.startMethod("<init>",
                        "(Lorg/mozilla/javascript/Scriptable;"
                        +"Lorg/mozilla/javascript/Scriptable;"
                        +")V",
                        ClassFileWriter.ACC_PUBLIC);

        // Invoke base class constructor
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.addInvoke(ByteCode.INVOKESPECIAL, superName, "<init>", "()V");

        // Save parameter in instance variable "delegee"
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.add(ByteCode.ALOAD_1);  // first arg
        cfw.add(ByteCode.PUTFIELD, adapterName, "delegee",
                "Lorg/mozilla/javascript/Scriptable;");

        // save self
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.add(ByteCode.ALOAD_2);  // second arg
        cfw.add(ByteCode.PUTFIELD, adapterName, "self",
                "Lorg/mozilla/javascript/Scriptable;");

        cfw.add(ByteCode.RETURN);
        cfw.stopMethod((short)20); // TODO: magic number "20"
    }

    private static void generateEmptyCtor(ClassFileWriter cfw,
                                          String adapterName,
                                          String superName,
                                          String scriptClassName)
    {
        cfw.startMethod("<init>", "()V", ClassFileWriter.ACC_PUBLIC);

        // Invoke base class constructor
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.addInvoke(ByteCode.INVOKESPECIAL, superName, "<init>", "()V");

        // Load script class
        cfw.add(ByteCode.NEW, scriptClassName);
        cfw.add(ByteCode.DUP);
        cfw.addInvoke(ByteCode.INVOKESPECIAL, scriptClassName, "<init>", "()V");

        // Run script and save resulting scope
        cfw.addInvoke(ByteCode.INVOKESTATIC,
                      "org/mozilla/javascript/JavaAdapter",
                      "runScript",
                      "(Lorg/mozilla/javascript/Script;"
                      +")Lorg/mozilla/javascript/Scriptable;");
        cfw.add(ByteCode.ASTORE_1);

        // Save the Scriptable in instance variable "delegee"
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.add(ByteCode.ALOAD_1);  // the Scriptable
        cfw.add(ByteCode.PUTFIELD, adapterName, "delegee",
                "Lorg/mozilla/javascript/Scriptable;");

        cfw.add(ByteCode.ALOAD_0);  // this for the following PUTFIELD for self
        // create a wrapper object to be used as "this" in method calls
        cfw.add(ByteCode.ALOAD_1);  // the Scriptable
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.addInvoke(ByteCode.INVOKESTATIC,
                      "org/mozilla/javascript/JavaAdapter",
                      "createAdapterWrapper",
                      "(Lorg/mozilla/javascript/Scriptable;"
                      +"Ljava/lang/Object;"
                      +")Lorg/mozilla/javascript/Scriptable;");
        cfw.add(ByteCode.PUTFIELD, adapterName, "self",
                "Lorg/mozilla/javascript/Scriptable;");

        cfw.add(ByteCode.RETURN);
        cfw.stopMethod((short)2); // this + delegee
    }

    /**
     * Generates code to wrap Java arguments into Object[].
     * Non-primitive Java types are left as is pending convertion
     * in the helper method. Leaves the array object on the top of the stack.
     */
    private static void generatePushWrappedArgs(ClassFileWriter cfw,
                                                Class[] argTypes,
                                                int arrayLength)
    {
        // push arguments
        cfw.addPush(arrayLength);
        cfw.add(ByteCode.ANEWARRAY, "java/lang/Object");
        int paramOffset = 1;
        for (int i = 0; i != argTypes.length; ++i) {
            cfw.add(ByteCode.DUP); // duplicate array reference
            cfw.addPush(i);
            paramOffset += generateWrapArg(cfw, paramOffset, argTypes[i]);
            cfw.add(ByteCode.AASTORE);
        }
    }

    /**
     * Generates code to wrap Java argument into Object.
     * Non-primitive Java types are left unconverted pending convertion
     * in the helper method. Leaves the wrapper object on the top of the stack.
     */
    private static int generateWrapArg(ClassFileWriter cfw, int paramOffset,
                                       Class argType)
    {
        int size = 1;
        if (!argType.isPrimitive()) {
            cfw.add(ByteCode.ALOAD, paramOffset);

        } else if (argType == Boolean.TYPE) {
            // wrap boolean values with java.lang.Boolean.
            cfw.add(ByteCode.NEW, "java/lang/Boolean");
            cfw.add(ByteCode.DUP);
            cfw.add(ByteCode.ILOAD, paramOffset);
            cfw.addInvoke(ByteCode.INVOKESPECIAL, "java/lang/Boolean",
                          "<init>", "(Z)V");

        } else if (argType == Character.TYPE) {
            // Create a string of length 1 using the character parameter.
            cfw.add(ByteCode.ILOAD, paramOffset);
            cfw.addInvoke(ByteCode.INVOKESTATIC, "java/lang/String",
                          "valueOf", "(C)Ljava/lang/String;");

        } else {
            // convert all numeric values to java.lang.Double.
            cfw.add(ByteCode.NEW, "java/lang/Double");
            cfw.add(ByteCode.DUP);
            String typeName = argType.getName();
            switch (typeName.charAt(0)) {
            case 'b':
            case 's':
            case 'i':
                // load an int value, convert to double.
                cfw.add(ByteCode.ILOAD, paramOffset);
                cfw.add(ByteCode.I2D);
                break;
            case 'l':
                // load a long, convert to double.
                cfw.add(ByteCode.LLOAD, paramOffset);
                cfw.add(ByteCode.L2D);
                size = 2;
                break;
            case 'f':
                // load a float, convert to double.
                cfw.add(ByteCode.FLOAD, paramOffset);
                cfw.add(ByteCode.F2D);
                break;
            case 'd':
                cfw.add(ByteCode.DLOAD, paramOffset);
                size = 2;
                break;
            }
            cfw.addInvoke(ByteCode.INVOKESPECIAL, "java/lang/Double",
                          "<init>", "(D)V");
        }
        return size;
    }

    /**
     * Generates code to convert a wrapped value type to a primitive type.
     * Handles unwrapping java.lang.Boolean, and java.lang.Number types.
     * May need to map between char and java.lang.String as well.
     * Generates the appropriate RETURN bytecode.
     */
    private static void generateReturnResult(ClassFileWriter cfw,
                                             Class retType)
    {
        // wrap boolean values with java.lang.Boolean, convert all other
        // primitive values to java.lang.Double.
        if (retType == Void.TYPE) {
            cfw.add(ByteCode.POP);
            cfw.add(ByteCode.RETURN);

        } else if (retType == Boolean.TYPE) {
            cfw.addInvoke(ByteCode.INVOKESTATIC,
                          "org/mozilla/javascript/Context",
                          "toBoolean", "(Ljava/lang/Object;)Z");
            cfw.add(ByteCode.IRETURN);

        } else if (retType == Character.TYPE) {
                // characters are represented as strings in JavaScript.
                // return the first character.
                // first convert the value to a string if possible.
                cfw.addInvoke(ByteCode.INVOKESTATIC,
                              "org/mozilla/javascript/Context",
                              "toString",
                              "(Ljava/lang/Object;)Ljava/lang/String;");
                cfw.add(ByteCode.ICONST_0);
                cfw.addInvoke(ByteCode.INVOKEVIRTUAL, "java/lang/String",
                              "charAt", "(I)C");
                cfw.add(ByteCode.IRETURN);


        } else if (retType.isPrimitive()) {
            cfw.addInvoke(ByteCode.INVOKESTATIC,
                          "org/mozilla/javascript/Context",
                          "toNumber", "(Ljava/lang/Object;)D");
            String typeName = retType.getName();
            switch (typeName.charAt(0)) {
            case 'b':
            case 's':
            case 'i':
                cfw.add(ByteCode.D2I);
                cfw.add(ByteCode.IRETURN);
                break;
            case 'l':
                cfw.add(ByteCode.D2L);
                cfw.add(ByteCode.LRETURN);
                break;
            case 'f':
                cfw.add(ByteCode.D2F);
                cfw.add(ByteCode.FRETURN);
                break;
            case 'd':
                cfw.add(ByteCode.DRETURN);
                break;
            default:
                throw new RuntimeException("Unexpected return type " +
                                           retType.toString());
            }

        } else {
            String retTypeStr = retType.getName();
            cfw.addLoadConstant(retTypeStr);
            cfw.addInvoke(ByteCode.INVOKESTATIC,
                          "java/lang/Class",
                          "forName",
                          "(Ljava/lang/String;)Ljava/lang/Class;");

            cfw.addInvoke(ByteCode.INVOKESTATIC,
                          "org/mozilla/javascript/JavaAdapter",
                          "convertResult",
                          "(Ljava/lang/Object;"
                          +"Ljava/lang/Class;"
                          +")Ljava/lang/Object;");
            // Now cast to return type
            cfw.add(ByteCode.CHECKCAST, retTypeStr.replace('.', '/'));
            cfw.add(ByteCode.ARETURN);
        }
    }

    private static void generateMethod(ClassFileWriter cfw, String genName,
                                       String methodName, Class[] parms,
                                       Class returnType)
    {
        StringBuffer sb = new StringBuffer();
        int firstLocal = appendMethodSignature(parms, returnType, sb);
        String methodSignature = sb.toString();
        cfw.startMethod(methodName, methodSignature,
                        ClassFileWriter.ACC_PUBLIC);

        int FUNCTION = firstLocal;
        int LOCALS_END = firstLocal + 1;

        cfw.add(ByteCode.ALOAD_0);
        cfw.add(ByteCode.GETFIELD, genName, "delegee",
                "Lorg/mozilla/javascript/Scriptable;");
        cfw.addPush(methodName);
        cfw.addInvoke(ByteCode.INVOKESTATIC,
                      "org/mozilla/javascript/JavaAdapter",
                      "getFunction",
                      "(Lorg/mozilla/javascript/Scriptable;"
                      +"Ljava/lang/String;"
                      +")Lorg/mozilla/javascript/Function;");
        cfw.add(ByteCode.DUP);
        cfw.addAStore(FUNCTION);

        // Prepare stack to call calMethod
        // push thisObj
        cfw.add(ByteCode.ALOAD_0);
        cfw.add(ByteCode.GETFIELD, genName, "self",
                "Lorg/mozilla/javascript/Scriptable;");
        // push function
        cfw.addALoad(FUNCTION);

        // push arguments
        generatePushWrappedArgs(cfw, parms, parms.length);

        // push bits to indicate which parameters should be wrapped
        if (parms.length > 64) {
            // If it will be an issue, then passing a static boolean array
            // can be an option, but for now using simple bitmask
            throw Context.reportRuntimeError0(
                "JavaAdapter can not subclass methods with more then"
                +" 64 arguments.");
        }
        long convertionMask = 0;
        for (int i = 0; i != parms.length; ++i) {
            if (!parms[i].isPrimitive()) {
                convertionMask |= (1 << i);
            }
        }
        cfw.addPush(convertionMask);

        // go through utility method, which creates a Context to run the
        // method in.
        cfw.addInvoke(ByteCode.INVOKESTATIC,
                      "org/mozilla/javascript/JavaAdapter",
                      "callMethod",
                      "(Lorg/mozilla/javascript/Scriptable;"
                      +"Lorg/mozilla/javascript/Function;"
                      +"[Ljava/lang/Object;"
                      +"J"
                      +")Ljava/lang/Object;");

        generateReturnResult(cfw, returnType);

        cfw.stopMethod((short)LOCALS_END);
    }

    /**
     * Generates code to push typed parameters onto the operand stack
     * prior to a direct Java method call.
     */
    private static int generatePushParam(ClassFileWriter cfw, int paramOffset,
                                         Class paramType)
    {
        if (!paramType.isPrimitive()) {
            cfw.addALoad(paramOffset);
            return 1;
        }
        String typeName = paramType.getName();
        switch (typeName.charAt(0)) {
        case 'z':
        case 'b':
        case 'c':
        case 's':
        case 'i':
            // load an int value, convert to double.
            cfw.addILoad(paramOffset);
            return 1;
        case 'l':
            // load a long, convert to double.
            cfw.addLLoad(paramOffset);
            return 2;
        case 'f':
            // load a float, convert to double.
            cfw.addFLoad(paramOffset);
            return 1;
        case 'd':
            cfw.addDLoad(paramOffset);
            return 2;
        }
        throw Kit.codeBug();
    }

    /**
     * Generates code to return a Java type, after calling a Java method
     * that returns the same type.
     * Generates the appropriate RETURN bytecode.
     */
    private static void generatePopResult(ClassFileWriter cfw,
                                          Class retType)
    {
        if (retType.isPrimitive()) {
            String typeName = retType.getName();
            switch (typeName.charAt(0)) {
            case 'b':
            case 'c':
            case 's':
            case 'i':
            case 'z':
                cfw.add(ByteCode.IRETURN);
                break;
            case 'l':
                cfw.add(ByteCode.LRETURN);
                break;
            case 'f':
                cfw.add(ByteCode.FRETURN);
                break;
            case 'd':
                cfw.add(ByteCode.DRETURN);
                break;
            }
        } else {
            cfw.add(ByteCode.ARETURN);
        }
    }

    /**
     * Generates a method called "super$methodName()" which can be called
     * from JavaScript that is equivalent to calling "super.methodName()"
     * from Java. Eventually, this may be supported directly in JavaScript.
     */
    private static void generateSuper(ClassFileWriter cfw,
                                      String genName, String superName,
                                      String methodName, String methodSignature,
                                      Class[] parms, Class returnType)
    {
        cfw.startMethod("super$" + methodName, methodSignature,
                        ClassFileWriter.ACC_PUBLIC);

        // push "this"
        cfw.add(ByteCode.ALOAD, 0);

        // push the rest of the parameters.
        int paramOffset = 1;
        for (int i = 0; i < parms.length; i++) {
            paramOffset += generatePushParam(cfw, paramOffset, parms[i]);
        }

        // call the superclass implementation of the method.
        cfw.addInvoke(ByteCode.INVOKESPECIAL,
                      superName,
                      methodName,
                      methodSignature);

        // now, handle the return type appropriately.
        Class retType = returnType;
        if (!retType.equals(Void.TYPE)) {
            generatePopResult(cfw, retType);
        } else {
            cfw.add(ByteCode.RETURN);
        }
        cfw.stopMethod((short)(paramOffset + 1));
    }

    /**
     * Returns a fully qualified method name concatenated with its signature.
     */
    private static String getMethodSignature(Method method, Class[] argTypes)
    {
        StringBuffer sb = new StringBuffer();
        appendMethodSignature(argTypes, method.getReturnType(), sb);
        return sb.toString();
    }

    private static int appendMethodSignature(Class[] argTypes,
                                             Class returnType,
                                             StringBuffer sb)
    {
        sb.append('(');
        int firstLocal = 1 + argTypes.length; // includes this.
        for (int i = 0; i < argTypes.length; i++) {
            Class type = argTypes[i];
            appendTypeString(sb, type);
            if (type == Long.TYPE || type == Double.TYPE) {
                // adjust for duble slot
                ++firstLocal;
            }
        }
        sb.append(')');
        appendTypeString(sb, returnType);
        return firstLocal;
    }

    private static StringBuffer appendTypeString(StringBuffer sb, Class type)
    {
        while (type.isArray()) {
            sb.append('[');
            type = type.getComponentType();
        }
        if (type.isPrimitive()) {
            char typeLetter;
            if (type == Boolean.TYPE) {
                typeLetter = 'Z';
            } else if (type == Long.TYPE) {
                typeLetter = 'J';
            } else {
                String typeName = type.getName();
                typeLetter = Character.toUpperCase(typeName.charAt(0));
            }
            sb.append(typeLetter);
        } else {
            sb.append('L');
            sb.append(type.getName().replace('.', '/'));
            sb.append(';');
        }
        return sb;
    }

    private static int[] getArgsToConvert(Class[] argTypes)
    {
        int count = 0;
        for (int i = 0; i != argTypes.length; ++i) {
            if (!argTypes[i].isPrimitive())
                ++count;
        }
        if (count == 0)
            return null;
        int[] array = new int[count];
        count = 0;
        for (int i = 0; i != argTypes.length; ++i) {
            if (!argTypes[i].isPrimitive())
                array[count++] = i;
        }
        return array;
    }

    private static final Object FTAG = new Object();
    private static final int Id_JavaAdapter = 1;
}
