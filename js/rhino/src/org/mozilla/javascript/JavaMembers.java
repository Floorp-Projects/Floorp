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
 * May 6, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Norris Boyd
 * Frank Mitchell
 * Mike Shaver
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
            FieldAndMethods fam = (FieldAndMethods) ht.get(name);
            member = fam.getField();
            if (member == null) 
                member = fam.getBeanSetMethod();
        }
        
        // Is this a bean property "set"?
        if (member instanceof Method) { 
            try {
                Method method = (Method) member;
                Class[] types = method.getParameterTypes();
                Object[] params = { NativeJavaObject.coerceType(types[0], value) };
                method.invoke(javaObject, params);
            } catch (IllegalAccessException accessEx) {
                throw new RuntimeException("unexpected IllegalAccessException "+
                                           "accessing Java field");
            } catch (InvocationTargetException invTarget) {
                throw Context.reportRuntimeError("unexpected " + invTarget.getTargetException() +
                                                 " accessing Java field");
            }
        }
        else {
            Field field = null;
            try {
                field = (Field) member;
                if (field == null) {
                    Object[] args = {name};
                    throw Context.reportRuntimeError(
                        Context.getMessage("msg.java.internal.private", args));
                }
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
    
    void reflectField(Scriptable scope, Field field) {
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
                FieldAndMethods fam = new FieldAndMethods(method.getMethods(),
                                                          field,
                                                          null);
                fam.setParentScope(scope);
                fam.setPrototype(ScriptableObject.getFunctionPrototype(scope));
                getFieldAndMethodsTable(isStatic).put(name, fam);
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
            reflectField(scope, fields[i]);

        makeBeanProperties(scope, false);
        makeBeanProperties(scope, true);

        ctors = cl.getConstructors();
    }
    
    Hashtable getFieldAndMethodsTable(boolean isStatic) {
        Hashtable fmht = isStatic ? staticFieldAndMethods
                                  : fieldAndMethods;
        if (fmht == null) {
            fmht = new Hashtable(11);
            if (isStatic)
                staticFieldAndMethods = fmht;
            else
                fieldAndMethods = fmht;
        }
        
        return fmht;
    }

    void makeBeanProperties(Scriptable scope, boolean isStatic) {
        Hashtable ht = isStatic ? staticMembers : members;
        Hashtable toAdd = new Hashtable();
        Scriptable prototype = ScriptableObject.getFunctionPrototype(scope);
        
        // Now, For each member, make "bean" properties.
        for (Enumeration e = ht.keys(); e.hasMoreElements(); ) {
            
            // Is this a getter?
            String name = (String) e.nextElement();
            boolean memberIsGetMethod = name.startsWith("get"), memberIsIsMethod = name.startsWith("is");
            if (memberIsGetMethod || memberIsIsMethod)
            {
                // Double check name component.
                String nameComponent = name.substring(memberIsGetMethod ? 3 : 2);
                if( nameComponent.length() == 0 ) 
                    continue;
                
                // Make the bean property name.
                String beanPropertyName = nameComponent;
                if( nameComponent.length() > 1 && 
                    Character.isUpperCase( nameComponent.charAt(0)) && 
                    ! Character.isUpperCase( nameComponent.charAt(1)) )
                   beanPropertyName = Character.toLowerCase(nameComponent.charAt(0)) + nameComponent.substring(1);
                
                // If we already have a field by this name, don't do this property.
                if (ht.containsKey(beanPropertyName)) {
                    
                    // Exclude field.
                    Object propertyMethod = ht.get(beanPropertyName);
                    if (propertyMethod instanceof Field)
                        continue;
                    
                    // Exclude when there's a method with non-bean characteristics or non-static affinity.
                    Method[] methods = ((NativeJavaMethod) propertyMethod).getMethods();
                    boolean exclude = false;
                    for (int i = 0; i < methods.length; ++i) 
                        if (Modifier.isStatic(methods[i].getModifiers()) != isStatic ||
                            methods[i].getParameterTypes().length > 0)
                            exclude = true;
                    if (exclude)
                        continue;
                }
                
                // Get the method by this name.
                Object method = ht.get(name);
                if (!(method instanceof NativeJavaMethod))
                    continue;
                NativeJavaMethod getJavaMethod = (NativeJavaMethod) method;
                
                // Grab and inspect the getter method; does it have an empty parameter list,
                // with a return value (eg. a getSomething() or isSomething())?
                Class[] params;
                Method[] getMethods = getJavaMethod.getMethods();
                Class type;
                if( getMethods != null && 
                    getMethods.length == 1 && 
                    (type = getMethods[0].getReturnType()) != null &&
                    (params = getMethods[0].getParameterTypes()) != null && 
                    params.length == 0) { 
                    
                    // Make sure the method static-ness is preserved for this property.
                    if (isStatic && !Modifier.isStatic(getMethods[0].getModifiers()))
                        continue;
                        
                    // We have a getter.  Now, do we have a setter?
                    Method setMethod = null;
                    String setter = "set" + nameComponent;
                    if (ht.containsKey(setter)) { 

                        // Is this value a method?
                        method = ht.get(setter);
                        if (method instanceof NativeJavaMethod) {
                        
                            //
                            // Note: it may be preferable to allow NativeJavaMethod.findFunction()
                            //       to find the appropriate setter; unfortunately, it requires an
                            //       instance of the target arg to determine that.
                            //

                            // Make two passes: one to find a method with direct type assignment, 
                            // and one to find a widening conversion.
                            NativeJavaMethod setJavaMethod = (NativeJavaMethod) method;
                            Method[] setMethods = setJavaMethod.getMethods();
                            for (int pass = 1; pass <= 2 && setMethod == null; ++pass)
                                for (int i = 0; i < setMethods.length; ++i) {
                                    if (setMethods[i].getReturnType() == void.class &&
                                        (!isStatic || Modifier.isStatic(setMethods[i].getModifiers())) &&
                                        (params = setMethods[i].getParameterTypes()) != null && 
                                        params.length == 1 ) { 
                                        
                                        if ((pass == 1 && params[0] == type) ||
                                            (pass == 2 && params[0].isAssignableFrom(type))) { 
                                            setMethod = setMethods[i];
                                            break;
                                        }
                                    }
                                }
                        }
                    }
                            
                    // Make the property.
                    Method[] bothMethods = {getMethods[0], setMethod};
                    Method[] oneMethod = {getMethods[0]};
                    Method[] methods = (setMethod == null) ? oneMethod : bothMethods;
                    FieldAndMethods fam = new FieldAndMethods(methods, null, beanPropertyName);
                    fam.setParentScope(scope);
                    fam.setPrototype(prototype);
                    toAdd.put(beanPropertyName, fam);
                }
            }
        }           
        
        // Add the new bean properties.
        Hashtable fmht = getFieldAndMethodsTable(isStatic);
        for (Enumeration e = toAdd.keys(); e.hasMoreElements();) {
            String key = (String) e.nextElement();
            Object value = toAdd.get(key);
            ht.put(key, value);
            fmht.put(key, value);
        }
    }

    Hashtable getFieldAndMethodsObjects(Scriptable scope, Object javaObject, boolean isStatic) {
        Hashtable ht = isStatic ? staticFieldAndMethods : fieldAndMethods;
        if (ht == null)
            return null;
        int len = ht.size();
        Hashtable result = new Hashtable(Math.max(len,1));
        Enumeration e = ht.elements();
        while (len-- > 0) {
            FieldAndMethods fam = (FieldAndMethods) e.nextElement();
            fam = (FieldAndMethods) fam.clone();
            fam.setJavaObject(javaObject);
            fam.setParentScope(scope);
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

    FieldAndMethods(Method[] methods, Field field, String name) {
        super(methods);
        this.field = field;
        this.name = name;
    }

    void setJavaObject(Object javaObject) {
        this.javaObject = javaObject;
    }

    String getName() {
        if (field == null)
            return name;
        return field.getName();
    }

    Field getField() {
        return field;
    }
    
    private Method getBeanMethod(boolean which) {
        Method methods[] = getMethods();
        for (int i = 0; i < methods.length; ++i) {
            Class[] params = methods[i].getParameterTypes();
            boolean isSetter = params != null && params.length == 1;
            if (which == isSetter) 
                return methods[i];
        }
        return null;
    }
    
    Method getBeanGetMethod() {
        return getBeanMethod(false);
    }
    
    Method getBeanSetMethod() {
        return getBeanMethod(true);
    }
    
    Class getBeanType() {
        Method methods[] = getMethods();
        for (int i = 0; i < methods.length; ++i) {
            Class[] params = methods[i].getParameterTypes();
            if (params != null && params.length == 1)
                return params[0];
            Class returnType = methods[i].getReturnType();
            if (returnType != null)
                return returnType;
        }
        return null;
    }

    public Object getDefaultValue(Class hint) {
        if (hint == ScriptRuntime.FunctionClass)
            return this;
        Object rval;
        Class type;
        try {
            if (field != null) {
                rval = field.get(javaObject);
                type = field.getType();
            }
            else {
                Method method = getBeanGetMethod();
                boolean isStatic = Modifier.isStatic(method.getModifiers());
                rval = method.invoke(isStatic ? null : javaObject, null);
                type = getBeanType();
            }
        } catch (InvocationTargetException invTarget) {
            throw Context.reportRuntimeError("unexpected " + invTarget.getTargetException() +
                                             " accessing Java property");
        } catch (IllegalAccessException accEx) {
            Object[] args = {getName()};
            throw Context.reportRuntimeError(Context.getMessage
                                          ("msg.java.internal.private", args));
        }
        rval = NativeJavaObject.wrap(this, rval, type);
        if (rval instanceof Scriptable) {
            ((Scriptable)rval).setParentScope(this);
            ((Scriptable)rval).setPrototype(parent != null ? parent.getPrototype() : getPrototype());
            // TODO: why are we setting the prototype of the return value here?
            //       this seems wrong...what if rval is a string, for instance?   
            
            // Handle string conversion request.
            if (hint == ScriptRuntime.StringClass && rval != null)
                rval = Context.toString(rval);
        }
        return rval;
    }

    public Object clone() {
        FieldAndMethods result = new FieldAndMethods(methods, field, name);
        result.javaObject = javaObject;
        return result;
    }
    
    private Field field;
    private Object javaObject;
    private String name;
}
