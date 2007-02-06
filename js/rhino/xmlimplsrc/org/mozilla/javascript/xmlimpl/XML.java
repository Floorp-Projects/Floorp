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
 *   Ethan Hugg
 *   Terry Lucas
 *   Milen Nankov
 *   David P. Caldwell <inonit@inonit.com>
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

import java.io.Serializable;
import java.util.*;

import org.w3c.dom.*;

import org.mozilla.javascript.*;

class XML extends XMLObjectImpl {
	static final long serialVersionUID = -630969919086449092L;
	
    private XmlNode node;

	XML() {
	}

	void initialize(XmlNode node) {
		if (node == null) throw new RuntimeException("Unreachable code: assertion failed.");
		this.node = node;
		this.node.setXml(this);
	}
	
	final XML XML() {
		return this;
	}

	void replaceWith(XML value) {
		//	We use the underlying document structure if the node is not
		//	"standalone," but we need to just replace the XmlNode instance
		//	otherwise
		if (this.node.parent() != null || false) {
			this.node.replaceWith(value.node);
		} else {
			this.initialize(value.node);
		}
	}

	private void ecmaPutAdd(XMLList insert, XML value) {
		if (value.isAttribute()) {
			value = toXML( XmlNode.createText(getProcessor(), value.node.getAttributeValue() ) );
		}
		insert.addToList( value );
	}
	
	private void ecmaPut(XMLName name, XMLObjectImpl value) {
		if (isText() || isComment() || isProcessingInstruction() || isAttribute()) return;
		if (value instanceof XMLObjectImpl) {
			value = value.copy();
		}
		XMLList insert = newXMLList();
		if (value instanceof XML) {
			ecmaPutAdd(insert, (XML)value);
		} else {
			XMLList list = (XMLList)value;
			for (int i=0; i<list.length(); i++) {
				ecmaPutAdd(insert, list.item(i));
			}
		}
		this.setChildren( insert );
	}

	/** @deprecated I would love to encapsulate this somehow. */
	XML makeXmlFromString(XMLName name, String value) {
		try {
			return newTextElementXML(this.node, name.toQname(), value.toString());
		} catch(Exception e) {
			throw ScriptRuntime.typeError(e.getMessage());
		}
	}
	
	/** @deprecated Rename this, at the very least.  But it's not clear it's even necessary */
	XmlNode getAnnotation() {
		return node; 
	}
	
	/** @deprecated */
	private XmlNode.QName adapt(javax.xml.namespace.QName targetProperty) {
		if (targetProperty == null) return null;
		return XmlNode.QName.create(targetProperty.getNamespaceURI(), targetProperty.getLocalPart(), targetProperty.getPrefix());
	}
	
    //
    //  Methods from ScriptableObject
    //

	//	TODO Either cross-reference this next comment with the specification or delete it and change the behavior
	//	The comment: XML[0] should return this, all other indexes are Undefined
	public Object get(int index, Scriptable start) {
		if (index == 0) {
			return this;
		} else {
			return Scriptable.NOT_FOUND;
		}
	}
	
	public boolean has(int index, Scriptable start) {
		return (index == 0);
	}

	public void put(int index, Scriptable start, Object value) {
		//	TODO	Clarify the following comment and add a reference to the spec
		//	The comment: Spec says assignment to indexed XML object should return type error
		throw ScriptRuntime.typeError("Assignment to indexed XML is not allowed");
	}
	
	public Object[] getIds() {
		if (isPrototype()) {
			return new Object[0];
		} else {
			return new Object[] { new Integer(0) };
		}
	}
	
	//	TODO	This is how I found it but I am not sure it makes sense
	public void delete(int index) {
		if (index == 0) {
			this.remove();
		}
	}

	//
	//	Methods from XMLObjectImpl
	//

	boolean hasXMLProperty(XMLName xmlName) {
		if (isPrototype()) {
			return getMethod(xmlName.localName()) != NOT_FOUND;
		} else {
			return (getPropertyList(xmlName).length() > 0) || (getMethod(xmlName.localName()) != NOT_FOUND);
		}
	}

	Object getXMLProperty(XMLName xmlName) {
		if (isPrototype()) {
			return getMethod(xmlName.localName());
		} else {
			return getPropertyList(xmlName);
		}
	}

