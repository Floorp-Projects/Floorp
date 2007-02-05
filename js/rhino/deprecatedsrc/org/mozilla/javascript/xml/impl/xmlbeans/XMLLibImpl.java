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
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Igor Bukanov
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

package org.mozilla.javascript.xml.impl.xmlbeans;

import java.io.Serializable;

import org.mozilla.javascript.*;
import org.mozilla.javascript.xml.*;

import org.apache.xmlbeans.XmlCursor;
import org.apache.xmlbeans.XmlObject;

public final class XMLLibImpl extends XMLLib implements Serializable
{
    private static final long serialVersionUID = 1L;

    private Scriptable globalScope;

    XML xmlPrototype;
    XMLList xmlListPrototype;
    Namespace namespacePrototype;
    QName qnamePrototype;


    // Environment settings...
    boolean ignoreComments;
    boolean ignoreProcessingInstructions;
    boolean ignoreWhitespace;
    boolean prettyPrinting;
    int prettyIndent;

    Scriptable globalScope()
    {
        return globalScope;
    }

    private XMLLibImpl(Scriptable globalScope)
    {
        this.globalScope = globalScope;
        defaultSettings();
    }

    public static void init(Context cx, Scriptable scope, boolean sealed)
    {
        // To force LinkageError if XmlObject is not available
        XmlObject.class.getName();

        XMLLibImpl lib = new XMLLibImpl(scope);
        XMLLib bound = lib.bindToScope(scope);
        if (bound == lib) {
            lib.exportToScope(sealed);
        }
    }

    private void exportToScope(boolean sealed)
    {
        xmlPrototype = XML.createEmptyXML(this);
        xmlListPrototype = new XMLList(this);
        namespacePrototype = new Namespace(this, "", "");
        qnamePrototype = new QName(this, "", "", "");

        xmlPrototype.exportAsJSClass(sealed);
        xmlListPrototype.exportAsJSClass(sealed);
        namespacePrototype.exportAsJSClass(sealed);
        qnamePrototype.exportAsJSClass(sealed);
    }

    void defaultSettings()
    {
        ignoreComments = true;
        ignoreProcessingInstructions = true;
        ignoreWhitespace = true;
        prettyPrinting = true;
        prettyIndent = 2;
    }

    XMLName toAttributeName(Context cx, Object nameValue)
    {
        String uri;
        String localName;

        if (nameValue instanceof String) {
            uri = "";
            localName = (String)nameValue;
        } else if (nameValue instanceof XMLName) {
            XMLName xmlName = (XMLName)nameValue;
            if (!xmlName.isAttributeName()) {
                xmlName.setAttributeName();
            }
            return xmlName;
        } else if (nameValue instanceof QName) {
            QName qname = (QName)nameValue;
            uri = qname.uri();
            localName = qname.localName();
        } else if (nameValue instanceof Boolean
                   || nameValue instanceof Number
                   || nameValue == Undefined.instance
                   || nameValue == null)
        {
            throw badXMLName(nameValue);
        } else {
            uri = "";
            localName = ScriptRuntime.toString(nameValue);
        }
        XMLName xmlName = XMLName.formProperty(uri, localName);
        xmlName.setAttributeName();
        return xmlName;
    }

    private static RuntimeException badXMLName(Object value)
    {
        String msg;
        if (value instanceof Number) {
            msg = "Can not construct XML name from number: ";
        } else if (value instanceof Boolean) {
            msg = "Can not construct XML name from boolean: ";
        } else if (value == Undefined.instance || value == null) {
            msg = "Can not construct XML name from ";
        } else {
            throw new IllegalArgumentException(value.toString());
        }
        return ScriptRuntime.typeError(msg+ScriptRuntime.toString(value));
    }

