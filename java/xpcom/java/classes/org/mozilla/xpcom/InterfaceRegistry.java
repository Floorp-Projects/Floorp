/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Denis Sharypov <sdv@sparc.spb.su>
 */

package org.mozilla.xpcom;

import java.util.Hashtable;
import java.lang.reflect.*;

public class InterfaceRegistry {
    
    private static String IID_STRING = "IID";
    private static Hashtable interfaces = null;
    private static Hashtable iMethods = null;
    private static Hashtable keywords = null;
    private static boolean debug = true;
    
    private InterfaceRegistry() {
    }

    public static void register(nsISupports obj) {
        if (obj == null) {
            return;
        }
        Class cl = obj.getClass();
        register(cl);
    }

    public static void register(String name) {
        try {
            register(Class.forName(name));
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public static void register(Class cl) {
        if (cl == null) {
            return;
        }
        if (interfaces == null) {
            interfaces = new Hashtable();
        }
        if (iMethods == null) {
            iMethods = new Hashtable();
        }
        if (keywords == null) {
            keywords = new Hashtable(javaKeywords.length);
            for (int i = 0; i < javaKeywords.length; i++) {
                keywords.put(javaKeywords[i], javaKeywords[i]);
            }
        }
        if (!cl.isInterface()) {
            Class[] ifaces = cl.getInterfaces();
            for (int i = 0; i < ifaces.length; i++) {
                registerInterfaces(ifaces[i]);
            }
        } else {
            registerInterfaces(cl);
        }
    }

    public static void unregister(IID iid) {
        interfaces.remove(iid);
        iMethods.remove(iid);
    }
    
    private static Hashtable registerInterfaces(Class cl) {
        try {
            Object iidStr = cl.getField(IID_STRING).get(cl);
            if (iidStr instanceof String) {
                IID iid = new IID((String)iidStr);
                // if this iface hasn't been registered, yet
                if (interfaces.get(iid) == null) {
                    String[] methodNames = Utilities.getInterfaceMethodNames((String)iidStr);
                    if (methodNames != null) {
                        Method[] rmethods = new Method[methodNames.length];
                        Class[] ifaces = cl.getInterfaces();
                        // check for single inheritance (xpcom)
                        if (ifaces.length < 2) {
                            // recursively get all parent interface methods                    
                            Hashtable mhash = null;
                            Method[] methods = cl.getDeclaredMethods();
                            // the very super iface
                            if (ifaces.length == 0) {
                                mhash = new Hashtable(methods.length);
                            } else {
                                mhash = new Hashtable(registerInterfaces(ifaces[0]));
                            }
                            for (int i = 0; i < methods.length; i++) {
                                mhash.put(methods[i].getName(), methods[i]);
                            }
                        
                            for (int j = methodNames.length - 1; j >= 0; j--) {
                                rmethods[j] = (Method)mhash.get(subscriptMethodName(methodNames[j]));
                            }
                        
                            interfaces.put(iid, cl);
                            iMethods.put(iid, new MethodArray(rmethods, mhash));
                            
                            debug(cl.getName() + ": " + iid + "  ( " + cl + " )");
                            printMethods(rmethods);
                            
                            return mhash;
                        } 
                    }   
                // simply pass iface methods
                } else {
                    MethodArray m = (MethodArray)iMethods.get(iid);
                    if (m != null) {            
                        return m.names;
                    }
                }
            }
        } catch (NoSuchFieldException e) {
            // the interface doesn't define IID field
            debug("no such field...");		
        } catch (IllegalAccessException e1) {
            debug("can't access field...");		
        }
        return new Hashtable();
    }

    public static Class getInterface(IID iid) {
        Object obj = null;
        if (interfaces != null) {
            obj = interfaces.get(iid);
        }
        if (obj == null || !(obj instanceof Class)) {
            return null;
        }
        return (Class)obj;
    }
    
    public static Method getMethodByIndex(int index, IID iid) {
        Method result = null;
        MethodArray m = (MethodArray)iMethods.get(iid);
        if (m != null && m.methods !=null) {
            try {
                result = m.methods[index];
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        return result;
    }

    public static int getIndexByMethod(Method method, IID iid) {
        int result = -1;
        MethodArray m = (MethodArray)iMethods.get(iid);
        if (m != null && m.methods != null) {            
            for (int i = 0; i < m.methods.length; i++) {
                if (m.methods[i] != null && method.equals(m.methods[i])) {
                    result = i;
                    break;
                }
            }
        }
        return result;
    }

    private static String subscriptMethodName(String str) {
        if (keywords.get(str) != null) {
            return str + "_";
        }
        return str;
    }

    // methods for debugging

    private static void printMethods(Method[] methods) {  
        if (debug) {
            for (int i = 0; i < methods.length; i++) { 
                printMethod(methods[i]);
            }
        }
    }
    
    private static void printMethod(Method m) {
        if (m == null) {
            System.out.println("<null>");
            return;
        }
        Class retType = m.getReturnType();
        Class[] paramTypes = m.getParameterTypes();
        String name = m.getName();
        System.out.print(Modifier.toString(m.getModifiers()));
        System.out.print(" " + retType.getName() + " " + name 
                         + "(");
        for (int j = 0; j < paramTypes.length; j++) {
            if (j > 0) System.out.print(", ");
            System.out.print(paramTypes[j].getName());
        }
        System.out.println(");");
    }    
    
    private static void debug(String str) {
        if (debug) {
            System.out.println(str);
        }
    }

    private static String[] javaKeywords = {
        "abstract", "default", "if"        , "private"     , "this"     ,
        "boolean" , "do"     , "implements", "protected"   , "throw"    ,
        "break"   , "double" , "import",     "public"      , "throws"   ,
        "byte"    , "else"   , "instanceof", "return"      , "transient",
        "case"    , "extends", "int"       , "short"       , "try"      ,
        "catch"   , "final"  , "interface" , "static"      , "void"     ,
        "char"    , "finally", "long"      , "strictfp"    , "volatile" ,
        "class"   , "float"  , "native"    , "super"       , "while"    ,
        "const"   , "for"    , "new"       , "switch"      , 
        "continue", "goto"   , "package"   , "synchronized", 
        // non-final method names of Object class
        "toString", "clone"  , "finalize"  , "hashCode"    , "equals"};
}

class MethodArray {
    Method[] methods;
    Hashtable names;
    MethodArray(Method[] _methods, Hashtable _names) {
        methods = _methods;
        names = _names;
    }
}