    //
    //
    //	Methods that merit further review
    //
    //
	
	XmlNode.QName getNodeQname() {
		return this.node.getQname();
	}
	
	XML[] getChildren() {
		if (!isElement()) return null;
		XmlNode[] children = this.node.getMatchingChildren(XmlNode.Filter.TRUE);
		XML[] rv = new XML[children.length];
		for (int i=0; i<rv.length; i++) {
			rv[i] = toXML(children[i]);
		}
		return rv;
	}

	XML[] getAttributes() {
		XmlNode[] attributes = this.node.getAttributes();
		XML[] rv = new XML[attributes.length];
		for (int i=0; i<rv.length; i++) {
			rv[i] = toXML(attributes[i]);
		}
		return rv;
	}

	//	Used only by XML, XMLList
	XMLList getPropertyList(XMLName name) {
		return name.getMyValueOn(this);
	}

	void deleteXMLProperty(XMLName name) {
		XMLList list = getPropertyList(name);
		for (int i=0; i<list.length(); i++) {
			list.item(i).node.deleteMe();
		}
	}

	void putXMLProperty(XMLName xmlName, Object value) {
		if (isPrototype()) {
			//	TODO	Is this really a no-op?  Check the spec to be sure
		} else {
			xmlName.setMyValueOn(this, value);
		}
	}

	boolean hasOwnProperty(XMLName xmlName) {
		boolean hasProperty = false;
		
		if (isPrototype()) {
			String property = xmlName.localName();
			hasProperty = (0 != findPrototypeId(property));
		} else {
			hasProperty = (getPropertyList(xmlName).length() > 0);
		}
		
		return hasProperty;
	}
	
	protected Object jsConstructor(Context cx, boolean inNewExpr, Object[] args) {
		if (args.length == 0 || args[0] == null || args[0] == Undefined.instance) {
			args = new Object[] { "" };
		}
		//	ECMA 13.4.2 does not appear to specify what to do if multiple arguments are sent.
		XML toXml = ecmaToXml(args[0]);
		if (inNewExpr) {
			return toXml.copy();
		} else {
			return toXml;
		}
	}

	//	See ECMA 357, 11_2_2_1, Semantics, 3_f.
	public Scriptable getExtraMethodSource(Context cx) {
		if (hasSimpleContent()) {
			String src = toString();
			return ScriptRuntime.toObjectOrNull(cx, src);
		}
		return null;
	}
	
	//
	//	TODO	Miscellaneous methods not yet grouped
	//
	
	void removeChild(int index) {
		this.node.removeChild((int)index);
	}

	void normalize() {
		this.node.normalize();
	}
	
	private XML toXML(XmlNode node) {
		if (node.getXml() == null) {
			node.setXml(newXML(node));
		}
		return node.getXml();
	}
	
	void setAttribute(XMLName xmlName, Object value) {
		if (!isElement()) throw new IllegalStateException("Can only set attributes on elements.");
		//	TODO	Is this legal, but just not "supported"?  If so, support it.
		if (xmlName.uri() == null && xmlName.localName().equals("*")) {
			throw ScriptRuntime.typeError("@* assignment not supported.");
		}
		this.node.setAttribute(xmlName.toQname(), ScriptRuntime.toString(value));
	}
	
	void remove() {
		this.node.deleteMe();
	}
	
	void addMatches(XMLList rv, XMLName name) {
		name.addMatches(rv, this);
	}

	XMLList elements(XMLName name) {
		XMLList rv = newXMLList();
		rv.setTargets(this, name.toQname());
		//	TODO	Should have an XMLNode.Filter implementation based on XMLName
		XmlNode[] elements = this.node.getMatchingChildren(XmlNode.Filter.ELEMENT);
		for (int i=0; i<elements.length; i++) {
			if (name.matches( toXML(elements[i]) )) {
				rv.addToList( toXML(elements[i]) );
			}
		}
		return rv;
	}
	
