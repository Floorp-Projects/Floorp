/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* ** */

/* more doc todo:
 *  threads
 *  gc
 *  
 *
 */

package netscape.javascript;

import java.applet.Applet;

/**
 * JSObject allows Java to manipulate objects that are
 * defined in JavaScript.
 * Values passed from Java to JavaScript are converted as
 * follows:<ul>
 * <li>JSObject is converted to the original JavaScript object
 * <li>Any other Java object is converted to a JavaScript wrapper,
 *   which can be used to access methods and fields of the java object.
 *   Converting this wrapper to a string will call the toString method
 *   on the original object, converting to a number will call the
 *   floatValue method if possible and fail otherwise.  Converting
 *   to a boolean will try to call the booleanValue method in the
 *   same way.
 * <li>Java arrays are wrapped with a JavaScript object that understands
 *   array.length and array[index]
 * <li>A Java boolean is converted to a JavaScript boolean
 * <li>Java byte, char, short, int, long, float, and double are converted
 *   to JavaScript numbers
 * </ul>
 * Values passed from JavaScript to Java are converted as follows:<ul>
 * <li>objects which are wrappers around java objects are unwrapped
 * <li>other objects are wrapped with a JSObject
 * <li>strings, numbers and booleans are converted to String, Float,
 *   and Boolean objects respectively
 * </ul>
 * This means that all JavaScript values show up as some kind
 * of java.lang.Object in Java.  In order to make much use of them,
 * you will have to cast them to the appropriate subclass of Object,
 * e.g. <code>(String) window.getMember("name");</code> or
 * <code>(JSObject) window.getMember("document");</code>.
 */
public final class JSObject {
    /* the internal object data */
    private int                               internal;

    /**
     * initialize
     */
    private static native void initClass();
    static {
        SecurityManager.enablePrivilege("UniversalLinkAccess");
        SecurityManager.enablePrivilege("UniversalPropertyRead");
        System.loadLibrary(System.getProperty("netscape.jsj.dll", "jsj"));
        SecurityManager.revertPrivilege();
        initClass();
    }

    /**
     * it is illegal to construct a JSObject manually
     */
    private                          JSObject() {}

    /**
     * Retrieves a named member of a JavaScript object. 
     * Equivalent to "this.<i>name</i>" in JavaScript.
     */
    public native Object	getMember(String name);

    /**
     * Retrieves an indexed member of a JavaScript object.
     * Equivalent to "this[<i>index</i>]" in JavaScript.
     */
//    public Object		getMember(int index) { return getSlot(index); }
    public native Object	getSlot(int index);

    /**
     * Sets a named member of a JavaScript object. 
     * Equivalent to "this.<i>name</i> = <i>value</i>" in JavaScript.
     */
    public native void 		setMember(String name, Object value);

    /**
     * Sets an indexed member of a JavaScript object. 
     * Equivalent to "this[<i>index</i>] = <i>value</i>" in JavaScript.
     */
//    public void 		setMember(int index, Object value) {
//        setSlot(index, value);
//    }
    public native void 		setSlot(int index, Object value);

    /**
     * Removes a named member of a JavaScript object.
     */
    public native void 		removeMember(String name);

    /**
     * Calls a JavaScript method.
     * Equivalent to "this.<i>methodName</i>(<i>args</i>[0], <i>args</i>[1], ...)" in JavaScript.
     */
    public native Object	call(String methodName, Object args[]);

    /**
     * Evaluates a JavaScript expression. The expression is a string 
     * of JavaScript source code which will be evaluated in the context
     * given by "this".
     */
    public native Object	eval(String s);

    /**
     * Converts a JSObject to a String.
     */
    public native String        toString();

    // should use some sort of identifier rather than String
    // is "property" the right word?
  //    native String[]                         listProperties();


    /**
     * get a JSObject for the window containing the given applet
     */
    public static native JSObject	getWindow(Applet applet);

    /**
     * Finalization decrements the reference count on the corresponding
     * JavaScript object.
     */
    protected native void	finalize();
}
