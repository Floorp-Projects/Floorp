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
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Norris Boyd
 * Frank Mitchell
 * Mike Shaver
 * Kurt Westerfeld
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
        } else {
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
            return member;      // why is this here?
        Object rval;
        Class type;
        try {
            if (member instanceof BeanProperty) {
                BeanProperty bp = (BeanProperty) member;
                try {
                    rval = bp.getter.invoke(javaObject, null);
                } catch (IllegalAccessException e) {
                    rval = NativeJavaMethod.retryIllegalAccessInvoke(
                            bp.getter,
                            javaObject,
                            null,
                            e);
                }
                type = bp.getter.getReturnType();
            } else {
                Field field = (Field) member;
                rval = field.get(isStatic ? null : javaObject);
                type = field.getType();
            }
        } catch (IllegalAccessException accEx) {
            throw new RuntimeException("unexpected IllegalAccessException "+
                                       "accessing Java field");
        } catch (InvocationTargetException e) {
            throw WrappedException.wrapException(
                JavaScriptException.wrapException(scope, e));
        }
        // Need to wrap the object before we return it.
        scope = ScriptableObject.getTopLevelScope(scope);
        return NativeJavaObject.wrap(scope, rval, type);
    }

    Member findExplicitFunction(String name, boolean isStatic) {
        Hashtable ht = isStatic ? staticMembers : members;
        int sigStart = name.indexOf('(');
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

    Object getExplicitFunction(Scriptable scope, String name, 
                               Object javaObject, boolean isStatic) 
    {
        Hashtable ht = isStatic ? staticMembers : members;
        Object member = null;
        Member methodOrCtor = this.findExplicitFunction(name, isStatic);

        if (methodOrCtor != null) {
            Scriptable prototype = 
                ScriptableObject.getFunctionPrototype(scope);

            if (methodOrCtor instanceof Constructor) {
                NativeJavaConstructor fun = 
                    new NativeJavaConstructor((Constructor)methodOrCtor);
                fun.setPrototype(prototype);
                member = fun;
                ht.put(name, fun);
            } else {
                String trueName = methodOrCtor.getName();
                member = ht.get(trueName);

                if (member instanceof NativeJavaMethod &&
                    ((NativeJavaMethod)member).getMethods().length > 1 ) {
                    NativeJavaMethod fun = 
                        new NativeJavaMethod((Method)methodOrCtor, name);
                    fun.setPrototype(prototype);
                    ht.put(name, fun);
                    member = fun;
                }
            }
        }

        return member;
    }


    public void put(Scriptable scope, String name, Object javaObject, 
                    Object value, boolean isStatic)
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
        }
        
        // Is this a bean property "set"?
        if (member instanceof BeanProperty) { 
            try {
                Method method = ((BeanProperty) member).setter;
                if (method == null)
                    throw reportMemberNotFound(name);
                Class[] types = method.getParameterTypes();
                Object[] params = { NativeJavaObject.coerceType(types[0], value) };
                method.invoke(javaObject, params);
            } catch (IllegalAccessException accessEx) {
                throw new RuntimeException("unexpected IllegalAccessException " +
                                           "accessing Java field");
            } catch (InvocationTargetException e) {
                throw WrappedException.wrapException(
                    JavaScriptException.wrapException(scope, e));
            }
        }
        else {
            Field field = null;
            try {
                field = (Field) member;
                if (field == null) {
                    throw Context.reportRuntimeError1(
                        "msg.java.internal.private", name);
                }
                field.set(javaObject,
                          NativeJavaObject.coerceType(field.getType(), value));
            } catch (ClassCastException e) {
                throw Context.reportRuntimeError1(
                    "msg.java.method.assign", name);
            } catch (IllegalAccessException accessEx) {
                throw new RuntimeException("unexpected IllegalAccessException "+
                                           "accessing Java field");
            } catch (IllegalArgumentException argEx) {
                throw Context.reportRuntimeError3(
                    "msg.java.internal.field.type", 
                    value.getClass().getName(), field,
                    javaObject.getClass().getName());
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
                fam.setPrototype(ScriptableObject.getFunctionPrototype(scope));
                getFieldAndMethodsTable(isStatic).put(name, fam);
                ht.put(name, fam);
                return;
            }
            if (member instanceof Field) {
            	Field oldField = (Field) member;
            	// If this newly reflected field shadows an inherited field, 
                // then replace it. Otherwise, since access to the field 
                // would be ambiguous from Java, no field should be reflected.
            	// For now, the first field found wins, unless another field 
                // explicitly shadows it.
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
            if (scope != null)
                fun.setPrototype(ScriptableObject.getFunctionPrototype(scope));
            ht.put(name, fun);
            fun.add(method);
        } else {
            fun.add(method);
        }
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
        
        // Now, For each member, make "bean" properties.
        for (Enumeration e = ht.keys(); e.hasMoreElements(); ) {
            
            // Is this a getter?
            String name = (String) e.nextElement();
            boolean memberIsGetMethod = name.startsWith("get");
            boolean memberIsIsMethod = name.startsWith("is");
            if (memberIsGetMethod || memberIsIsMethod) {
                // Double check name component.
                String nameComponent = name.substring(memberIsGetMethod ? 3 : 2);
                if (nameComponent.length() == 0) 
                    continue;
                
                // Make the bean property name.
                String beanPropertyName = nameComponent;
                if (Character.isUpperCase(nameComponent.charAt(0))) {
                    if (nameComponent.length() == 1) {
                        beanPropertyName = nameComponent.substring(0, 1).toLowerCase();
                    } else if (!Character.isUpperCase(nameComponent.charAt(1))) {
                        beanPropertyName = Character.toLowerCase(nameComponent.charAt(0)) + 
                                           nameComponent.substring(1);
                    }
                }
                
                // If we already have a member by this name, don't do this
                // property.
                if (ht.containsKey(beanPropertyName))
                    continue;
                
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
                if (getMethods != null && 
                    getMethods.length == 1 && 
                    (type = getMethods[0].getReturnType()) != null &&
                    (params = getMethods[0].getParameterTypes()) != null && 
                    params.length == 0) 
                { 
                    
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
                            for (int pass = 1; pass <= 2 && setMethod == null; ++pass) {
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
                    }
                            
                    // Make the property.
                    BeanProperty bp = new BeanProperty(getMethods[0], setMethod);
                    toAdd.put(beanPropertyName, bp);
                }
            }
        }           
        
        // Add the new bean properties.
        for (Enumeration e = toAdd.keys(); e.hasMoreElements();) {
            String key = (String) e.nextElement();
            Object value = toAdd.get(key);
            ht.put(key, value);
        }
    }

    Hashtable getFieldAndMethodsObjects(Scriptable scope, Object javaObject,
                                        boolean isStatic) 
    {
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
        Hashtable ct = classTable;  // use local reference to avoid synchronize
        JavaMembers members = (JavaMembers) ct.get(cl);
        if (members != null)
            return members;
        if (staticType != null && staticType != dynamicType &&
            !Modifier.isPublic(dynamicType.getModifiers()) &&
            Modifier.isPublic(staticType.getModifiers()))
        {
            cl = staticType;

            // If the static type is an interface, use it
            if( !cl.isInterface() )
            {
                // We can use the static type, and that is OK, but we'll trace
                // back the java class chain here to look for something more suitable.
                for (Class parentType = dynamicType; 
                     parentType != null && parentType != ScriptRuntime.ObjectClass;
                     parentType = parentType.getSuperclass())
                {
                    if (Modifier.isPublic(parentType.getModifiers())) {
                        cl = parentType;
                        break;
                    }
                }
            }
        }
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
        if (Context.isCachingEnabled) 
            ct.put(cl, members);
        return members;
    }

    RuntimeException reportMemberNotFound(String memberName) {
        return Context.reportRuntimeError2(
            "msg.java.member.not.found", cl.getName(), memberName);
    }

    static Hashtable classTable = new Hashtable();

    private Class cl;
    private Hashtable members;
    private Hashtable fieldAndMethods;
    private Hashtable staticMembers;
    private Hashtable staticFieldAndMethods;
    private Constructor[] ctors;
}

class BeanProperty {
    BeanProperty(Method getter, Method setter) {
        this.getter = getter;
        this.setter = setter;
    }
    Method getter;
    Method setter;
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
    
    public Object getDefaultValue(Class hint) {
        if (hint == ScriptRuntime.FunctionClass)
            return this;
        Object rval;
        Class type;
        try {
            rval = field.get(javaObject);
            type = field.getType();
        } catch (IllegalAccessException accEx) {
            throw Context.reportRuntimeError1(
                "msg.java.internal.private", getName());
        }
        rval = NativeJavaObject.wrap(this, rval, type);
        if (rval instanceof Scriptable) {
            rval = ((Scriptable) rval).getDefaultValue(hint);
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