	XMLList child(XMLName xmlName) {
		//	TODO	Right now I think this method would allow child( "@xxx" ) to return the xxx attribute, which is wrong
		
		XMLList rv = newXMLList();
		
		//	TODO	Should this also match processing instructions?  If so, we have to change the filter and also the XMLName
		//			class to add an acceptsProcessingInstruction() method
		
		XmlNode[] elements = this.node.getMatchingChildren(XmlNode.Filter.ELEMENT);
		for (int i=0; i<elements.length; i++) {
			if (xmlName.matchesElement(elements[i].getQname())) {
				rv.addToList( toXML(elements[i]) );
			}
		}
		rv.setTargets(this, xmlName.toQname());
		return rv;
	}

	XML replace(XMLName xmlName, Object xml) {
		putXMLProperty(xmlName, xml);
		return this;
	}

	XMLList children() {
		XMLList rv = newXMLList();
		XMLName all = XMLName.formStar();
		rv.setTargets(this, all.toQname());
		XmlNode[] children = this.node.getMatchingChildren(XmlNode.Filter.TRUE);
		for (int i=0; i<children.length; i++) {
			rv.addToList( toXML(children[i]) );
		}
		return rv;
	}

	XMLList child(int index) {
		//	ECMA357 13.4.4.6 (numeric case)
		XMLList result = newXMLList();
		result.setTargets(this, null);
		if (index >= 0 && index < this.node.getChildCount()) {
			result.addToList(getXmlChild(index));
		}
		return result;
	}

	XML getXmlChild(int index) {
		XmlNode child = this.node.getChild((int)index);
		if (child.getXml() == null) {
			child.setXml(newXML(child));
		}
		return child.getXml();
	}

	int childIndex() {
		return this.node.getChildIndex();
	}

	boolean contains(Object xml) {
		if (xml instanceof XML) {
			return equivalentXml(xml);
		} else {
			return false;
		}
	}

	//	Method overriding XMLObjectImpl
	boolean equivalentXml(Object target) {
		boolean result = false;
		
		if (target instanceof XML) {
			//	TODO	This is a horrifyingly inefficient way to do this so we should make it better.  It may also not work.
			return this.node.toXmlString(getProcessor()).equals( ((XML)target).node.toXmlString(getProcessor()) );
		} else if (target instanceof XMLList) {
			//	TODO	Is this right?  Check the spec ...
			XMLList otherList = (XMLList) target;
			
			if (otherList.length() == 1) {
				result = equivalentXml(otherList.XML());
			}
		} else if (hasSimpleContent()) {
			String otherStr = ScriptRuntime.toString(target);
			
			result = toString().equals(otherStr);
		}
		
		return result;
	}

	XMLObjectImpl copy() {
		return newXML( this.node.copy() );
	}

	boolean hasSimpleContent() {
		if (isComment() || isProcessingInstruction()) return false;
		if (isText() || this.node.isAttributeType()) return true;
		return !this.node.hasChildElement();
	}

	boolean hasComplexContent() {
		return !hasSimpleContent();
	}
	
	//	TODO Cross-reference comment below with spec
	//	Comment is: Length of an XML object is always 1, it's a list of XML objects of size 1.
	int length() {
		return 1;
	}

	//	TODO	it is not clear what this method was for ...
	boolean is(XML other) {
		return this.node.isSameNode(other.node);
	}

	Object nodeKind() {
		return ecmaClass();
	}
	
	Object parent() {
		XmlNode parent = this.node.parent();
		if (parent == null) return null;
		return newXML(this.node.parent());
	}
	
    boolean propertyIsEnumerable(Object name)
    {
        boolean result;
        if (name instanceof Integer) {
            result = (((Integer)name).intValue() == 0);
        } else if (name instanceof Number) {
            double x = ((Number)name).doubleValue();
            // Check that number is positive 0
            result = (x == 0.0 && 1.0 / x > 0);
        } else {
            result = ScriptRuntime.toString(name).equals("0");
        }
        return result;
    }

	Object valueOf() {
		return this;
	}
	
	//
	//	Selection of children
	//
	
	XMLList comments() {
		XMLList rv = newXMLList();
		this.node.addMatchingChildren(rv, XmlNode.Filter.COMMENT);
		return rv;
	}

	XMLList text() {
		XMLList rv = newXMLList();
		this.node.addMatchingChildren(rv, XmlNode.Filter.TEXT);
		return rv;
	}

