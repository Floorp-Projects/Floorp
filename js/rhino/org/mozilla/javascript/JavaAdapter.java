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

import org.mozilla.classfile.*;
import java.lang.reflect.*;
import java.io.*;
import java.util.*;

public class JavaAdapter extends ScriptableObject {
    
    public String getClassName() {
        return "JavaAdapter";
    }
    
    public static Object js_JavaAdapter(Context cx, Object[] args, 
                                        Function ctorObj, boolean inNewExpr)
        throws InstantiationException, NoSuchMethodException, 
               IllegalAccessException, InvocationTargetException,
               ClassNotFoundException
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
        StringBuffer sb = new StringBuffer("adapter");
        sb.append(serial++);
        Class[] interfaces = new Class[interfaceCount];
        System.arraycopy(intfs, 0, interfaces, 0, interfaceCount);
        Scriptable obj = (Scriptable) args[args.length - 1];
        Class adapterClass = createAdapterClass(cx, obj, sb.toString(), 
                                                superClass, interfaces, 
                                                null, null);
        Class[] ctorParms = { FlattenedObject.class };
        Object[] ctorArgs = { new FlattenedObject(obj) };
        Object v = adapterClass.getConstructor(ctorParms).newInstance(ctorArgs);
        return cx.toObject(v, ScriptableObject.getTopLevelScope(ctorObj));                           
    }
    
    public static Class createAdapterClass(Context cx, Scriptable jsObj,
                                           String name, Class superClass, 
                                           Class[] interfaces, 
                                           String scriptClassName,
                                           ClassNameHelper nameHelper)
        throws ClassNotFoundException
    {
        ClassFileWriter cfw = new ClassFileWriter(name, 
                                                  superClass.getName(), 
                                                  "<adapter>");
        cfw.addField("o", "Lorg/mozilla/javascript/FlattenedObject;",
                     ClassFileWriter.ACC_PRIVATE);
        cfw.addField("self", "Lorg/mozilla/javascript/Scriptable;",
                     (short) (ClassFileWriter.ACC_PUBLIC | 
                              ClassFileWriter.ACC_FINAL));
        int interfacesCount = interfaces == null ? 0 : interfaces.length;
        for (int i=0; i < interfacesCount; i++) {
            if (interfaces[i] != null)
                cfw.addInterface(interfaces[i].getName());
        }
        
        generateCtor(cfw, name, superClass);
        if (scriptClassName != null)
            generateEmptyCtor(cfw, name, superClass, scriptClassName);
        
        Hashtable generatedOverrides = new Hashtable();
        Hashtable generatedMethods = new Hashtable();
        
        // if abstract class was specified, then generate appropriate overrides.
        for (int i = 0; i < interfacesCount; i++) {
            Method[] methods = interfaces[i].getMethods();
            for (int j = 0; j < methods.length; j++) {
            	Method method = methods[j];
                int mods = method.getModifiers();
                if (Modifier.isStatic(mods) || Modifier.isFinal(mods))
                    continue;
                // make sure to generate only one instance of a particular 
                // method/signature.
            	String methodKey = getMethodSignature(method);
            	if (! generatedOverrides.containsKey(methodKey)) {
                    Class[] parms = method.getParameterTypes();
                    generateMethod(cfw, name, method.getName(), parms, 
                                   method.getReturnType());
                    generatedOverrides.put(methodKey, methodKey);
                    generatedMethods.put(method.getName(), Boolean.TRUE);
                }
            }
        }

        // Now, go through the superclasses methods, checking for abstract 
        // methods or additional methods to override.
        FlattenedObject obj = jsObj != null 
                              ? new FlattenedObject(jsObj) 
                              : null;

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
            if (Modifier.isAbstract(mods) || 
                (obj != null && obj.hasProperty(method.getName())))
            {
                // make sure to generate only one instance of a particular 
                // method/signature.
                String methodKey = getMethodSignature(method);
                if (! generatedOverrides.containsKey(methodKey)) {
                    Class[] parms = method.getParameterTypes();
                    generateMethod(cfw, name, method.getName(), parms, 
                                   method.getReturnType());
                    generatedOverrides.put(methodKey, method);
                    generatedMethods.put(method.getName(), Boolean.TRUE);
                }
            }
        }
        
        // Generate Java methods, fields for remaining properties that
        // are not overrides.
        Object[] ids = obj.getIds();
        for (int j=0; j < ids.length; j++) {
            if (!(ids[j] instanceof String)) 
                continue;
            if (generatedMethods.containsKey((String) ids[j]))
                continue;
            Object f = obj.getProperty(ids[j]);
            int length;
            if (f instanceof FlattenedObject) {
                Object p = ((FlattenedObject) f).getObject();
                if (!(p instanceof Function))
                    continue;
                length = (int) Context.toNumber(((FlattenedObject) f).getProperty("length"));
            } else if (f instanceof FunctionNode) {
                length = ((FunctionNode) f).getVariableTable().getParameterCount();
            } else {
                continue;
            }
            Class[] parms = new Class[length];
            for (int k=0; k < length; k++) 
                parms[k] = Object.class;
            generateMethod(cfw, name, (String) ids[j], parms, Object.class);
        }  
        
        ByteArrayOutputStream out = new ByteArrayOutputStream(512);
        try {
            cfw.write(out);
        }
        catch (IOException ioe) {
            throw new RuntimeException("unexpected IOException");
        }
        byte[] bytes = out.toByteArray();
        
        if (nameHelper != null && nameHelper.getGeneratingDirectory() != null) 
        {
            try {
                int lastDot = name.lastIndexOf('.');
                if (lastDot != -1)
                    name = name.substring(lastDot+1);
                String filename = nameHelper.getTargetClassFileName(name);
                FileOutputStream file = new FileOutputStream(filename);
                file.write(bytes);
                file.close();
            }
            catch (IOException iox) {
                throw WrappedException.wrapException(iox);
            }
            return null;
        }
            
        SecuritySupport ss = cx.getSecuritySupport();
        if (ss != null)  {
            Object securityDomain = cx.getSecurityDomainForStackDepth(-1);
            return ss.defineClass(name, bytes, securityDomain);
        } else {
            if (classLoader == null)
                classLoader = new MyClassLoader();
            classLoader.defineClass(name, bytes);
            return classLoader.loadClass(name, true);
        }
    }

    /**
     * Utility method, which dynamically binds a Context to the current thread, 
     * if none already exists.
     */
    public static Object callMethod(FlattenedObject object, Object methodId, 
                                    Object[] args) 
    {
        Context cx = Context.enter();
        try {
            if (object.hasProperty(methodId))
                return object.callMethod(methodId, args);
        } catch (PropertyException ex) {
            // shouldn't occur
        } catch (NotAFunctionException ex) {
              // TODO: could occur
        } catch (JavaScriptException ex) {
              // TODO: could occur
            /*
        } catch (Exception ex) {
            // TODO: wouldn't it be better to let the exception propagate
            // up so that it could be dealt with by the calling code?
            ex.printStackTrace(System.err);
            throw new Error(ex.getMessage());
            */
        } finally {	
            Context.exit();
        }
        return Context.getUndefinedValue();
    }
    
    private static void generateCtor(ClassFileWriter cfw, String name, 
                                     Class superClass) 
    {
        cfw.startMethod("<init>", 
                        "(Lorg/mozilla/javascript/FlattenedObject;)V",
                        ClassFileWriter.ACC_PUBLIC);
        
        // Invoke base class constructor
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.add(ByteCode.INVOKESPECIAL,
                superClass.getName().replace('.', '/'), 
                "<init>", "()", "V");
        
        // Set the prototype of the js object to be a LiveConnect 
        // wapper of the generated class's object
        cfw.add(ByteCode.ALOAD_1);  // first arg
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.add(ByteCode.INVOKESTATIC,
                "org/mozilla/javascript/ScriptRuntime",
                "setAdapterProto",
                "(Lorg/mozilla/javascript/FlattenedObject;" +
                 "Ljava/lang/Object;)",
                "V");
        
        // Save parameter in instance variable
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.add(ByteCode.ALOAD_1);  // first arg
        cfw.add(ByteCode.PUTFIELD, name, "o", 
                "Lorg/mozilla/javascript/FlattenedObject;");

        // Store Scriptable object in "self", a public instance variable, 
        //  so scripts can read it.
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.add(ByteCode.ALOAD_1);  // first arg
        cfw.add(ByteCode.INVOKEVIRTUAL,
                "org/mozilla/javascript/FlattenedObject",
                "getObject", "()",
                "Lorg/mozilla/javascript/Scriptable;");
        cfw.add(ByteCode.PUTFIELD, name, "self",
                "Lorg/mozilla/javascript/Scriptable;");		
        
        cfw.add(ByteCode.RETURN);
        cfw.stopMethod((short)20, null); // TODO: magic number "20"
    }

    private static void generateEmptyCtor(ClassFileWriter cfw, String name, 
                                          Class superClass, 
                                          String scriptClassName) 
    {
        cfw.startMethod("<init>", "()V", ClassFileWriter.ACC_PUBLIC);
        
        // Invoke base class constructor
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.add(ByteCode.INVOKESPECIAL,
                superClass.getName().replace('.', '/'), 
                "<init>", "()", "V");
        
        // Load script class
        cfw.add(ByteCode.NEW, scriptClassName);
        cfw.add(ByteCode.DUP);
        cfw.add(ByteCode.INVOKESPECIAL, scriptClassName, "<init>", "()", "V");
        
        // Run script and save resulting scope
        cfw.add(ByteCode.INVOKESTATIC,
                "org/mozilla/javascript/ScriptRuntime",
                "runScript",
                "(Lorg/mozilla/javascript/Script;)",
                "Lorg/mozilla/javascript/FlattenedObject;");
        cfw.add(ByteCode.ASTORE_1);
                
        // Save the FlattenedObject in instance variable
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.add(ByteCode.ALOAD_1);  // the FlattenedObject
        cfw.add(ByteCode.PUTFIELD, name, "o", 
                "Lorg/mozilla/javascript/FlattenedObject;");

        // Store Scriptable object in "self", a public instance variable, 
        //  so scripts can read it.
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.add(ByteCode.ALOAD_1);  // the FlattenedObject
        cfw.add(ByteCode.INVOKEVIRTUAL,
                "org/mozilla/javascript/FlattenedObject",
                "getObject", "()",
                "Lorg/mozilla/javascript/Scriptable;");
        cfw.add(ByteCode.PUTFIELD, name, "self",
                "Lorg/mozilla/javascript/Scriptable;");		
        
        // Set the prototype of the js object to be a LiveConnect 
        // wapper of the generated class's object
        cfw.add(ByteCode.ALOAD_1);  // the FlattenedObject
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.add(ByteCode.INVOKESTATIC,
                "org/mozilla/javascript/ScriptRuntime",
                "setAdapterProto",
                "(Lorg/mozilla/javascript/FlattenedObject;" +
                 "Ljava/lang/Object;)",
                "V");

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
        } else  if (retType.equals(String.class)) {
            cfw.add(ByteCode.INVOKESTATIC, 
                    "org/mozilla/javascript/Context", 
                    "toString", "(Ljava/lang/Object;)", 
                    "Ljava/lang/String;");
            cfw.add(ByteCode.ARETURN);
        } else if (retType.equals(Scriptable.class)) {
            cfw.add(ByteCode.ALOAD_0);  // load 'this' to find scope from
            cfw.add(ByteCode.INVOKESTATIC, 
                    "org/mozilla/javascript/Context", 
                    "toObject", 
                    "(Ljava/lang/Object;" +
                     "Lorg/mozilla/javascript/Scriptable;)",
                    "Lorg/mozilla/javascript/Scriptable;");
            cfw.add(ByteCode.ARETURN);
        } else { 
            // If it is a wrapped type, cast to Wrapper and call unwrap()
            cfw.add(ByteCode.DUP);
            cfw.add(ByteCode.INSTANCEOF, "org/mozilla/javascript/Wrapper");
            // skip 3 for IFEQ, 3 for CHECKCAST, and 5 for INVOKEINTERFACE
            cfw.add(ByteCode.IFEQ, 11); 
            cfw.add(ByteCode.CHECKCAST, "org/mozilla/javascript/Wrapper");
            cfw.add(ByteCode.INVOKEINTERFACE,
                    "org/mozilla/javascript/Wrapper", 
                    "unwrap", "()", "Ljava/lang/Object;");

            // If Undefined, return null	   
            cfw.add(ByteCode.DUP);
            cfw.add(ByteCode.INSTANCEOF, "org/mozilla/javascript/Undefined");
            // skip 3 for IFEQ, 1 for ACONST_NULL, 1 for ARETURN
            cfw.add(ByteCode.IFEQ, 5);
            cfw.add(ByteCode.ACONST_NULL);
            cfw.add(ByteCode.ARETURN);

            // Now cast to return type
            String retTypeStr = retType.getName().replace('.', '/');
            cfw.add(ByteCode.CHECKCAST, retTypeStr);
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
                    cfw.add(ByteCode.GETFIELD, genName, "self", 
                            "Lorg/mozilla/javascript/Scriptable;");
                    cfw.add(ByteCode.ASTORE, scopeLocal);
                    loadedScope = true;
                }
                cfw.add(ByteCode.ALOAD, scopeLocal);

                // Get argument Class 
                cfw.addLoadConstant(parms[i].getName());
                cfw.add(ByteCode.INVOKESTATIC, 
                        "java/lang/Class", 
                        "forName", 
                        "(Ljava/lang/String;)",                        
                        "Ljava/lang/Class;");
      
                cfw.add(ByteCode.INVOKESTATIC, 
                        "org/mozilla/javascript/Context", 
                        "toObject", 
                        "(Ljava/lang/Object;" +
                         "Lorg/mozilla/javascript/Scriptable;" +
                         "Ljava/lang/Class;)",                         
                        "Lorg/mozilla/javascript/Scriptable;");
            }
            cfw.add(ByteCode.AASTORE);
        }
        
        cfw.add(ByteCode.ALOAD_0);  // this
        cfw.add(ByteCode.GETFIELD, genName, "o", 
                "Lorg/mozilla/javascript/FlattenedObject;");
        
        cfw.addLoadConstant(methodName);
        cfw.add(ByteCode.ALOAD, arrayLocal);
        
        // go through utility method, which creates a Context to run the 
        // method in.
        cfw.add(ByteCode.INVOKESTATIC,
                "org/mozilla/javascript/JavaAdapter",
                "callMethod",
                "(Lorg/mozilla/javascript/FlattenedObject;" +
                 "Ljava/lang/Object;[Ljava/lang/Object;)",
                "Ljava/lang/Object;");

        Class retType = returnType;
        if (retType.equals(Void.TYPE)) {
            cfw.add(ByteCode.POP);
            cfw.add(ByteCode.RETURN);
        } else {
            generateReturnResult(cfw, retType);
        }
        cfw.stopMethod((short)(scopeLocal + 1), null);
    }
    
    /**
     * Returns a fully qualified method name concatenated with its signature.
     */
    private static String getMethodSignature(Method method) {
        Class[] parms = method.getParameterTypes();
        StringBuffer sb = new StringBuffer();
        sb.append(method.getName());
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
                if (loader != null)
                    return loader.loadClass(name);
                clazz = findSystemClass(name);
            }
            if (resolve)
                resolveClass(clazz);
            return clazz;
        }

        private java.util.Hashtable loaded;
    }
    
    private static int serial;
    private static MyClassLoader classLoader;
}
