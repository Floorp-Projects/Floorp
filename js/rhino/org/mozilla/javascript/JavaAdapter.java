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
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Patrick Beard
 * Norris Boyd
 * Mike McCabe
 * Matthias Radestock
 * Andi Vajda
 * Andrew Wason
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

public class JavaAdapter extends ScriptableObject {
    public boolean equals(Object obj) {
    	return super.equals(obj);
    }
    
    public String getClassName() {
        return "JavaAdapter";
    }

    public static Object convertResult(Object result, String classname)
        throws ClassNotFoundException 
    {
        Class c = ScriptRuntime.loadClassName(classname);
        if (result == Undefined.instance && 
            (c != ScriptRuntime.ObjectClass && 
             c != ScriptRuntime.StringClass))
        {
            // Avoid an error for an undefined value; return null instead.
            return null;
        }
        return NativeJavaObject.coerceType(c, result);
    }

    public static Scriptable setAdapterProto(Scriptable obj, Object adapter) {
        Scriptable res = ScriptRuntime.toObject(ScriptableObject.getTopLevelScope(obj),adapter);
        res.setPrototype(obj);
        return res;
    }

    public static Object getAdapterSelf(Class adapterClass, Object adapter)
        throws NoSuchFieldException, IllegalAccessException
    {
        Field self = adapterClass.getDeclaredField("self");
        return self.get(adapter);
    }

    public static Object js_JavaAdapter(Context cx, Object[] args, 
                                        Function ctorObj, boolean inNewExpr)
        throws InstantiationException, NoSuchMethodException, 
               IllegalAccessException, InvocationTargetException,
               ClassNotFoundException, NoSuchFieldException
    {
        Class superClass = Object.class;
        Class[] intfs = new Class[args.length-1];
        int interfaceCount = 0;
        for (int i=0; i < args.length-1; i++) {
            if (!(args[i] instanceof NativeJavaClass)) {
                // TODO: report error
                throw new RuntimeException("expected java class object");
            }
            Class c = ((NativeJavaClass) args[i]).getClassObject();
            if (!c.isInterface()) {
                superClass = c;
                break;
            }
            intfs[interfaceCount++] = c;
        }
        
        Class[] interfaces = new Class[interfaceCount];
        System.arraycopy(intfs, 0, interfaces, 0, interfaceCount);
        Scriptable obj = (Scriptable) args[args.length - 1];
        
        ClassSignature sig = new ClassSignature(superClass, interfaces, obj);
        Class adapterClass = (Class) generatedClasses.get(sig);
        if (adapterClass == null) {
            String adapterName = "adapter" + serial++;
            adapterClass = createAdapterClass(cx, obj, adapterName, 
                                              superClass, interfaces, 
                                              null, null);
            generatedClasses.put(sig, adapterClass);
        }
        
        Class[] ctorParms = { Scriptable.class };
        Object[] ctorArgs = { obj };
        Object adapter = adapterClass.getConstructor(ctorParms).newInstance(ctorArgs);
        return getAdapterSelf(adapterClass, adapter);
    }

    public static Class createAdapterClass(Context cx, Scriptable jsObj,
                                           String adapterName, Class superClass, 
                                           Class[] interfaces, 
                                           String scriptClassName,
                                           ClassNameHelper nameHelper)
        throws ClassNotFoundException
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
        if (scriptClassName != null)
            generateEmptyCtor(cfw, adapterName, superName, scriptClassName);
        
        Hashtable generatedOverrides = new Hashtable();
        Hashtable generatedMethods = new Hashtable();
        