	XMLList processingInstructions(XMLName xmlName) {
		XMLList rv = newXMLList();
		this.node.addMatchingChildren(rv, XmlNode.Filter.PROCESSING_INSTRUCTION(xmlName));
		return rv;
	}

	//
	//	Methods relating to modification of child nodes
	//
	
	//	We create all the nodes we are inserting before doing the insert to
	//	avoid nasty cycles caused by mutability of these objects.  For example,
	//	what if the toString() method of value modifies the XML object we were
	//	going to insert into?  insertAfter might get confused about where to
	//	insert.  This actually came up with SpiderMonkey, leading to a (very)
	//	long discussion.  See bug #354145.
	private XmlNode[] getNodesForInsert(Object value) {
		if (value instanceof XML) {
			return new XmlNode[] { ((XML)value).node };
		} else if (value instanceof XMLList) {
			XMLList list = (XMLList)value;
			XmlNode[] rv = new XmlNode[list.length()];
			for (int i=0; i<list.length(); i++) {
				rv[i] = list.item(i).node;
			}
			return rv;
		} else {
			return new XmlNode[] {
				XmlNode.createText(getProcessor(), ScriptRuntime.toString(value))
			};
		}
	}

	XML replace(int index, Object xml) {
		XMLList xlChildToReplace = (XMLList)child(index);
		if (xlChildToReplace.length() > 0) {
			// One exists an that index
			XML childToReplace = (XML)xlChildToReplace.item(0);
			insertChildAfter(childToReplace, xml);
			removeChild(index);
		}
		return this;
	}

	XML prependChild(Object xml) {
		if (this.node.isParentType()) {
			this.node.insertChildrenAt(0, getNodesForInsert(xml));
		}
		return this;
	}

	XML appendChild(Object xml) {
		if (this.node.isParentType()) {
			XmlNode[] nodes = getNodesForInsert(xml);
			this.node.insertChildrenAt(this.node.getChildCount(), nodes);
		}
		return this;
	}
	
	private int getChildIndexOf(XML child) {
		for (int i=0; i<this.node.getChildCount(); i++) {
			if (this.node.getChild(i).isSameNode(child.node)) {
				return i;
			}
		}
		return -1;
	}

	XML insertChildBefore(XML child, Object xml) {
		if (child == null) {
			// Spec says inserting before nothing is the same as appending
			appendChild(xml);
		} else {
			XmlNode[] toInsert = getNodesForInsert(xml);
			int index = getChildIndexOf(child);
			if (index != -1) {
				this.node.insertChildrenAt(index, toInsert);
			}
		}
		
		return this;
	}

	XML insertChildAfter(XML child, Object xml) {
		if (child == null) {
			// Spec says inserting after nothing is the same as prepending
			prependChild(xml);
		} else {
			XmlNode[] toInsert = getNodesForInsert(xml);
			int index = getChildIndexOf(child);
			if (index != -1) {
				this.node.insertChildrenAt(index+1, toInsert);
			}
		}
		
		return this;
	}

	XML setChildren(Object xml) {
		//	TODO	Have not carefully considered the spec but it seems to call for this
		if (!isElement()) return this;
		
		while(this.node.getChildCount() > 0) {
			this.node.removeChild(0);
		}
		XmlNode[] toInsert = getNodesForInsert(xml);
		// append new children
		this.node.insertChildrenAt(0, toInsert);
		
		return this;
	}
	
	//
	//	Name and namespace-related methods
	//
	
	private void addInScopeNamespace(Namespace ns) {
		if (!isElement()) {
			return;
		}
		//	See ECMA357 9.1.1.13
		//	in this implementation null prefix means ECMA undefined
		if (ns.prefix() != null) {
			if (ns.prefix().length() == 0 && ns.uri().length() == 0) {
				return;
			}
			if (node.getQname().getNamespace().getPrefix().equals(ns.prefix())) {
				node.invalidateNamespacePrefix();
			}
			node.declareNamespace(ns.prefix(), ns.uri());
		} else {
			return;
		}
	}
	
	Namespace[] inScopeNamespaces() {
		XmlNode.Namespace[] inScope = this.node.getInScopeNamespaces();
		return createNamespaces(inScope);
	}
	
