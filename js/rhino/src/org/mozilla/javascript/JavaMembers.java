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

import java.lang.reflect.*;
import java.util.Hashtable;
import java.util.Enumeration;

/**
 *
 * @author Mike Shaver
 * @author Norris Boyd
 * @see NativeJavaObject
 * @see NativeJavaClass
 */
class JavaMembers {

    JavaMembers(Scriptable scope, Class cl) {
        this.members = new Hashtable(23);
        this.staticMembers = new Hashtable(7);
        this.cl = cl;
        reflect(scope, cl);
    }

    boolean has(String name, boolean isStatic) {
        Hashtable ht = isStatic ? staticMembers : members;
        Object obj = ht.get(name);
        if (obj != null) {
            return true;
        }
        else {
            Member member = this.findExplicitFunction(name, isStatic);
            return member != null;
        }
    }

    Object get(Scriptable scope, String name, Object javaObject,
               boolean isStatic)
    {
        Hashtable ht = isStatic ? staticMembers : members;
        Object member = ht.get(name);
        if (!isStatic && member == null) {
            // Try to get static member from instance (LC3)
            member = staticMembers.get(name);
        }
        if (member == null) {
            member = this.getExplicitFunction(scope, name, 
                                              javaObject, isStatic);
            if (member == null)
                return Scriptable.NOT_FOUND;
        }
        if (member instanceof Scriptable)
            return member;
        Field field = (Field) member;
        Object rval;
        try {
            rval = field.get(isStatic ? null : javaObject);
        } catch (IllegalAccessException accEx) {
            throw new RuntimeException("unexpected IllegalAccessException "+
                                       "accessing Java field");
        }
        // Need to wrap the object before we return it.
        Class fieldType = field.getType();
        scope = ScriptableObject.getTopLevelScope(scope);
        return NativeJavaObject.wrap(scope, rval, fieldType);
    }

    Member findExplicitFunction(String name, boolean isStatic) {
        Hashtable ht = isStatic ? staticMembers : members;
        int sigStart = name.indexOf('(');
        Object member = null;
        Member[] methodsOrCtors = null;
        NativeJavaMethod method = null;
        boolean isCtor = (isStatic && sigStart == 0);

        if (isCtor) {
            // Explicit request for an overloaded constructor
            methodsOrCtors = ctors;
        }
        else if (sigStart > 0) {
            // Explicit request for an overloaded method
            String trueName = name.substring(0,sigStart);
            Object obj = ht.get(trueName);
            if (!isStatic && obj == null) {
                // Try to get static member from instance (LC3)
                obj = staticMembers.get(trueName);
            }
            if (obj != null && obj instanceof NativeJavaMethod) {
                method = (NativeJavaMethod)obj;
                methodsOrCtors = method.getMethods();
            }
        }

        if (methodsOrCtors != null) {
            for (int i = 0; i < methodsOrCtors.length; i++) {
                String nameWithSig = 
                    NativeJavaMethod.signature(methodsOrCtors[i]);
                if (name.equals(nameWithSig)) {
                    return methodsOrCtors[i];
                }
            }
        }

        return null;
    }

    Object getExplicitFunction(Scriptable scope, 
                               String name,
                               Object javaObject,
                               boolean isStatic) {

        Hashtable ht = isStatic ? staticMembers : members;
        Object member = null;
        Member methodOrCtor = this.findExplicitFunction(name, isStatic);

        if (methodOrCtor != null) {
            Scriptable prototype = 
                ScriptableObject.getFunctionPrototype(scope);

            if (methodOrCtor instanceof Constructor) {
                NativeJavaConstructor fun = 
                    new NativeJavaConstructor((Constructor)methodOrCtor);
                fun.setParentScope(scope);
                fun.setPrototype(prototype);
                member = fun;
                ht.put(name, fun);
            }
            else {
                String trueName = methodOrCtor.getName();
                member = ht.get(trueName);

                if (member instanceof NativeJavaMethod &&
                    ((NativeJavaMethod)member).getMethods().length > 1 ) {
                    NativeJavaMethod fun = 
                        new NativeJavaMethod((Method)methodOrCtor, name);
                    fun.setParentScope(scope);
                    fun.setPrototype(prototype);
                    ht.put(name, fun);
                    member = fun;
                }
            }
        }

        return member;
    }