    XMLName toXMLName(Context cx, Object nameValue)
    {
        XMLName result;

        if (nameValue instanceof XMLName) {
            result = (XMLName)nameValue;
        } else if (nameValue instanceof QName) {
            QName qname = (QName)nameValue;
            result = XMLName.formProperty(qname.uri(), qname.localName());
        } else if (nameValue instanceof String) {
            result = toXMLNameFromString(cx, (String)nameValue);
        } else if (nameValue instanceof Boolean
                   || nameValue instanceof Number
                   || nameValue == Undefined.instance
                   || nameValue == null)
        {
            throw badXMLName(nameValue);
        } else {
            String name = ScriptRuntime.toString(nameValue);
            result = toXMLNameFromString(cx, name);
        }

        return result;
    }

    /**
     * If value represents Uint32 index, make it available through
     * ScriptRuntime.lastUint32Result(cx) and return null.
     * Otherwise return the same value as toXMLName(cx, value).
     */
    XMLName toXMLNameOrIndex(Context cx, Object value)
    {
        XMLName result;

        if (value instanceof XMLName) {
            result = (XMLName)value;
        } else if (value instanceof String) {
            String str = (String)value;
            long test = ScriptRuntime.testUint32String(str);
            if (test >= 0) {
                ScriptRuntime.storeUint32Result(cx, test);
                result = null;
            } else {
                result = toXMLNameFromString(cx, str);
            }
        } else if (value instanceof Number) {
            double d = ((Number)value).doubleValue();
            long l = (long)d;
            if (l == d && 0 <= l && l <= 0xFFFFFFFFL) {
                ScriptRuntime.storeUint32Result(cx, l);
                result = null;
            } else {
                throw badXMLName(value);
            }
        } else if (value instanceof QName) {
            QName qname = (QName)value;
            String uri = qname.uri();
            boolean number = false;
            result = null;
            if (uri != null && uri.length() == 0) {
                // Only in this case qname.toString() can resemble uint32
                long test = ScriptRuntime.testUint32String(uri);
                if (test >= 0) {
                    ScriptRuntime.storeUint32Result(cx, test);
                    number = true;
                }
            }
            if (!number) {
                result = XMLName.formProperty(uri, qname.localName());
            }
        } else if (value instanceof Boolean
                   || value == Undefined.instance
                   || value == null)
        {
            throw badXMLName(value);
        } else {
            String str = ScriptRuntime.toString(value);
            long test = ScriptRuntime.testUint32String(str);
            if (test >= 0) {
                ScriptRuntime.storeUint32Result(cx, test);
                result = null;
            } else {
                result = toXMLNameFromString(cx, str);
            }
        }

        return result;
    }

    XMLName toXMLNameFromString(Context cx, String name)
    {
        if (name == null)
            throw new IllegalArgumentException();

        int l = name.length();
        if (l != 0) {
            char firstChar = name.charAt(0);
            if (firstChar == '*') {
                if (l == 1) {
                    return XMLName.formStar();
                }
            } else if (firstChar == '@') {
                XMLName xmlName = XMLName.formProperty("", name.substring(1));
                xmlName.setAttributeName();
                return xmlName;
            }
        }

        String uri = getDefaultNamespaceURI(cx);

        return XMLName.formProperty(uri, name);
    }

    Namespace constructNamespace(Context cx, Object uriValue)
    {
        String prefix;
        String uri;

        if (uriValue instanceof Namespace) {
            Namespace ns = (Namespace)uriValue;
            prefix = ns.prefix();
            uri = ns.uri();
        } else if (uriValue instanceof QName) {
            QName qname = (QName)uriValue;
            uri = qname.uri();
            if (uri != null) {
                prefix = qname.prefix();
            } else {
                uri = qname.toString();
                prefix = null;
            }
        } else {
            uri = ScriptRuntime.toString(uriValue);
            prefix = (uri.length() == 0) ? "" : null;
        }

        return new Namespace(this, prefix, uri);
    }

    Namespace castToNamespace(Context cx, Object namescapeObj)
    {
        if (namescapeObj instanceof Namespace) {
            return (Namespace)namescapeObj;
        }
        return constructNamespace(cx, namescapeObj);
    }

    Namespace constructNamespace(Context cx)
    {
        return new Namespace(this, "", "");
    }