	private XmlNode.Namespace adapt(Namespace ns) {
		if (ns.prefix() == null) {
			return XmlNode.Namespace.create(ns.uri());
		} else {
			return XmlNode.Namespace.create(ns.prefix(), ns.uri());
		}
	}

	XML removeNamespace(Namespace ns) {
		if (!isElement()) return this;
		this.node.removeNamespace(adapt(ns));
		return this;
	}

    XML addNamespace(Namespace ns) {
		addInScopeNamespace(ns);
		return this;
    }

	QName name() {
		if (isText() || isComment()) return null;
		if (isProcessingInstruction()) return newQName("", this.node.getQname().getLocalName(), null);
		return newQName(node.getQname());
	}

	Namespace[] namespaceDeclarations() {
		XmlNode.Namespace[] declarations = node.getNamespaceDeclarations();
		return createNamespaces(declarations);
	}

	Namespace namespace(String prefix) {
		if (prefix == null) {
			return createNamespace( this.node.getNamespaceDeclaration() );
		} else {
			return createNamespace( this.node.getNamespaceDeclaration(prefix) );
		}
	}

	String localName() {
		if (name() == null) return null;
		return name().localName();
	}
	
	void setLocalName(String localName) {
		//	ECMA357 13.4.4.34
		if (isText() || isComment()) return;
		this.node.setLocalName(localName);
	}

	void setName(QName name) {
		//	See ECMA357 13.4.4.35
		if (isText() || isComment()) return;
		if (isProcessingInstruction()) {
			//	Spec says set the name URI to empty string and then set the [[Name]] property, but I understand this to do the same
			//	thing, unless we allow colons in processing instruction targets, which I think we do not.
			this.node.setLocalName(name.localName());
			return;
		}
		node.renameNode(name.getDelegate());
	}

	void setNamespace(Namespace ns) {
		//	See ECMA357 13.4.4.36
		if (isText() || isComment() || isProcessingInstruction()) return;
		setName(newQName(ns.uri(), localName(), ns.prefix()));
	}
	
	final String ecmaClass() {
		//	See ECMA357 9.1
		
		//	TODO	See ECMA357 9.1.1 last paragraph for what defaults should be
		
		if (node.isTextType()) {
			return "text";
		} else if (node.isAttributeType()) {
			return "attribute";
		} else if (node.isCommentType()) {
			return "comment";
		} else if (node.isProcessingInstructionType()) {
			return "processing-instruction";
		} else if (node.isElementType()) {
			return "element";
		} else {
			throw new RuntimeException("Unrecognized type: " + node);
		}
	}
	
	public String getClassName() {
		//	TODO:	This appears to confuse the interpreter if we use the "real" class property from ECMA.  Otherwise this code
		//	would be:
		//	return ecmaClass();
		return "XML";
	}
	
	private String ecmaValue() {
		return node.ecmaValue();
	}

	private String ecmaToString() {
		//	See ECMA357 10.1.1
		if (isAttribute() || isText()) {
			return ecmaValue();
		}
		if (this.hasSimpleContent()) {
			StringBuffer rv = new StringBuffer();
			for (int i=0; i<this.node.getChildCount(); i++) {
				XmlNode child = this.node.getChild(i);
				if (!child.isProcessingInstructionType() && !child.isCommentType()) {
					//	TODO	Probably inefficient; taking clean non-optimized solution for now
					XML x = new XML();
					x.initialize(child);
					rv.append(x.toString());
				}
			}
			return rv.toString();
		}
		return toXMLString();
	}
	
	public String toString() {
		return ecmaToString();
	}

	String toXMLString() {
		return this.node.ecmaToXMLString(getProcessor());
	}
	
	final boolean isAttribute() {
		return node.isAttributeType();
	}

	final boolean isComment() {
		return node.isCommentType();
	}
	
	final boolean isText() {
		return node.isTextType();
	}
	
	final boolean isElement() {
		return node.isElementType();
	}
	
	final boolean isProcessingInstruction() {
		return node.isProcessingInstructionType();
	}
	
	//	Support experimental Java interface
	org.w3c.dom.Node toDomNode() {
		return node.toDomNode();
	}
}