    public void put(String name, Object javaObject, Object value,
                    boolean isStatic)
    {
        Hashtable ht = isStatic ? staticMembers : members;
        Object member = ht.get(name);
        if (!isStatic && member == null) {
            // Try to get static member from instance (LC3)
            member = staticMembers.get(name);
        }
        if (member == null)
            throw reportMemberNotFound(name);
        if (member instanceof FieldAndMethods) {
            member = ((FieldAndMethods) ht.get(name)).getField();
        }
        Field field = null;
        try {
            field = (Field) member;
            field.set(javaObject, NativeJavaObject.coerceType(field.getType(),
                                                              value));
        } catch (ClassCastException e) {
            Object errArgs[] = { name };
            throw Context.reportRuntimeError(Context.getMessage
                                          ("msg.java.method.assign",
                                          errArgs));
        } catch (IllegalAccessException accessEx) {
            throw new RuntimeException("unexpected IllegalAccessException "+
                                       "accessing Java field");
        } catch (IllegalArgumentException argEx) {
            Object errArgs[] = { value.getClass().getName(), field,
                                 javaObject.getClass().getName() };
            throw Context.reportRuntimeError(Context.getMessage(
                "msg.java.internal.field.type", errArgs));
        }
    }

    Object[] getIds(boolean isStatic) {
        Hashtable ht = isStatic ? staticMembers : members;
        int len = ht.size();
        Object[] result = new Object[len];
        Enumeration keys = ht.keys();
        for (int i=0; i < len; i++)
            result[i] = keys.nextElement();
        return result;
    }
    
    Class getReflectedClass() {
        return cl;
    }

    void reflectField(Field field) {
        int mods = field.getModifiers();
        if (!Modifier.isPublic(mods))
            return;
        boolean isStatic = Modifier.isStatic(mods);
        Hashtable ht = isStatic ? staticMembers : members;
        String name = field.getName();
        Object member = ht.get(name);
        if (member != null) {
            if (member instanceof NativeJavaMethod) {
                NativeJavaMethod method = (NativeJavaMethod) member;
                Hashtable fmht = isStatic ? staticFieldAndMethods
                                          : fieldAndMethods;
                if (fmht == null) {
                    fmht = new Hashtable(11);
                    if (isStatic)
                        staticFieldAndMethods = fmht;
                    else
                        fieldAndMethods = fmht;
                }
                FieldAndMethods fam = new FieldAndMethods(method.getMethods(),
                                                          field);
                fmht.put(name, fam);
                ht.put(name, fam);
                return;
            }
            if (member instanceof Field) {
            	Field oldField = (Field) member;
            	// beard:
            	// If an exception is thrown here, then JDirect classes on MRJ can't be used. JDirect
            	// classes implement multiple interfaces that each have a static "libraryInstance" field.
            	if (false) {
	                throw new RuntimeException("cannot have multiple Java " +
    	                                       "fields with same name");
            	}
            	// If this newly reflected field shadows an inherited field, then replace it. Otherwise,
            	// since access to the field would be ambiguous from Java, no field should be reflected.
            	// For now, the first field found wins, unless another field explicitly shadows it.
            	if (oldField.getDeclaringClass().isAssignableFrom(field.getDeclaringClass()))
            		ht.put(name, field);
            	return;
            }
            throw new RuntimeException("unknown member type");
        }
        ht.put(name, field);
    }

    void reflectMethod(Scriptable scope, Method method) {
        int mods = method.getModifiers();
        if (!Modifier.isPublic(mods))
            return;
        boolean isStatic = Modifier.isStatic(mods);
        Hashtable ht = isStatic ? staticMembers : members;
        String name = method.getName();
        NativeJavaMethod fun = (NativeJavaMethod) ht.get(name);
        if (fun == null) {
            fun = new NativeJavaMethod();
            fun.setParentScope(scope);
            fun.setPrototype(ScriptableObject.getFunctionPrototype(scope));
            ht.put(name, fun);
            fun.add(method);
        }
        else
            fun.add(method);
    }

