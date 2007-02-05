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
 *   Milen Nankov
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

package org.mozilla.javascript.xmlimpl;

import org.mozilla.javascript.*;

class XMLName extends Ref {
	static final long serialVersionUID = 3832176310755686977L;
	
	private static boolean isNCNameStartChar(int c) {
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
	
	private static boolean isNCNameChar(int c) {
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
	
	//	This means "accept" in the parsing sense
	//	See ECMA357 13.1.2.1
	static boolean accept(Object nameObj) {
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
	
	private XmlNode.QName qname;
	private boolean isAttributeName;
	private boolean isDescendants;
	private XMLObjectImpl xmlObject;
	
	private XMLName() {
	}
	
	static XMLName formStar() {
		XMLName rv = new XMLName();
		rv.qname = XmlNode.QName.create(null, null);
		return rv;
	}
	
	/** @deprecated */
	static XMLName formProperty(XmlNode.Namespace namespace, String localName) {
		if (localName != null && localName.equals("*")) localName = null;
		XMLName rv = new XMLName();
		rv.qname = XmlNode.QName.create(namespace, localName);
		return rv;
	}
	
	/** @deprecated */
	static XMLName formProperty(String uri, String localName) {
		return formProperty(XmlNode.Namespace.create(uri), localName);
	}
	
	/** @deprecated */
	static XMLName create(String defaultNamespaceUri, String name) {
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
		
		return XMLName.formProperty(defaultNamespaceUri, name);
	}
	
	static XMLName create(XmlNode.QName qname, boolean attribute, boolean descendants) {
		XMLName rv = new XMLName();
		rv.qname = qname;
		rv.isAttributeName = attribute;
		rv.isDescendants = descendants;
		return rv;
	}
	
	/** @deprecated */
	static XMLName create(XmlNode.QName qname) {
		return create(qname, false, false);
	}
	
	void initXMLObject(XMLObjectImpl xmlObject) {
		if (xmlObject == null) throw new IllegalArgumentException();
		if (this.xmlObject != null) throw new IllegalStateException();
		this.xmlObject = xmlObject;
	}
	
	String uri() {
		if (qname.getNamespace() == null) return null;
		return qname.getNamespace().getUri();
	}
	
	String localName() {
		if (qname.getLocalName() == null) return "*";
		return qname.getLocalName();
	}
	
	private void addDescendantChildren(XMLList list, XML target) {
		XMLName xmlName = this;
		if (target.isElement()) {
			XML[] children = target.getChildren();
			for (int i=0; i<children.length; i++) {
				if (xmlName.matches( children[i] )) {
					list.addToList( children[i] );
				}
				addDescendantChildren(list, children[i]);
			}
		}
	}
	
	void addMatchingAttributes(XMLList list, XML target) {
		XMLName name = this;
		if (target.isElement()) {
			XML[] attributes = target.getAttributes();
			for (int i=0; i<attributes.length; i++) {
				if (name.matches( attributes[i]) ) {
					list.addToList( attributes[i] );
				}
			}
		}
	}
	
	private void addDescendantAttributes(XMLList list, XML target) {
		XMLName name = this;
		if (target.isElement()) {
			addMatchingAttributes(list, target);
			XML[] children = target.getChildren();
			for (int i=0; i<children.length; i++) {
				addDescendantAttributes(list, children[i]);
			}
		}
	}
	
	XMLList matchDescendantAttributes(XMLList rv, XML target) {
		rv.setTargets(target, null);
		addDescendantAttributes(rv, target);
		return rv;
	}

	XMLList matchDescendantChildren(XMLList rv, XML target) {
		rv.setTargets(target, null);
		addDescendantChildren(rv, target);
		return rv;
	}

	void addDescendants(XMLList rv, XML target) {
		XMLName xmlName = this;
		if (xmlName.isAttributeName()) {
			matchDescendantAttributes(rv, target);
		} else {
			matchDescendantChildren(rv, target);
		}
	}
	
	private void addAttributes(XMLList rv, XML target) {
		addMatchingAttributes(rv, target);
	}
	
	void addMatches(XMLList rv, XML target) {
		if (isDescendants()) {
			addDescendants(rv, target);
		} else if (isAttributeName()) {
			addAttributes(rv, target);
		} else {
			XML[] children = target.getChildren();
			if (children != null) {
				for (int i=0; i<children.length; i++) {
					if (this.matches(children[i])) {
						rv.addToList( children[i] );
					}
				}
			}
			rv.setTargets(target, this.toQname());			
		}
	}
	
	XMLList getMyValueOn(XML target) {
		XMLList rv = target.newXMLList();
		addMatches(rv, target);
		return rv;
	}
	
	void setMyValueOn(XML target, Object value) {
		// Special-case checks for undefined and null
		if (value == null) {
			value = "null";
		} else if (value instanceof Undefined) {
			value = "undefined";
		}
		
		XMLName xmlName = this;
		// Get the named property
		if (xmlName.isAttributeName()) {
			target.setAttribute(xmlName, value);
		} else if (xmlName.uri() == null && xmlName.localName().equals("*")) {
			target.setChildren(value);
		} else {
			// Convert text into XML if needed.
			XMLObjectImpl xmlValue = null;
			
			if (value instanceof XMLObjectImpl) {
				xmlValue = (XMLObjectImpl)value;
				
				// Check for attribute type and convert to textNode
				if (xmlValue instanceof XML) {
					if (((XML)xmlValue).isAttribute()) {
						xmlValue = target.makeXmlFromString(xmlName, xmlValue.toString());
					}
				}
				
				if (xmlValue instanceof XMLList) {
					for (int i = 0; i < xmlValue.length(); i++) {
						XML xml = (XML)((XMLList) xmlValue).item(i);
						
						if (xml.isAttribute()) {
							((XMLList)xmlValue).replace(i, target.makeXmlFromString(xmlName, xml.toString()));
						}
					}
				}
			} else {
				xmlValue = target.makeXmlFromString(xmlName, ScriptRuntime.toString(value));
			}
			
			XMLList matches = (XMLList)target.getPropertyList(xmlName);
			
			if (matches.length() == 0) {
				target.appendChild(xmlValue);
			} else {
				// Remove all other matches
				for (int i = 1; i < matches.length(); i++) {
					target.removeChild(matches.item(i).childIndex());
				}
				
				// Replace first match with new value.
				XML firstMatch = matches.item(0);
				target.replace(firstMatch.childIndex(), xmlValue);
			}
		}
	}
	
	public boolean has(Context cx) {
		if (xmlObject == null) {
			return false;
		}
		return xmlObject.hasXMLProperty(this);
	}
	
	public Object get(Context cx) {
		if (xmlObject == null) {
			throw ScriptRuntime.undefReadError(Undefined.instance,
				toString());
		}
		return xmlObject.getXMLProperty(this);
	}
	
	public Object set(Context cx, Object value) {
		if (xmlObject == null) {
			throw ScriptRuntime.undefWriteError(Undefined.instance,
				toString(),
				value);
		}
		// Assignment to descendants causes parse error on bad reference
		// and this should not be called
		if (isDescendants) throw Kit.codeBug();
		xmlObject.putXMLProperty(this, value);
		return value;
	}
	
	public boolean delete(Context cx) {
		if (xmlObject == null) {
			return true;
		}
		xmlObject.deleteXMLProperty(this);
		return !xmlObject.hasXMLProperty(this);
	}
	
	public String toString() {
		//return qname.localName();
		StringBuffer buff = new StringBuffer();
		if (isDescendants) buff.append("..");
		if (isAttributeName) buff.append('@');
		if (uri() == null) {
			buff.append('*');
			if(localName().equals("*")) {
				return buff.toString();
			}
		} else {
			buff.append('"').append(uri()).append('"');
		}
		buff.append(':').append(localName());
		return buff.toString();
	}
	
	private boolean equals(String one, String two) {
		if (one == null && two == null) return true;
		if (one == null || two == null) return false;
		return one.equals(two);
	}
	
	final XmlNode.QName toQname() {
		return this.qname;
	}
	
	final boolean matchesLocalName(String localName) {
		return localName().equals("*") || localName().equals(localName);
	}
	
	final boolean matchesElement(XmlNode.QName qname) {
		if (this.uri() == null || this.uri().equals(qname.getNamespace().getUri())) {
			if (this.localName().equals("*") || this.localName().equals(qname.getLocalName())) {
				return true;
			}
		}
		return false;
	}
	
	final boolean matches(XML node) {
		XmlNode.QName qname = node.getNodeQname();
		String nodeUri = null;
		if (qname.getNamespace() != null) {
			nodeUri = qname.getNamespace().getUri();
		}
		if (isAttributeName) {
			if (node.isAttribute()) {
				if (this.uri() == null || this.uri().equals(nodeUri)) {
					if (this.localName().equals("*") || this.localName().equals(qname.getLocalName())) {
						return true;
					}
				}
				return false;
			} else {
				//	TODO	Could throw exception maybe, should not call this method on attribute name with arbitrary node type
				//			unless we traverse all attributes and children habitually
				return false;
			}
		} else {
			if ( this.uri() == null || ((node.isElement()) && this.uri().equals(nodeUri)) ) {
				if (localName().equals("*")) return true;
				if (node.isElement()) {
					if (localName().equals(qname.getLocalName())) return true;
				}
			}
			return false;
		}
	}
	
	/**	@deprecated */
	boolean isAttributeName() {
		return isAttributeName;
	}
	
	//	TODO	Fix whether this is an attribute XMLName at construction?
	/** @deprecated */
	void setAttributeName() {
//		if (isAttributeName) throw new IllegalStateException();
		isAttributeName = true;
	}
	
	/**	@deprecated */
	boolean isDescendants() {
		return isDescendants;
	}
	
	//	TODO	Fix whether this is an descendant XMLName at construction?
	/** @deprecated */
	void setIsDescendants() {
//		if (isDescendants) throw new IllegalStateException();
		isDescendants = true;
	}	
}
