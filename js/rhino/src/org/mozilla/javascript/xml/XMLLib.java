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

public abstract class XMLLib
{
    private static final Object XML_LIB_KEY = new Object();

    public static XMLLib extractFromScopeOrNull(Scriptable scope)
    {
        ScriptableObject so = ScriptRuntime.getLibraryScopeOrNull(scope);
        if (so == null) {
            // If librray is not yet initialized, return null
            return null;
        }

        // Ensure lazily initialization of real XML library instance
        // which is done on first access to XML property
        ScriptableObject.getProperty(so, "XML");

        return (XMLLib)so.getAssociatedValue(XML_LIB_KEY);
    }

    public static XMLLib extractFromScope(Scriptable scope)
    {
        XMLLib lib = extractFromScopeOrNull(scope);
        if (lib != null) {
            return lib;
        }
        String msg = ScriptRuntime.getMessage0("msg.XML.not.available");
        throw Context.reportRuntimeError(msg);
    }

    protected final XMLLib bindToScope(Scriptable scope)
    {
        ScriptableObject so = ScriptRuntime.getLibraryScopeOrNull(scope);
        if (so == null) {
            // standard library should be initialized at this point
            throw new IllegalStateException();
        }
        return (XMLLib)so.associateValue(XML_LIB_KEY, this);
    }

    public abstract boolean isXMLName(Context cx, Object name);

    public abstract Object toQualifiedName(String namespace,
                                           Object nameValue,
                                           Scriptable scope);

    public abstract Object toAttributeName(Context cx, Object nameValue);

    public abstract Object toDescendantsName(Context cx, Object name);

    public abstract Reference xmlPrimaryReference(Object nameObject,
                                                  Scriptable scope);

    /**
     * Escapes the reserved characters in a value of an attribute
     * and surround it by "".
     *
     * @param value Unescaped text
     * @return The escaped text
     */
    public abstract String escapeAttributeValue(Object value);

    /**
     * Escapes the reserved characters in a value of a text node
     *
     * @param value Unescaped text
     * @return The escaped text
     */
    public abstract String escapeTextValue(Object value);


    /**
     * Construct namespace for default xml statement.
     */
    public abstract Object toDefaultXmlNamespace(Context cx, Object uriValue);

    /**
     * Return wrap of the given Java object as appropritate XML object or
     * null if Java object has no XML representation.
     * The default implementation returns null to indicate no special
     * wrapping of XML objects.
     */
    public Scriptable wrapAsXMLOrNull(Context cx, Object javaObject)
    {
        return null;
    }
}
