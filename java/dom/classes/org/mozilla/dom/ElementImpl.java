/* 
 The contents of this file are subject to the Mozilla Public
 License Version 1.1 (the "License"); you may not use this file
 except in compliance with the License. You may obtain a copy of
 the License at http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS
 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 implied. See the License for the specific language governing
 rights and limitations under the License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is Sun Microsystems,
 Inc. Portions created by Sun are
 Copyright (C) 1999 Sun Microsystems, Inc. All
 Rights Reserved.

 Contributor(s): 
*/

package org.mozilla.dom;

import org.w3c.dom.Attr;
import org.w3c.dom.Element;
import org.w3c.dom.NodeList;
import org.w3c.dom.DOMException;
import org.w3c.dom.TypeInfo;

public class ElementImpl extends NodeImpl implements Element {

    // instantiated from JNI or Document.createElement()
    private ElementImpl() {}

    public String getAttribute(String name) {
	return nativeGetAttribute(name);
    }
    native String nativeGetAttribute(String name);

    public Attr getAttributeNode(String name) {
	return nativeGetAttributeNode(name);
    }
    native Attr nativeGetAttributeNode(String name);

    public NodeList getElementsByTagName(String name) {
	return nativeGetElementsByTagName(name);
    }
    native NodeList nativeGetElementsByTagName(String name);

    public String getTagName() {
	return nativeGetTagName();
    }
    native String nativeGetTagName();

    public void normalize() {
	nativeNormalize();
    }
    native void nativeNormalize();

    public void removeAttribute(String name) {
	nativeRemoveAttribute(name);
    }
    native void nativeRemoveAttribute(String name);

    public Attr removeAttributeNode(Attr oldAttr) {
	return nativeRemoveAttributeNode(oldAttr);
    }
    native Attr nativeRemoveAttributeNode(Attr oldAttr);

    public void setAttribute(String name, String value) {
	nativeSetAttribute(name, value);
    }
    native void nativeSetAttribute(String name, String value);

    public Attr setAttributeNode(Attr newAttr) {
	return nativeSetAttributeNode(newAttr);
    }
    native Attr nativeSetAttributeNode(Attr newAttr);


    //since DOM2
    public String getAttributeNS(String namespaceURI, String localName) {
	return nativeGetAttributeNS(namespaceURI, localName);
    }
    native String nativeGetAttributeNS(String namespaceURI, String localName);

    public void setAttributeNS(String namespaceURI, String qualifiedName, String value) {
	nativeSetAttributeNS(namespaceURI, qualifiedName, value);
    }
    native void nativeSetAttributeNS(String namespaceURI, String qualifiedName, String value);

    public void removeAttributeNS(String namespacURI, String localName) {
	nativeRemoveAttributeNS(namespacURI, localName);
    }
    native void nativeRemoveAttributeNS(String namespacURI, String localName);   

    public Attr getAttributeNodeNS(String namespaceURI, String localName) {
	return nativeGetAttributeNodeNS(namespaceURI, localName);
    }
    native Attr nativeGetAttributeNodeNS(String namespaceURI, String localName);

    public Attr setAttributeNodeNS(Attr newAttr) {
	return nativeSetAttributeNodeNS(newAttr);
    }
    native Attr nativeSetAttributeNodeNS(Attr newAttr);

    public NodeList getElementsByTagNameNS(String namespaceURI, String localName) {
	return nativeGetElementsByTagNameNS(namespaceURI, localName);
    }
    native NodeList nativeGetElementsByTagNameNS(String namespaceURI, String localName);

    public boolean hasAttribute(String name) {
	return nativeHasAttribute(name);
    }
    native boolean nativeHasAttribute(String name);

    public boolean hasAttributeNS(String namespaceURI, String localName) {
	return nativeHasAttributeNS(namespaceURI, localName);
    }
    native boolean nativeHasAttributeNS(String namespaceURI, String localName);

    public TypeInfo getSchemaTypeInfo() {
        throw new UnsupportedOperationException();
    }

    public void setIdAttributeNS(String namespaceURI, String localName, boolean isId) throws DOMException {
        throw new UnsupportedOperationException();
    }

    public void setIdAttribute(String name, boolean isId) throws DOMException {
        throw new UnsupportedOperationException();
    }

    public void setIdAttributeNode(Attr idAttr, boolean isId) throws DOMException {
        throw new UnsupportedOperationException();
    }
}
