/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
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
 * Igor Bukanov
 * Ethan Hugg
 * Terry Lucas
 * Milen Nankov
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

package org.mozilla.javascript.xml;

import org.mozilla.javascript.*;

/**
 *  This Interface describes what all XML objects (XML, XMLList) should have in common.
 *
 * @see XML
 */
public abstract class XMLObject extends IdScriptableObject
{
    public XMLObject()
    {
    }

    public XMLObject(Scriptable scope, Scriptable prototype)
    {
        super(scope, prototype);
    }

    /**
     * Implementation of ECMAScript [[Has]].
     */
    public abstract boolean ecmaHas(Context cx, Object id);

    /**
     * Implementation of ECMAScript [[Get]].
     */
    public abstract Object ecmaGet(Context cx, Object id);

    /**
     * Implementation of ECMAScript [[Put]].
     */
    public abstract void ecmaPut(Context cx, Object id, Object value);

    /**
     * Implementation of ECMAScript [[Delete]].
     */
    public abstract boolean ecmaDelete(Context cx, Object id);

    /**
     * To implement ECMAScript [[Descendants]].
     */
    public abstract Reference getDescendantsRef(Context cx, Object id);

    /**
     * Wrap this object into NativeWith to implement the with statement.
     */
    public abstract NativeWith enterWith(Scriptable scope);

    /**
     * Wrap this object into NativeWith to implement the .() query.
     */
    public abstract NativeWith enterDotQuery(Scriptable scope);

    /**
     * Custom <tt>+</tt> operator.
     * Should return {@link Scriptable#NOT_FOUND} if this object does not have
     * custom addition operator for the given value,
     * or the result of the addition operation.
     * <p>
     * The default implementation returns {@link Scriptable#NOT_FOUND}
     * to indicate no custom addition operation.
     *
     * @param cx the Context object associated with the current thread.
     * @param thisIsLeft if true, the object should calculate this + value
     *                   if false, the object should calculate value + this.
     * @param value the second argument for addition operation.
     */
    public Object addValues(Context cx, boolean thisIsLeft, Object value)
    {
        return Scriptable.NOT_FOUND;
    }

}