    public Namespace constructNamespace(Context cx, Object prefixValue,
                                        Object uriValue)
    {
        String prefix;
        String uri;

        if (uriValue instanceof QName) {
            QName qname = (QName)uriValue;
            uri = qname.uri();
            if (uri == null) {
                uri = qname.toString();
            }
        } else {
            uri = ScriptRuntime.toString(uriValue);
        }

        if (uri.length() == 0) {
            if (prefixValue == Undefined.instance) {
                prefix = "";
            } else {
                prefix = ScriptRuntime.toString(prefixValue);
                if (prefix.length() != 0) {
                    throw ScriptRuntime.typeError(
                        "Illegal prefix '"+prefix+"' for 'no namespace'.");
                }
            }
        } else if (prefixValue == Undefined.instance) {
            prefix = "";
        } else if (!isXMLName(cx, prefixValue)) {
            prefix = "";
        } else {
            prefix = ScriptRuntime.toString(prefixValue);
        }

        return new Namespace(this, prefix, uri);
    }

    String getDefaultNamespaceURI(Context cx)
    {
        String uri = "";
        if (cx == null) {
            cx = Context.getCurrentContext();
        }
        if (cx != null) {
            Object ns = ScriptRuntime.searchDefaultNamespace(cx);
            if (ns != null) {
                if (ns instanceof Namespace) {
                    uri = ((Namespace)ns).uri();
                } else {
                    // Should not happen but for now it could
                    // due to bad searchDefaultNamespace implementation.
                }
            }
        }
        return uri;
    }

    Namespace getDefaultNamespace(Context cx)
    {
        if (cx == null) {
            cx = Context.getCurrentContext();
            if (cx == null) {
                return namespacePrototype;
            }
        }

        Namespace result;
        Object ns = ScriptRuntime.searchDefaultNamespace(cx);
        if (ns == null) {
            result = namespacePrototype;
        } else {
            if (ns instanceof Namespace) {
                result = (Namespace)ns;
            } else {
                // Should not happen but for now it could
                // due to bad searchDefaultNamespace implementation.
                result = namespacePrototype;
            }
        }
        return result;
    }

    QName castToQName(Context cx, Object qnameValue)
    {
        if (qnameValue instanceof QName) {
            return (QName)qnameValue;
        }
        return constructQName(cx, qnameValue);
    }

    QName constructQName(Context cx, Object nameValue)
    {
        QName result;

        if (nameValue instanceof QName) {
            QName qname = (QName)nameValue;
            result = new QName(this, qname.uri(), qname.localName(),
                               qname.prefix());
        } else {
            String localName = ScriptRuntime.toString(nameValue);
            result = constructQNameFromString(cx, localName);
        }

        return result;
    }

    /**
     * Optimized version of constructQName for String type
     */
    QName constructQNameFromString(Context cx, String localName)
    {
        if (localName == null)
            throw new IllegalArgumentException();

        String uri;
        String prefix;

        if ("*".equals(localName)) {
            uri = null;
            prefix = null;
        } else {
            Namespace ns = getDefaultNamespace(cx);
            uri = ns.uri();
            prefix = ns.prefix();
        }

        return new QName(this, uri, localName, prefix);
    }

    QName constructQName(Context cx, Object namespaceValue, Object nameValue)
    {
        String uri;
        String localName;
        String prefix;

        if (nameValue instanceof QName) {
            QName qname = (QName)nameValue;
            localName = qname.localName();
        } else {
            localName = ScriptRuntime.toString(nameValue);
        }

        Namespace ns;
        if (namespaceValue == Undefined.instance) {
            if ("*".equals(localName)) {
                ns = null;
            } else {
                ns = getDefaultNamespace(cx);
            }
        } else if (namespaceValue == null) {
            ns = null;
        } else if (namespaceValue instanceof Namespace) {
            ns = (Namespace)namespaceValue;
        } else {
            ns = constructNamespace(cx, namespaceValue);
        }

        if (ns == null) {
            uri = null;
            prefix = null;
        } else {
            uri = ns.uri();
            prefix = ns.prefix();
        }

        return new QName(this, uri, localName, prefix);
    }

