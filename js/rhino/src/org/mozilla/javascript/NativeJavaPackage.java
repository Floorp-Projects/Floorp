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

/**
 * This class reflects Java packages into the JavaScript environment.  We 
 * lazily reflect classes and subpackages, and use a caching/sharing 
 * system to ensure that members reflected into one JavaPackage appear
 * in all other references to the same package (as with Packages.java.lang 
 * and java.lang).
 *
 * @author Mike Shaver
 * @see NativeJavaArray
 * @see NativeJavaObject
 * @see NativeJavaClass
 */

public class NativeJavaPackage extends ScriptableObject {

    // we know these are packages so we can skip the class check
    // note that this is ok even if the package isn't present.
    static final String[] commonPackages = {
        "java.lang",
        "java.lang.reflect",
        "java.io",
        "java.math",
        "java.util",
        "java.util.zip",
        "java.text",
        "java.text.resources",
        "java.applet",
        "netscape.security",
        "netscape.plugin",
        "netscape.application",
    };

    public static Scriptable init(Scriptable scope) 
        throws PropertyException
    {
        NativeJavaPackage packages = new NativeJavaPackage("");
        packages.setPrototype(getObjectPrototype(scope));
        packages.setParentScope(scope);

        // We want to get a real alias, and not a distinct JavaPackage
        // with the same packageName, so that we share classes and packages
        // that are underneath.
        NativeJavaPackage javaAlias = (NativeJavaPackage)packages.get("java",
                                                                      packages);

        // It's safe to downcast here since initStandardObjects takes
        // a ScriptableObject.
        ScriptableObject global = (ScriptableObject) scope;

        global.defineProperty("Packages", packages, ScriptableObject.DONTENUM);
        global.defineProperty("java", javaAlias, ScriptableObject.DONTENUM);

        for (int i = 0; i < commonPackages.length; i++)
            packages.forcePackage(commonPackages[i]);

        NativeJavaObject.initJSObject();
        
        String[] methods = { "getClass" };
        global.defineFunctionProperties(methods, NativeJavaPackage.class, 
                                        ScriptableObject.DONTENUM);

        // I think I'm supposed to return the prototype, but I don't have one.
        return packages;
    }

    // set up a name which is known to be a package so we don't
    // need to look for a class by that name
    void forcePackage(String name) {
        NativeJavaPackage pkg;
        int end = name.indexOf('.');
        if (end == -1)
            end = name.length();

        String id = name.substring(0, end);
        Object cached = super.get(id, this);
        if (cached != null && cached instanceof NativeJavaPackage) {
            pkg = (NativeJavaPackage) cached;
        } else {
            String newPackage = packageName.length() == 0 
                                ? id 
                                : packageName + "." + id;
            pkg = new NativeJavaPackage(newPackage);
            pkg.setParentScope(this);
            pkg.setPrototype(this.prototype);
            super.put(id, this, pkg);
        }
        if (end < name.length())
            pkg.forcePackage(name.substring(end+1));
    }

    public NativeJavaPackage(String packageName) {
        this.packageName = packageName;
    }

    public String getClassName() {
        return "JavaPackage";
    }

    public boolean has(String id, int index, Scriptable start) {
        return true;
    }

    public void put(String id, Scriptable start, Object value) {
        // Can't add properties to Java packages.  Sorry.
    }

    public void put(int index, Scriptable start, Object value) {
        // Can't add properties to Java packages.  Sorry.
    }

    public synchronized Object get(String id, Scriptable start) {
        Object cached = super.get(id, start);
        if (cached != NOT_FOUND)
            return cached;

        String newPackage = packageName.length() == 0
                            ? id 
                            : packageName + "." + id;
        Scriptable newValue;
        try {
            Class newClass = Class.forName(newPackage);
            newValue =  NativeJavaClass.wrap(getTopLevelScope(this), newClass);
            newValue.setParentScope(this);
            newValue.setPrototype(this.prototype);
        } catch (ClassNotFoundException ex) {
            newValue = new NativeJavaPackage(newPackage);
            newValue.setParentScope(this);
            newValue.setPrototype(this.prototype);
        }

        // Make it available for fast lookup and sharing of lazily-reflected
        // constructors and static members.
        super.put(id, start, newValue);
        return newValue;
    }

    public synchronized Object get(int index, Scriptable start) {
        return NOT_FOUND;
    }

    public Object getDefaultValue(Class ignored) {
        return toString();
    }

    public String toString() {
        return "[JavaPackage " + packageName + "]";
    }
    
    public static Scriptable getClass(Context cx, Scriptable thisObj,
                                      Object[] args, Function funObj)
    {
        if (args.length > 0  && args[0] instanceof NativeJavaObject) {
            NativeJavaObject nativeJavaObj = (NativeJavaObject) args[0];
            Scriptable result = getTopLevelScope(thisObj);
            Class cl = nativeJavaObj.unwrap().getClass();
            // Evaluate the class name by getting successive properties of 
            // the string to find the appropriate NativeJavaClass object
            String name = "Packages." + cl.getName();
            int offset = 0;
            for (;;) {
                int index = name.indexOf('.', offset);
                String propName = index == -1
                                  ? name.substring(offset)
                                  : name.substring(offset, index);
                Object prop = result.get(propName, result);
                if (!(prop instanceof Scriptable)) 
                    break;  // fall through to error
                result = (Scriptable) prop;
                if (index == -1)
                    return result;
                offset = index+1;
            }
        }
        throw Context.reportRuntimeError(
            Context.getMessage("msg.not.java.obj", null));
    }

    private String packageName;
}
