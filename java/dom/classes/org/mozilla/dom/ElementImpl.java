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

import org.mozilla.util.ReturnRunnable;


public class ElementImpl extends NodeImpl implements Element {

    // instantiated from JNI or Document.createElement()
    private ElementImpl() {}

    public String getAttribute(String name) {
	final String finalName = name;
	String result = (String)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetAttribute(finalName);
		    }
		    public String toString() {
			return "Element.getAttribute";
		    }
		});
	return result;

    }
    native String nativeGetAttribute(String name);

    public Attr getAttributeNode(String name) {
	final String finalName = name;
	Attr result = (Attr)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetAttributeNode(finalName);
		    }
		    public String toString() {
			return "Element.getAttributeNode";
		    }
		});
	return result;

    }
    native Attr nativeGetAttributeNode(String name);

    public NodeList getElementsByTagName(String name) {
	final String finalName = name;
	NodeList result = (NodeList)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetElementsByTagName(finalName);
		    }
		    public String toString() {
			return "Element.getElementsByTagName";
		    }
		});
	return result;

    }
    native NodeList nativeGetElementsByTagName(String name);

    public String getTagName() {
	String result = (String)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetTagName();
		    }
		    public String toString() {
			return "Element.getTagName";
		    }
		});
	return result;

    }
    native String nativeGetTagName();

    public void normalize() {
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			nativeNormalize();
			return null;
		    }
		    public String toString() {
			return "Element.normalize";
		    }
		});

    }
    native void nativeNormalize();

    public void removeAttribute(String name) {
	final String finalName = name;
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			nativeRemoveAttribute(finalName);
			return null;
		    }
		    public String toString() {
			return "Element.removeAttribute";
		    }
		});


    }
    native void nativeRemoveAttribute(String name);

    public Attr removeAttributeNode(Attr oldAttr) {
	final Attr finalOldAttr = oldAttr;
	Attr result = (Attr)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeRemoveAttributeNode(finalOldAttr);
		    }
		    public String toString() {
			return "Element.removeAttributeNode";
		    }
		});
	return result;

    }
    native Attr nativeRemoveAttributeNode(Attr oldAttr);

    public void setAttribute(String name, String value) {
	final String finalName = name;
	final  String finalValue = value;
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			nativeSetAttribute(finalName, finalValue);
			return null;
		    }
		    public String toString() {
			return "Element.setAttribute";
		    }
		});

    }
    native void nativeSetAttribute(String name, String value);

    public Attr setAttributeNode(Attr newAttr) {
	final Attr finalNewAttr = newAttr;
	Attr result = (Attr)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeSetAttributeNode(finalNewAttr);
		    }
		    public String toString() {
			return "Element.setAttributeNode";
		    }
		});
	return result;

    }
    native Attr nativeSetAttributeNode(Attr newAttr);


    //since DOM2
    public String getAttributeNS(String namespaceURI, String localName) {
	final String finalNamespaceURI = namespaceURI;
	final  String finalLocalName = localName;
	String result = (String)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetAttributeNS(finalNamespaceURI, finalLocalName);
		    }
		    public String toString() {
			return "Element.getAttributeNS";
		    }
		});
	return result; 

    }
    native String nativeGetAttributeNS(String namespaceURI, String localName);

    public void setAttributeNS(String namespaceURI, String qualifiedName, String value) {
	final String finalNamespaceURI = namespaceURI;
	final  String finalQualifiedName = qualifiedName;
	final String finalValue = value;
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			nativeSetAttributeNS(finalNamespaceURI, finalQualifiedName, finalValue);
			return null;
		    }
		    public String toString() {
			return "Element.setAttributeNS";
		    }
		});

    }
    native void nativeSetAttributeNS(String namespaceURI, String qualifiedName, String value);

    public void removeAttributeNS(String namespacURI, String localName) {
	final String finalNamespacURI = namespacURI;
	final  String finalLocalName = localName;
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			nativeRemoveAttributeNS(finalNamespacURI, finalLocalName);
			return null;
		    }
		    public String toString() {
			return "Element.removeAttributeNS";
		    }
		});

    }
    native void nativeRemoveAttributeNS(String namespacURI, String localName);   

    public Attr getAttributeNodeNS(String namespaceURI, String localName) {
	final String finalNamespaceURI = namespaceURI;
	final  String finalLocalName = localName;
	Attr result = (Attr)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetAttributeNodeNS(finalNamespaceURI, finalLocalName);
		    }
		    public String toString() {
			return "Element.getAttributeNodeNS";
		    }
		});
	return result; 

    }
    native Attr nativeGetAttributeNodeNS(String namespaceURI, String localName);

    public Attr setAttributeNodeNS(Attr newAttr) {
	final Attr finalNewAttr = newAttr;
	Attr result = (Attr)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeSetAttributeNodeNS(finalNewAttr);
		    }
		    public String toString() {
			return "Element.setAttributeNodeNS";
		    }
		});
	return result;

    }
    native Attr nativeSetAttributeNodeNS(Attr newAttr);

    public NodeList getElementsByTagNameNS(String namespaceURI, String localName) {
	final String finalNamespaceURI = namespaceURI;
	final  String finalLocalName = localName;
	NodeList result = (NodeList)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetElementsByTagNameNS(finalNamespaceURI, finalLocalName);
		    }
		    public String toString() {
			return "Element.getElementsByTagNameNS";
		    }
		});
	return result; 

    }
    native NodeList nativeGetElementsByTagNameNS(String namespaceURI, String localName);

    public boolean hasAttribute(String name) {
	final String finalName = name;
	Boolean result = (Boolean)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			boolean booleanResult = nativeHasAttribute(finalName);
			return booleanResult ? Boolean.TRUE : Boolean.FALSE;
		    }
		    public String toString() {
			return "Element.hasAttribute";
		    }
		});
	return result;

    }
    native boolean nativeHasAttribute(String name);

    public boolean hasAttributeNS(String namespaceURI, String localName) {
	final String finalNamespaceURI = namespaceURI;
	final  String finalLocalName = localName;
	Boolean result = (Boolean)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			boolean booleanResult = 
			    nativeHasAttributeNS(finalNamespaceURI, finalLocalName);
			return booleanResult ? Boolean.TRUE : Boolean.FALSE;
		    }
		    public String toString() {
			return "Element.hasAttributeNS";
		    }
		});
	return result; 

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