    Object addXMLObjects(Context cx, XMLObject obj1, XMLObject obj2)
    {
        XMLList listToAdd = new XMLList(this);

        if (obj1 instanceof XMLList) {
            XMLList list1 = (XMLList)obj1;
            if (list1.length() == 1) {
                listToAdd.addToList(list1.item(0));
            } else {
                // Might be xmlFragment + xmlFragment + xmlFragment + ...;
                // then the result will be an XMLList which we want to be an
                // rValue and allow it to be assigned to an lvalue.
                listToAdd = new XMLList(this, obj1);
            }
        } else {
            listToAdd.addToList(obj1);
        }

        if (obj2 instanceof XMLList) {
            XMLList list2 = (XMLList)obj2;
            for (int i = 0; i < list2.length(); i++) {
                listToAdd.addToList(list2.item(i));
            }
        } else if (obj2 instanceof XML) {
            listToAdd.addToList(obj2);
        }

        return listToAdd;
    }

    //
    //
    // Overriding XMLLib methods
    //
    //

    /**
     * See E4X 13.1.2.1.
     */
    public boolean isXMLName(Context cx, Object nameObj)
    {
        String name;
        try {
            name = ScriptRuntime.toString(nameObj);
        } catch (EcmaError ee) {
            if ("TypeError".equals(ee.getName())) {
                return false;
            }
            throw ee;
        }

        // See http://w3.org/TR/xml-names11/#NT-NCName
        int length = name.length();
        if (length != 0) {
            if (isNCNameStartChar(name.charAt(0))) {
                for (int i = 1; i != length; ++i) {
                    if (!isNCNameChar(name.charAt(i))) {
                        return false;
                    }
                }
                return true;
            }
        }

        return false;
    }

    private static boolean isNCNameStartChar(int c)
    {
        if ((c & ~0x7F) == 0) {
            // Optimize for ASCII and use A..Z < _ < a..z
            if (c >= 'a') {
                return c <= 'z';
            } else if (c >= 'A') {
                if (c <= 'Z') {
                    return true;
                }
                return c == '_';
            }
        } else if ((c & ~0x1FFF) == 0) {
            return (0xC0 <= c && c <= 0xD6)
                   || (0xD8 <= c && c <= 0xF6)
                   || (0xF8 <= c && c <= 0x2FF)
                   || (0x370 <= c && c <= 0x37D)
                   || 0x37F <= c;
        }
        return (0x200C <= c && c <= 0x200D)
               || (0x2070 <= c && c <= 0x218F)
               || (0x2C00 <= c && c <= 0x2FEF)
               || (0x3001 <= c && c <= 0xD7FF)
               || (0xF900 <= c && c <= 0xFDCF)
               || (0xFDF0 <= c && c <= 0xFFFD)
               || (0x10000 <= c && c <= 0xEFFFF);
    }

    private static boolean isNCNameChar(int c)
    {
        if ((c & ~0x7F) == 0) {
            // Optimize for ASCII and use - < . < 0..9 < A..Z < _ < a..z
            if (c >= 'a') {
                return c <= 'z';
            } else if (c >= 'A') {
                if (c <= 'Z') {
                    return true;
                }
                return c == '_';
            } else if (c >= '0') {
                return c <= '9';
            } else {
                return c == '-' || c == '.';
            }
        } else if ((c & ~0x1FFF) == 0) {
            return isNCNameStartChar(c) || c == 0xB7
                   || (0x300 <= c && c <= 0x36F);
        }
        return isNCNameStartChar(c) || (0x203F <= c && c <= 0x2040);
    }