        // generate methods to satisfy all specified interfaces.
        for (int i = 0; i < interfacesCount; i++) {
            Method[] methods = interfaces[i].getMethods();
            for (int j = 0; j < methods.length; j++) {
            	Method method = methods[j];
                int mods = method.getModifiers();
                if (Modifier.isStatic(mods) || Modifier.isFinal(mods))
                    continue;
                // make sure to generate only one instance of a particular 
                // method/signature.
                String methodName = method.getName();
            	String methodKey = methodName + getMethodSignature(method);
            	if (! generatedOverrides.containsKey(methodKey)) {
                    generateMethod(cfw, adapterName, methodName,
                                   method.getParameterTypes(),
                                   method.getReturnType());
                    generatedOverrides.put(methodKey, Boolean.TRUE);
                    generatedMethods.put(methodName, Boolean.TRUE);
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
            if (isAbstractMethod || 
                (jsObj != null && ScriptableObject.getProperty(jsObj,method.getName()) != Scriptable.NOT_FOUND))
            {
                // make sure to generate only one instance of a particular 
                // method/signature.
                String methodName = method.getName();
                String methodSignature = getMethodSignature(method);
                String methodKey = methodName + methodSignature;
                if (! generatedOverrides.containsKey(methodKey)) {
                    generateMethod(cfw, adapterName, methodName,
                                   method.getParameterTypes(),
                                   method.getReturnType());
                    generatedOverrides.put(methodKey, Boolean.TRUE);
                    generatedMethods.put(methodName, Boolean.TRUE);
                }
                // if a method was overridden, generate a "super$method"
                // which lets the delegate call the superclass' version.
                if (!isAbstractMethod) {
                    generateSuper(cfw, adapterName, superName,
                                  methodName, methodSignature,
                                  method.getParameterTypes(),
                                  method.getReturnType());
                }
            }
        }
        
        // Generate Java methods, fields for remaining properties that
        // are not overrides.
        for (Scriptable o = jsObj; o != null; o = (Scriptable)o.getPrototype()) {
            Object[] ids = jsObj.getIds();
            for (int j=0; j < ids.length; j++) {
                if (!(ids[j] instanceof String)) 
                    continue;
                String id = (String) ids[j];
                if (generatedMethods.containsKey(id))
                    continue;
                Object f = o.get(id, o);
                int length;
                if (f instanceof Scriptable) {
                    Scriptable p = (Scriptable) f;
                    if (!(p instanceof Function))
                        continue;
                    length = (int) Context.toNumber(
                                ScriptableObject.getProperty(p, "length"));
                } else if (f instanceof FunctionNode) {
                    length = ((FunctionNode) f).getVariableTable()
                                .getParameterCount();
                } else {
                    continue;
                }
                Class[] parms = new Class[length];
                for (int k=0; k < length; k++) 
                    parms[k] = Object.class;
                generateMethod(cfw, adapterName, id, parms, Object.class);
            }  
        }
        ByteArrayOutputStream out = new ByteArrayOutputStream(512);
        try {
            cfw.write(out);
        }
        catch (IOException ioe) {
            throw new RuntimeException("unexpected IOException");
        }
        byte[] bytes = out.toByteArray();
        
        if (nameHelper != null)
        {
            if (nameHelper.getGeneratingDirectory() != null) {
                try {
                    int lastDot = adapterName.lastIndexOf('.');
                    if (lastDot != -1)
                        adapterName = adapterName.substring(lastDot+1);
                    String filename = nameHelper.getTargetClassFileName(adapterName);
                    FileOutputStream file = new FileOutputStream(filename);
                    file.write(bytes);
                    file.close();
                }
                catch (IOException iox) {
                    throw WrappedException.wrapException(iox);
                }
                return null;
            } else {
                try {
                    ClassOutput classOutput = nameHelper.getClassOutput();

                    if (classOutput != null) {
                        OutputStream cOut =
                            classOutput.getOutputStream(adapterName);

                        cOut.write(bytes);
                        cOut.close();
                    }
                } catch (IOException iox) {
                    throw WrappedException.wrapException(iox);
                }
            }
        }
            
        SecuritySupport ss = cx.getSecuritySupport();
        if (ss != null)  {
            Object securityDomain = cx.getSecurityDomainForStackDepth(-1);
            Class result = ss.defineClass(adapterName, bytes, securityDomain);
            if (result != null)
                return result;
        } 
        if (classLoader == null)
            classLoader = new MyClassLoader();
        classLoader.defineClass(adapterName, bytes);
        return classLoader.loadClass(adapterName, true);
    }
    
    /**
     * Utility method which dynamically binds a Context to the current thread, 
     * if none already exists.
     */
    public static Object callMethod(Scriptable object, Object thisObj,
                                    String methodId, Object[] args) 
    {
        try {
            Context cx = Context.enter();
            Object fun = ScriptableObject.getProperty(object,methodId);
            if (fun == Scriptable.NOT_FOUND) {
                // It is an error that this function doesn't exist.
                // Easiest thing to do is just pass the undefined value 
                // along and ScriptRuntime will take care of reporting 
                // the error.
                fun = Undefined.instance;
            }
            return ScriptRuntime.call(cx, fun, thisObj, args, object);
        } catch (JavaScriptException ex) {
            throw WrappedException.wrapException(ex);
        } finally {
            Context.exit();
        }
    }
    
    public static Scriptable toObject(Object value, Scriptable scope,
                                      Class staticType)
    {
        Context.enter();
        try {
            return Context.toObject(value, scope, staticType);
        } finally {
            Context.exit();
        }
    }
    
    private static void generateCtor(ClassFileWriter cfw, String adapterName, 
                                     String superName) 
    {
        cfw.startMethod("<init>", 
                        "(Lorg/mozilla/javascript/Scriptable;)V",
                        ClassFileWriter.ACC_PUBLIC);
        
        // Invoke base class constructor
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.add(ByteCode.INVOKESPECIAL, superName, "<init>", "()", "V");
        
        // Save parameter in instance variable "delegee"
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.add(ByteCode.ALOAD_1);  // first arg
        cfw.add(ByteCode.PUTFIELD, adapterName, "delegee", 
                "Lorg/mozilla/javascript/Scriptable;");

        // create a wrapper object to be used as "this" in method calls
        cfw.add(ByteCode.ALOAD_1);  // the Scriptable
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.add(ByteCode.INVOKESTATIC,
                "org/mozilla/javascript/JavaAdapter",
                "setAdapterProto",
                "(Lorg/mozilla/javascript/Scriptable;" +
                 "Ljava/lang/Object;)",
                "Lorg/mozilla/javascript/Scriptable;");
        
        // save the wrapper
        cfw.add(ByteCode.ASTORE_1);
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.add(ByteCode.ALOAD_1);  // first arg
        cfw.add(ByteCode.PUTFIELD, adapterName, "self", 
                "Lorg/mozilla/javascript/Scriptable;");

        cfw.add(ByteCode.RETURN);
        cfw.stopMethod((short)20, null); // TODO: magic number "20"
    }

    private static void generateEmptyCtor(ClassFileWriter cfw, String adapterName, 
                                          String superName, String scriptClassName) 
    {
        cfw.startMethod("<init>", "()V", ClassFileWriter.ACC_PUBLIC);
        
        // Invoke base class constructor
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.add(ByteCode.INVOKESPECIAL, superName, "<init>", "()", "V");
        
        // Load script class
        cfw.add(ByteCode.NEW, scriptClassName);
        cfw.add(ByteCode.DUP);
        cfw.add(ByteCode.INVOKESPECIAL, scriptClassName, "<init>", "()", "V");
        
        // Run script and save resulting scope
        cfw.add(ByteCode.INVOKESTATIC,
                "org/mozilla/javascript/ScriptRuntime",
                "runScript",
                "(Lorg/mozilla/javascript/Script;)",
                "Lorg/mozilla/javascript/Scriptable;");
        cfw.add(ByteCode.ASTORE_1);
                
        // Save the Scriptable in instance variable "delegee"
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.add(ByteCode.ALOAD_1);  // the Scriptable
        cfw.add(ByteCode.PUTFIELD, adapterName, "delegee", 
                "Lorg/mozilla/javascript/Scriptable;");
        
        // create a wrapper object to be used as "this" in method calls
        cfw.add(ByteCode.ALOAD_1);  // the Scriptable
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.add(ByteCode.INVOKESTATIC,
                "org/mozilla/javascript/JavaAdapter",
                "setAdapterProto",
                "(Lorg/mozilla/javascript/Scriptable;" +
                 "Ljava/lang/Object;)",
                "Lorg/mozilla/javascript/Scriptable;");
        //save the wrapper
        cfw.add(ByteCode.ASTORE_1);
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.add(ByteCode.ALOAD_1);  // first arg
        cfw.add(ByteCode.PUTFIELD, adapterName, "self", 
                "Lorg/mozilla/javascript/Scriptable;");

        cfw.add(ByteCode.RETURN);
        cfw.stopMethod((short)20, null); // TODO: magic number "20"
    }

    /**
     * Generates code to create a java.lang.Boolean, java.lang.Character or a 
     * java.lang.Double to wrap the specified primitive parameter. Leaves the 
     * wrapper object on the top of the stack.
     */
    private static int generateWrapParam(ClassFileWriter cfw, int paramOffset, 
                                         Class paramType) 
    {
        if (paramType.equals(Boolean.TYPE)) {
            // wrap boolean values with java.lang.Boolean.
            cfw.add(ByteCode.NEW, "java/lang/Boolean");
            cfw.add(ByteCode.DUP);
            cfw.add(ByteCode.ILOAD, paramOffset++);
            cfw.add(ByteCode.INVOKESPECIAL, "java/lang/Boolean", 
                    "<init>", "(Z)", "V");
        } else
            if (paramType.equals(Character.TYPE)) {
                // Create a string of length 1 using the character parameter.
                cfw.add(ByteCode.NEW, "java/lang/String");
                cfw.add(ByteCode.DUP);
                cfw.add(ByteCode.ICONST_1);       
                cfw.add(ByteCode.NEWARRAY, ByteCode.T_CHAR);
                cfw.add(ByteCode.DUP);
                cfw.add(ByteCode.ICONST_0);
                cfw.add(ByteCode.ILOAD, paramOffset++);
                cfw.add(ByteCode.CASTORE);                
                cfw.add(ByteCode.INVOKESPECIAL, "java/lang/String", 
                        "<init>", "([C)", "V");
            } else {
                // convert all numeric values to java.lang.Double.
                cfw.add(ByteCode.NEW, "java/lang/Double");
                cfw.add(ByteCode.DUP);
                String typeName = paramType.getName();
                switch (typeName.charAt(0)) {
                case 'b':
                case 's':
                case 'i':
                    // load an int value, convert to double.
                    cfw.add(ByteCode.ILOAD, paramOffset++);
                    cfw.add(ByteCode.I2D);
                    break;
                case 'l':
                    // load a long, convert to double.
                    cfw.add(ByteCode.LLOAD, paramOffset);
                    cfw.add(ByteCode.L2D);
                    paramOffset += 2;
                    break;
                case 'f':
                    // load a float, convert to double.
                    cfw.add(ByteCode.FLOAD, paramOffset++);
                    cfw.add(ByteCode.F2D);
                    break;
                case 'd':
                    cfw.add(ByteCode.DLOAD, paramOffset);
                    paramOffset += 2;
                    break;
                }
                cfw.add(ByteCode.INVOKESPECIAL, "java/lang/Double", 
                        "<init>", "(D)", "V");
            }
        return paramOffset;
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
        if (retType.equals(Boolean.TYPE)) {
            cfw.add(ByteCode.INVOKESTATIC, 
                    "org/mozilla/javascript/Context", 
                    "toBoolean", "(Ljava/lang/Object;)", 
                    "Z");
            cfw.add(ByteCode.IRETURN);
        } else if (retType.equals(Character.TYPE)) {
                // characters are represented as strings in JavaScript. 
                // return the first character.
                // first convert the value to a string if possible.
                cfw.add(ByteCode.INVOKESTATIC, 
                        "org/mozilla/javascript/Context", 
                        "toString", "(Ljava/lang/Object;)", 
                        "Ljava/lang/String;");
                cfw.add(ByteCode.ICONST_0);
                cfw.add(ByteCode.INVOKEVIRTUAL, "java/lang/String", "charAt", 
                        "(I)", "C");
                cfw.add(ByteCode.IRETURN);
        } else if (retType.isPrimitive()) {
            cfw.add(ByteCode.INVOKESTATIC, 
                    "org/mozilla/javascript/Context", 
                    "toNumber", "(Ljava/lang/Object;)", 
                    "D");
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
            cfw.add(ByteCode.INVOKESTATIC, 
                    "org/mozilla/javascript/JavaAdapter", 
                    "convertResult",
                    "(Ljava/lang/Object;" +
                    "Ljava/lang/String;)", 
                    "Ljava/lang/Object;");
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
        sb.append('(');
        short arrayLocal = 1;	// includes this.
        for (int i = 0; i < parms.length; i++) {
            Class type = parms[i];
            appendTypeString(sb, type);
            if (type.equals(Long.TYPE) || type.equals(Double.TYPE))
                arrayLocal += 2;
            else
                arrayLocal += 1;
        }
        sb.append(')');
        appendTypeString(sb, returnType);
        String methodSignature = sb.toString();
        // System.out.println("generating " + m.getName() + methodSignature);
        // System.out.flush();
        cfw.startMethod(methodName, methodSignature, 
                        ClassFileWriter.ACC_PUBLIC);
        cfw.add(ByteCode.BIPUSH, (byte) parms.length);  // > 255 parms?
        cfw.add(ByteCode.ANEWARRAY, "java/lang/Object");
        cfw.add(ByteCode.ASTORE, arrayLocal);
        
        // allocate a local variable to store the scope used to wrap native objects.
        short scopeLocal = (short) (arrayLocal + 1);
        boolean loadedScope = false;

        int paramOffset = 1;
        for (int i = 0; i < parms.length; i++) {
            cfw.add(ByteCode.ALOAD, arrayLocal);
            cfw.add(ByteCode.BIPUSH, i);
            if (parms[i].isPrimitive()) {
                paramOffset = generateWrapParam(cfw, paramOffset, parms[i]);
            } else {
                // An arbitary Java object; call Context.toObject to wrap in 
                // a Scriptable object
                cfw.add(ByteCode.ALOAD, paramOffset++);
                if (! loadedScope) {
                    // load this.self into a local the first time it's needed.
                    // it will provide the scope needed by Context.toObject().
                    cfw.add(ByteCode.ALOAD_0);
                    cfw.add(ByteCode.GETFIELD, genName, "delegee", 
                            "Lorg/mozilla/javascript/Scriptable;");
                    cfw.add(ByteCode.ASTORE, scopeLocal);
                    loadedScope = true;
                }
                cfw.add(ByteCode.ALOAD, scopeLocal);

                // Get argument Class 
                cfw.addLoadConstant(parms[i].getName());
                cfw.add(ByteCode.INVOKESTATIC, 
                        "org/mozilla/javascript/ScriptRuntime", 
                        "loadClassName", 
                        "(Ljava/lang/String;)",                        
                        "Ljava/lang/Class;");
      
                cfw.add(ByteCode.INVOKESTATIC, 
                        "org/mozilla/javascript/JavaAdapter", 
                        "toObject", 
                        "(Ljava/lang/Object;" +
                         "Lorg/mozilla/javascript/Scriptable;" +
                         "Ljava/lang/Class;)",                         
                        "Lorg/mozilla/javascript/Scriptable;");
            }
            cfw.add(ByteCode.AASTORE);
        }
        
        cfw.add(ByteCode.ALOAD_0);
        cfw.add(ByteCode.GETFIELD, genName, "delegee", 
                "Lorg/mozilla/javascript/Scriptable;");
        cfw.add(ByteCode.ALOAD_0);
        cfw.add(ByteCode.GETFIELD, genName, "self", 
                "Lorg/mozilla/javascript/Scriptable;");
        cfw.addLoadConstant(methodName);
        cfw.add(ByteCode.ALOAD, arrayLocal);
        
        // go through utility method, which creates a Context to run the 
        // method in.
        cfw.add(ByteCode.INVOKESTATIC,
                "org/mozilla/javascript/JavaAdapter",
                "callMethod",
                "(Lorg/mozilla/javascript/Scriptable;" +
                 "Ljava/lang/Object;Ljava/lang/String;[Ljava/lang/Object;)",
                "Ljava/lang/Object;");

        if (returnType.equals(Void.TYPE)) {
            cfw.add(ByteCode.POP);
            cfw.add(ByteCode.RETURN);
        } else {
            generateReturnResult(cfw, returnType);
        }
        cfw.stopMethod((short)(scopeLocal + 3), null);
    }

    /**
     * Generates code to push typed parameters onto the operand stack
     * prior to a direct Java method call.
     */
    private static int generatePushParam(ClassFileWriter cfw, int paramOffset, 
                                         Class paramType) 
    {
        String typeName = paramType.getName();
        switch (typeName.charAt(0)) {
        case 'z':
        case 'b':
        case 'c':
        case 's':
        case 'i':
            // load an int value, convert to double.
            cfw.add(ByteCode.ILOAD, paramOffset++);
            break;
        case 'l':
            // load a long, convert to double.
            cfw.add(ByteCode.LLOAD, paramOffset);
            paramOffset += 2;
            break;
        case 'f':
            // load a float, convert to double.
            cfw.add(ByteCode.FLOAD, paramOffset++);
            break;
        case 'd':
            cfw.add(ByteCode.DLOAD, paramOffset);
            paramOffset += 2;
            break;
        }
        return paramOffset;
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
            if (parms[i].isPrimitive()) {
                paramOffset = generatePushParam(cfw, paramOffset, parms[i]);
            } else {
                cfw.add(ByteCode.ALOAD, paramOffset++);
            }
        }
        
        // split the method signature at the right parentheses.
        int rightParen = methodSignature.indexOf(')');
        
        // call the superclass implementation of the method.
        cfw.add(ByteCode.INVOKESPECIAL,
                superName,
                methodName,
                methodSignature.substring(0, rightParen + 1),
                methodSignature.substring(rightParen + 1));

		// now, handle the return type appropriately.        
        Class retType = returnType;
        if (!retType.equals(Void.TYPE)) {
            generatePopResult(cfw, retType);
        } else {
        	cfw.add(ByteCode.RETURN);
        }
        cfw.stopMethod((short)(paramOffset + 1), null);
    }
    
    /**
     * Returns a fully qualified method name concatenated with its signature.
     */
    private static String getMethodSignature(Method method) {
        Class[] parms = method.getParameterTypes();
        StringBuffer sb = new StringBuffer();
        sb.append('(');
        for (int i = 0; i < parms.length; i++) {
            Class type = parms[i];
            appendTypeString(sb, type);
        }
        sb.append(')');
        appendTypeString(sb, method.getReturnType());
        return sb.toString();
    }
    
    private static StringBuffer appendTypeString(StringBuffer sb, Class type) 
    {
        while (type.isArray()) {
            sb.append('[');
            type = type.getComponentType();
        }
        if (type.isPrimitive()) {
            if (type.equals(Boolean.TYPE)) {
                sb.append('Z');
            } else
                if (type.equals(Long.TYPE)) {
                    sb.append('J');
                } else {
                    String typeName = type.getName();
                    sb.append(Character.toUpperCase(typeName.charAt(0)));
                }
        } else {
            sb.append('L');
            sb.append(type.getName().replace('.', '/'));
            sb.append(';');
        }
        return sb;
    }
    
    static final class MyClassLoader extends ClassLoader {
        public Class defineClass(String name, byte data[]) {
            return super.defineClass(name, data, 0, data.length);
        }

        protected Class loadClass(String name, boolean resolve)
            throws ClassNotFoundException
        {
            Class clazz = findLoadedClass(name);
            if (clazz == null) {
                ClassLoader loader = getClass().getClassLoader();
                try {
                    if (loader != null)
                        return loader.loadClass(name);
                    clazz = findSystemClass(name);
                } catch (ClassNotFoundException e) {
                    return ScriptRuntime.loadClassName(name);
                }
            }
            if (resolve)
                resolveClass(clazz);
            return clazz;
        }
    }

    /**
     * Provides a key with which to distinguish previously generated
     * adapter classes stored in a hash table.
     */
    static class ClassSignature {
        Class mSuperClass;
        Class[] mInterfaces;
        Object[] mProperties;    // JDK1.2: Use HashSet
    
        ClassSignature(Class superClass, Class[] interfaces, Scriptable jsObj) {
            mSuperClass = superClass;
            mInterfaces = interfaces;
            mProperties = ScriptableObject.getPropertyIds(jsObj);
        }
            
        public boolean equals(Object obj) {
            if (obj instanceof ClassSignature) {
                ClassSignature sig = (ClassSignature) obj;
                if (mSuperClass == sig.mSuperClass) {
                    Class[] interfaces = sig.mInterfaces;
                    if (mInterfaces != interfaces) {
                        if (mInterfaces == null || interfaces == null)
                            return false;
                        if (mInterfaces.length != interfaces.length)
                            return false;
                        for (int i=0; i < interfaces.length; i++)
                            if (mInterfaces[i] != interfaces[i])
                                return false;
                    }
                    if (mProperties.length != sig.mProperties.length)
                        return false;
                    for (int i=0; i < mProperties.length; i++) {
                        if (!mProperties[i].equals(sig.mProperties[i]))
                            return false;
                    }
                    return true;
                }
            }
            return false;
        }
            
        public int hashCode() {
            return mSuperClass.hashCode();
        }
    }
    
    private static int serial;
    private static MyClassLoader classLoader;
    private static Hashtable generatedClasses = new Hashtable(7);
}
