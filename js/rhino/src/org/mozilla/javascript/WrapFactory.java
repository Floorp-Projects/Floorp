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
 * Norris Boyd
 * Igor Bukanov
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

// API class

package org.mozilla.javascript;

import org.mozilla.javascript.xml.XMLLib;

/**
 * Embeddings that wish to provide their own custom wrappings for Java
 * objects may extend this class and call
 * {@link Context#setWrapFactory(WrapFactory)}
 * Once an instance of this class or an extension of this class is enabled
 * for a given context (by calling setWrapFactory on that context), Rhino
 * will call the methods of this class whenever it needs to wrap a value
 * resulting from a call to a Java method or an access to a Java field.
 *
 * @see org.mozilla.javascript.Context#setWrapFactory(WrapFactory)
 * @since 1.5 Release 4
 */
public class WrapFactory
{
    /**
     * Wrap the object.
     * <p>
     * The value returned must be one of
     * <UL>
     * <LI>java.lang.Boolean</LI>
     * <LI>java.lang.String</LI>
     * <LI>java.lang.Number</LI>
     * <LI>org.mozilla.javascript.Scriptable objects</LI>
     * <LI>The value returned by Context.getUndefinedValue()</LI>
     * <LI>null</LI>
     * </UL>
     * @param cx the current Context for this thread
     * @param scope the scope of the executing script
     * @param obj the object to be wrapped. Note it can be null.
     * @param staticType type hint. If security restrictions prevent to wrap
              object based on its class, staticType will be used instead.
     * @return the wrapped value.
     */
    public Object wrap(Context cx, Scriptable scope,
                       Object obj, Class staticType)
    {
        if (obj == null)
            return obj;
        if (staticType != null && staticType.isPrimitive()) {
            if (staticType == Void.TYPE)
                return Undefined.instance;
            if (staticType == Character.TYPE)
                return new Integer((int) ((Character) obj).charValue());
            return obj;
        }
        if (obj instanceof Scriptable) {
            return obj;
        }
        if (!isJavaPrimitiveWrap()) {
            if (obj instanceof String || obj instanceof Number
                || obj instanceof Boolean)
            {
                return obj;
            } else if (obj instanceof Character) {
                return String.valueOf(((Character)obj).charValue());
            }
        }
        Class cls = obj.getClass();
        if (cls.isArray()) {
            return NativeJavaArray.wrap(scope, obj);
        }
        return wrapAsJavaObject(cx, scope, obj, staticType);
    }

    /**
     * Wrap an object newly created by a constructor call.
     * @param cx the current Context for this thread
     * @param scope the scope of the executing script
     * @param obj the object to be wrapped
     * @return the wrapped value.
     */
    public Scriptable wrapNewObject(Context cx, Scriptable scope, Object obj)
    {
        if (obj instanceof Scriptable) {
            return (Scriptable)obj;
        }
        Class cls = obj.getClass();
        if (cls.isArray()) {
            return NativeJavaArray.wrap(scope, obj);
        }
        return wrapAsJavaObject(cx, scope, obj, null);
    }

    /**
     * Wrap Java object as Scriptable instance to allow full access to its
     * methods and fields from JavaScript.
     * <p>
     * {@link #wrap(Context, Scriptable, Object, Class)} and
     * {@link #wrapNewObject(Context, Scriptable, Object)} call this method
     * when they can not convert <tt>javaObject</tt> to JavaScript primitive
     * value or JavaScript array.
     * <p>
     * Subclasses can override the method to provide custom wrappers
     * for Java objects.
     * @param cx the current Context for this thread
     * @param scope the scope of the executing script
     * @param javaObject the object to be wrapped
     * @param staticType type hint. If security restrictions prevent to wrap
                object based on its class, staticType will be used instead.
     * @return the wrapped value which shall not be null
     */
    public Scriptable wrapAsJavaObject(Context cx, Scriptable scope,
                                       Object javaObject, Class staticType)
    {
        Scriptable wrap = null;
        XMLLib lib = XMLLib.extractFromScopeOrNull(scope);
        if (lib != null) {
            wrap = lib.wrapAsXMLOrNull(cx, javaObject);
        }
        if (wrap == null) {
            wrap = new NativeJavaObject(scope, javaObject, staticType);
        }
        return wrap;
    }

    /**
     * Return <code>false</code> if result of Java method, which is instance of
     * <code>String</code>, <code>Number</code>, <code>Boolean</code> and
     * <code>Character</code>, should be used directly as JavaScript primitive
     * type.
     * By default the method returns true to indicate that instances of
     * <code>String</code>, <code>Number</code>, <code>Boolean</code> and
     * <code>Character</code> should be wrapped as any other Java object and
     * scripts can access any Java method available in these objects.
     * Use {@link #setJavaPrimitiveWrap(boolean)} to change this.
     */
    public final boolean isJavaPrimitiveWrap()
    {
        return javaPrimitiveWrap;
    }

    /**
     * @see #isJavaPrimitiveWrap()
     */
    public final void setJavaPrimitiveWrap(boolean value)
    {
        Context cx = Context.getCurrentContext();
        if (cx != null && cx.isSealed()) {
            Context.onSealedMutation();
        }
        javaPrimitiveWrap = value;
    }

    private boolean javaPrimitiveWrap = true;

}
