/* -*- Mode: Java; tab-width: 8; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
 *   doubleValue method if possible and fail otherwise.  Converting
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
 * <li>strings, numbers and booleans are converted to String, Double,
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
    private long                              long_internal;

    /**
     * initialize
     */
    private static native void initClass();
    static {
    	// On MRJ, this property won't exist, because the library is preloaded.
    	String liveConnectLibrary = System.getProperty("netscape.jsj.dll", null);
    	if (liveConnectLibrary != null) {
			System.loadLibrary(liveConnectLibrary);
			initClass();
		}
    }

    /**
     * it is illegal to construct a JSObject manually
     */
    private JSObject(int jsobj_addr) {
        internal = jsobj_addr;
    }

    private JSObject(long jsobj_addr) {
        long_internal = jsobj_addr;
    }

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

    /**
     * Override java.lang.Object.equals() because identity is not preserved
     * with instances of JSObject.
     */
    public native boolean equals(Object obj);
}
