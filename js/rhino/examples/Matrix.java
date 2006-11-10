/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0
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
 * The Original Code is Rhino code, released
 * May 6, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 or later (the "GPL"), in which
 * case the provisions of the GPL are applicable instead of those above. If
 * you wish to allow use of your version of this file only under the terms of
 * the GPL and not to allow others to use your version of this file under the
 * MPL, indicate your decision by deleting the provisions above and replacing
 * them with the notice and other provisions required by the GPL. If you do
 * not delete the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * ***** END LICENSE BLOCK ***** */

import org.mozilla.javascript.*;
import java.util.Vector;

/**
 * Matrix: An example host object class that implements the Scriptable interface.
 *
 * Built-in JavaScript arrays don't handle multiple dimensions gracefully: the
 * script writer must create every array in an array of arrays. The Matrix class
 * takes care of that by automatically allocating arrays for every index that
 * is accessed. What's more, the Matrix constructor takes a integer argument
 * that specifies the dimension of the Matrix. If m is a Matrix with dimension 3,
 * then m[0] will be a Matrix with dimension 1, and m[0][0] will be an Array.
 *
 * Here's a shell session showing the Matrix object in action:
 * <pre>
 * js> defineClass("Matrix")
 * js> m = new Matrix(2);              <i>A constructor call, see <a href="#Matrix">Matrix</a> below.</i>
 * [object Matrix]                     <i>The "Matrix" here comes from <a href"#getClassName">getClassName</a>.</i>
 * js> version(120);                   <i>switch to JavaScript1.2 to see arrays better</i>
 * 0
 * js> m[0][0] = 3;
 * 3
 * js> m[0];                           <i>an array was created automatically!</i>
 * [3]
 * js> m[1];                           <i>array is created even if we don't set a value</i>
 * []
 * js> m.dim;                          <i>we can access the "dim" property</i>
 * 2
 * js> m.dim = 3;
 * 3
 * js> m.dim;                          <i>but not modify it</i>
 * 2
 * </pre>
 *
 * @see org.mozilla.javascript.Context
 * @see org.mozilla.javascript.Scriptable
 *
 * @author Norris Boyd
 */
public class Matrix implements Scriptable {

    /**
     * The zero-parameter constructor.
     *
     * When ScriptableObject.defineClass is called with this class, it will
     * construct Matrix.prototype using this constructor.
     */
    public Matrix() {
    }

    /**
     * The Java constructor, also used to define the JavaScript constructor.
     */
    public Matrix(int dimension) {
        if (dimension <= 0) {
            throw Context.reportRuntimeError(
                  "Dimension of Matrix must be greater than zero");
        }
        dim = dimension;
        v = new Vector();
    }

    /**
     * Returns the name of this JavaScript class, "Matrix".
     */
    public String getClassName() {
        return "Matrix";
    }

    /**
     * Defines the "dim" property by returning true if name is
     * equal to "dim".
     * <p>
     * Defines no other properties, i.e., returns false for
     * all other names.
     *
     * @param name the name of the property
     * @param start the object where lookup began
     */
    public boolean has(String name, Scriptable start) {
        return name.equals("dim");
    }

    /**
     * Defines all numeric properties by returning true.
     *
     * @param index the index of the property
     * @param start the object where lookup began
     */
    public boolean has(int index, Scriptable start) {
        return true;
    }

    /**
     * Get the named property.
     * <p>
     * Handles the "dim" property and returns NOT_FOUND for all
     * other names.
     * @param name the property name
     * @param start the object where the lookup began
     */
    public Object get(String name, Scriptable start) {
        if (name.equals("dim"))
            return new Integer(dim);

        return NOT_FOUND;
    }

    /**
     * Get the indexed property.
     * <p>
     * Look up the element in the associated vector and return
     * it if it exists. If it doesn't exist, create it.<p>
     * @param index the index of the integral property
     * @param start the object where the lookup began
     */
    public Object get(int index, Scriptable start) {
        if (index >= v.size())
            v.setSize(index+1);
        Object result = v.elementAt(index);
        if (result != null)
            return result;
        if (dim > 2) {
            Matrix m = new Matrix(dim-1);
            m.setParentScope(getParentScope());
            m.setPrototype(getPrototype());
            result = m;
        } else {
            Context cx = Context.getCurrentContext();
            Scriptable scope = ScriptableObject.getTopLevelScope(start);
            result = cx.newArray(scope, 0);
        }
        v.setElementAt(result, index);
        return result;
    }

    /**
     * Set a named property.
     *
     * We do nothing here, so all properties are effectively read-only.
     */
    public void put(String name, Scriptable start, Object value) {
    }

    /**
     * Set an indexed property.
     *
     * We do nothing here, so all properties are effectively read-only.
     */
    public void put(int index, Scriptable start, Object value) {
    }

    /**
     * Remove a named property.
     *
     * This method shouldn't even be called since we define all properties
     * as PERMANENT.
     */
    public void delete(String id) {
    }

    /**
     * Remove an indexed property.
     *
     * This method shouldn't even be called since we define all properties
     * as PERMANENT.
     */
    public void delete(int index) {
    }

    /**
     * Get prototype.
     */
    public Scriptable getPrototype() {
        return prototype;
    }

    /**
     * Set prototype.
     */
    public void setPrototype(Scriptable prototype) {
        this.prototype = prototype;
    }

    /**
     * Get parent.
     */
    public Scriptable getParentScope() {
        return parent;
    }

    /**
     * Set parent.
     */
    public void setParentScope(Scriptable parent) {
        this.parent = parent;
    }

    /**
     * Get properties.
     *
     * We return an empty array since we define all properties to be DONTENUM.
     */
    public Object[] getIds() {
        return new Object[0];
    }

    /**
     * Default value.
     *
     * Use the convenience method from Context that takes care of calling
     * toString, etc.
     */
    public Object getDefaultValue(Class typeHint) {
        return "[object Matrix]";
    }

    /**
     * instanceof operator.
     *
     * We mimick the normal JavaScript instanceof semantics, returning
     * true if <code>this</code> appears in <code>value</code>'s prototype
     * chain.
     */
    public boolean hasInstance(Scriptable value) {
        Scriptable proto = value.getPrototype();
        while (proto != null) {
            if (proto.equals(this))
                return true;
            proto = proto.getPrototype();
        }

        return false;
    }

    /**
     * Some private data for this class.
     */
    private int dim;
    private Vector v;
    private Scriptable prototype, parent;
}
