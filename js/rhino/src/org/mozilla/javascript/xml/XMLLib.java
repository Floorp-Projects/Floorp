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
    public static XMLLib extractFromScopeOrNull(Scriptable scope)
    {
        scope = ScriptableObject.getTopLevelScope(scope);
        Object testXML = ScriptableObject.getClassPrototype(scope, "XML");
        if (testXML instanceof XMLObject) {
            return ((XMLObject)testXML).lib();
        }
        return null;
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

    public abstract boolean isXMLName(Context cx, Object name);

    public abstract Object toQualifiedName(String namespace,
                                           Object nameValue,
                                           Scriptable scope);

    public abstract Object toAttributeName(Context cx, Object nameValue);

    public abstract Object toDescendantsName(Context cx, Object name);

    public abstract Reference xmlPrimaryReference(Object nameObject,
                                                  Scriptable scope);

    public abstract Scriptable enterXMLWith(XMLObject object,
                                            Scriptable scope);

    public abstract Scriptable enterDotQuery(XMLObject object,
                                             Scriptable scope);

    /**
     * Escapes the reserved characters in a value of an attribute
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
     * Must calculate obj1 + obj2
     */
    public abstract Object addXMLObjects(Context cx,
                                         XMLObject obj1,
                                         XMLObject obj2);

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