    void reflect(Scriptable scope, Class cl) {
        // We reflect methods first, because we want overloaded field/method
        // names to be allocated to the NativeJavaMethod before the field
        // gets in the way.
        Method[] methods = cl.getMethods();
        for (int i = 0; i < methods.length; i++)
            reflectMethod(scope, methods[i]);

        Field[] fields = cl.getFields();
        for (int i = 0; i < fields.length; i++)
            reflectField(fields[i]);

        ctors = cl.getConstructors();
    }

    Hashtable getFieldAndMethodsObjects(Object javaObject, boolean isStatic) {
        Hashtable ht = isStatic ? staticFieldAndMethods : fieldAndMethods;
        if (ht == null)
            return null;
        int len = ht.size();
        Hashtable result = new Hashtable(len);
        Enumeration e = ht.elements();
        while (len-- > 0) {
            FieldAndMethods fam = (FieldAndMethods) e.nextElement();
            fam = (FieldAndMethods) fam.clone();
            fam.setJavaObject(javaObject);
            result.put(fam.getName(), fam);
        }
        return result;
    }

    Constructor[] getConstructors() {
        return ctors;
    }

    static JavaMembers lookupClass(Scriptable scope, Class dynamicType,
                                   Class staticType)
    {
        Class cl = dynamicType;
        JavaMembers members = (JavaMembers) classTable.get(cl);
        if (members != null)
            return members;
        if (staticType != null && staticType != dynamicType &&
            !Modifier.isPublic(dynamicType.getModifiers()) &&
            Modifier.isPublic(staticType.getModifiers()))
        {
            cl = staticType;
        }
        synchronized (classTable) {
            members = (JavaMembers) classTable.get(cl);
            if (members != null)
                return members;
            try {
                members = new JavaMembers(scope, cl);
            } catch (SecurityException e) {
                // Reflection may fail for objects that are in a restricted 
                // access package (e.g. sun.*).  If we get a security
                // exception, try again with the static type. Otherwise, 
                // rethrow the exception.
                if (cl != staticType)
                    members = new JavaMembers(scope, staticType);
                else
                    throw e;
            }
            classTable.put(cl, members);
            return members;
        }
    }

    RuntimeException reportMemberNotFound(String memberName) {
        Object errArgs[] = { cl.getName(), 
                             memberName };
        return Context.reportRuntimeError(
            Context.getMessage("msg.java.member.not.found",
                               errArgs));
    }

    private static Hashtable classTable = new Hashtable();

    private Class cl;
    private Hashtable members;
    private Hashtable fieldAndMethods;
    private Hashtable staticMembers;
    private Hashtable staticFieldAndMethods;
    private Constructor[] ctors;
}


class FieldAndMethods extends NativeJavaMethod {

    FieldAndMethods(Method[] methods, Field field) {
        super(methods);
        this.field = field;
    }

    void setJavaObject(Object javaObject) {
        this.javaObject = javaObject;
    }

    String getName() {
        return field.getName();
    }

    Field getField() {
        return field;
    }

    public Object getDefaultValue(Class hint) {
        if (hint == ScriptRuntime.FunctionClass)
            return this;
        Object rval;
        try {
            rval = field.get(javaObject);
        } catch (IllegalAccessException accEx) {
            throw Context.reportRuntimeError(Context.getMessage
                                          ("msg.java.internal.private", null));
        }
        rval = NativeJavaObject.wrap(this, rval, field.getType());
        if (rval instanceof Scriptable) {
            ((Scriptable)rval).setParentScope(this);
            ((Scriptable)rval).setPrototype(parent.getPrototype());
        }
        return rval;
    }

    public Object clone() {
        FieldAndMethods result = new FieldAndMethods(methods, field);
        result.javaObject = javaObject;
        return result;
    }
    
    private Field field;
    private Object javaObject;
}