    XMLName toQualifiedName(Context cx, Object namespaceValue,
                            Object nameValue)
    {
        // This is duplication of constructQName(cx, namespaceValue, nameValue)
        // but for XMLName

        String uri;
        String localName;

        if (nameValue instanceof QName) {
            QName qname = (QName)nameValue;
            localName = qname.localName();
        } else {
            localName = ScriptRuntime.toString(nameValue);
        }

        Namespace ns;
        if (namespaceValue == Undefined.instance) {
            if ("*".equals(localName)) {
                ns = null;
            } else {
                ns = getDefaultNamespace(cx);
            }
        } else if (namespaceValue == null) {
            ns = null;
        } else if (namespaceValue instanceof Namespace) {
            ns = (Namespace)namespaceValue;
        } else {
            ns = constructNamespace(cx, namespaceValue);
        }

        if (ns == null) {
            uri = null;
        } else {
            uri = ns.uri();
        }

        return XMLName.formProperty(uri, localName);
    }

    public Ref nameRef(Context cx, Object name,
                       Scriptable scope, int memberTypeFlags)
    {
        if ((memberTypeFlags & Node.ATTRIBUTE_FLAG) == 0) {
            // should only be called foir cases like @name or @[expr]
            throw Kit.codeBug();
        }
        XMLName xmlName = toAttributeName(cx, name);
        return xmlPrimaryReference(cx, xmlName, scope);
    }

    public Ref nameRef(Context cx, Object namespace, Object name,
                       Scriptable scope, int memberTypeFlags)
    {
        XMLName xmlName = toQualifiedName(cx, namespace, name);
        if ((memberTypeFlags & Node.ATTRIBUTE_FLAG) != 0) {
            if (!xmlName.isAttributeName()) {
                xmlName.setAttributeName();
            }
        }
        return xmlPrimaryReference(cx, xmlName, scope);
    }

    private Ref xmlPrimaryReference(Context cx, XMLName xmlName,
                                    Scriptable scope)
    {
        XMLObjectImpl xmlObj;
        XMLObjectImpl firstXmlObject = null;
        for (;;) {
            // XML object can only present on scope chain as a wrapper
            // of XMLWithScope
            if (scope instanceof XMLWithScope) {
                xmlObj = (XMLObjectImpl)scope.getPrototype();
                if (xmlObj.hasXMLProperty(xmlName)) {
                    break;
                }
                if (firstXmlObject == null) {
                    firstXmlObject = xmlObj;
                }
            }
            scope = scope.getParentScope();
            if (scope == null) {
                xmlObj = firstXmlObject;
                break;
            }
        }

        // xmlObj == null corresponds to undefined as the target of
        // the reference
        if (xmlObj != null) {
            xmlName.initXMLObject(xmlObj);
        }
        return xmlName;
    }

    /**
     * Escapes the reserved characters in a value of an attribute
     *
     * @param value Unescaped text
     * @return The escaped text
     */
    public String escapeAttributeValue(Object value)
    {
        String text = ScriptRuntime.toString(value);

        if (text.length() == 0) return "";

        XmlObject xo = XmlObject.Factory.newInstance();

        XmlCursor cursor = xo.newCursor();
        cursor.toNextToken();
        cursor.beginElement("a");
        cursor.insertAttributeWithValue("a", text);
        cursor.dispose();

        String elementText = xo.toString();
        int begin = elementText.indexOf('"');
        int end = elementText.lastIndexOf('"');
        return elementText.substring(begin + 1, end);
    }

    /**
     * Escapes the reserved characters in a value of a text node
     *
     * @param value Unescaped text
     * @return The escaped text
     */
    public String escapeTextValue(Object value)
    {
        if (value instanceof XMLObjectImpl) {
            return ((XMLObjectImpl)value).toXMLString(0);
        }

        String text = ScriptRuntime.toString(value);

        if (text.length() == 0) return text;

        XmlObject xo = XmlObject.Factory.newInstance();

        XmlCursor cursor = xo.newCursor();
        cursor.toNextToken();
        cursor.beginElement("a");
        cursor.insertChars(text);
        cursor.dispose();

        String elementText = xo.toString();
        int begin = elementText.indexOf('>') + 1;
        int end = elementText.lastIndexOf('<');
        return (begin < end) ? elementText.substring(begin, end) : "";
    }

    public Object toDefaultXmlNamespace(Context cx, Object uriValue)
    {
        return constructNamespace(cx, uriValue);
    }
}
