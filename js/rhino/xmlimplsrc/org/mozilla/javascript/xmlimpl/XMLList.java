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

import org.mozilla.javascript.*;
import org.mozilla.javascript.xml.*;

class XMLList extends XMLObjectImpl implements Function {
	static final long serialVersionUID = -4543618751670781135L;
	
	// Fields
	private XmlNode.List _annos;
	
	private XMLObjectImpl targetObject = null;
	private XmlNode.QName targetProperty = null;
	
	XMLList() {
		_annos = new XmlNode.List();
	}
	
	/** @deprecated Will probably end up unnecessary as we move things around */
	XmlNode.List getNodeList() {
		return _annos;
	}
	
	//	TODO	Should be XMLObjectImpl, XMLName?
	void setTargets(XMLObjectImpl object, XmlNode.QName property) {
		targetObject = object;
		targetProperty = property;
	}
	
	/** @deprecated */
	private XML getXmlFromAnnotation(int index) {
		return getXML(_annos, index);
	}
	
	XML XML() {
		if (length() == 1) return getXmlFromAnnotation(0);
		return null;
	}
	
	private void internalRemoveFromList(int index) {
		_annos.remove(index);
	}
	
	void replace(int index, XML xml) {
		if (index < length()) {
			XmlNode.List newAnnoList = new XmlNode.List();
			newAnnoList.add(_annos, 0, index);
			newAnnoList.add(xml);
			newAnnoList.add(_annos, index+1, length());
			_annos = newAnnoList;
		}
	}
	
	private void insert(int index, XML xml) {
		if (index < length()) {
			XmlNode.List newAnnoList = new XmlNode.List();
			newAnnoList.add(_annos, 0, index);
			newAnnoList.add(xml);
			newAnnoList.add(_annos, index, length());
			_annos = newAnnoList;
		}
	}
	
	//
	//
	//  methods overriding ScriptableObject
	//
	//
	
	public String getClassName() {
		return "XMLList";
	}
	
	//
	//
	//  methods overriding IdScriptableObject
	//
	//
	
	public Object get(int index, Scriptable start) {
		//Log("get index: " + index);
		
		if (index >= 0 && index < length()) {
			return getXmlFromAnnotation(index);
		} else {
			return Scriptable.NOT_FOUND;
		}
	}
	
	boolean hasXMLProperty(XMLName xmlName) {
		boolean result = false;
		
		// Has now should return true if the property would have results > 0 or
		// if it's a method name
		String name = xmlName.localName();
		if ((getPropertyList(xmlName).length() > 0) ||
			(getMethod(name) != NOT_FOUND)) {
			result = true;
		}
		
		return result;
	}
	
	public boolean has(int index, Scriptable start) {
		return 0 <= index && index < length();
	}
	
	void putXMLProperty(XMLName xmlName, Object value) {
		//Log("put property: " + name);
		
		// Special-case checks for undefined and null
		if (value == null) {
			value = "null";
		} else if (value instanceof Undefined) {
			value = "undefined";
		}
		
		if (length() > 1) {
			throw ScriptRuntime.typeError("Assignment to lists with more than one item is not supported");
		} else if (length() == 0) {
			// Secret sauce for super-expandos.
			// We set an element here, and then add ourselves to our target.
			if (targetObject != null && targetProperty != null && targetProperty.getLocalName() != null) {
				// Add an empty element with our targetProperty name and then set it.
				XML xmlValue = newTextElementXML(null, targetProperty, null);
				addToList(xmlValue);
				
				if(xmlName.isAttributeName()) {
					setAttribute(xmlName, value);
				} else {
					XML xml = (XML)item(0);
					xml.putXMLProperty(xmlName, value);
					
					// Update the list with the new item at location 0.
					replace(0, (XML)item(0));
				}
				
				// Now add us to our parent
				XMLName name2 = XMLName.formProperty(targetProperty.getUri(), targetProperty.getLocalName());
				targetObject.putXMLProperty(name2, this);
			} else {
				throw ScriptRuntime.typeError("Assignment to empty XMLList without targets not supported");
			}
		} else if(xmlName.isAttributeName()) {
			setAttribute(xmlName, value);
		} else {
			XML xml = (XML)item(0);
			xml.putXMLProperty(xmlName, value);
			
			// Update the list with the new item at location 0.
			replace(0, (XML)item(0));
		}
	}
	
	Object getXMLProperty(XMLName name) {
		return getPropertyList(name);
	}
	
	private void replaceNode(XML xml, XML with) {
		xml.replaceWith(with);
	}
	
	public void put(int index, Scriptable start, Object value) {
		Object parent = Undefined.instance;
		// Convert text into XML if needed.
		XMLObject xmlValue;
		
		// Special-case checks for undefined and null
		if (value == null) {
			value = "null";
		} else if (value instanceof Undefined) {
			value = "undefined";
		}
		
		if (value instanceof XMLObject) {
			xmlValue = (XMLObject) value;
		} else {
			if (targetProperty == null) {
				xmlValue = newXMLFromJs(value.toString());
			} else {
				xmlValue = newTextElementXML(null, targetProperty, value.toString());
			}
		}
		
		// Find the parent
		if (index < length()) {
			parent = item(index).parent();
		} else {
			// Appending
			parent = parent();
		}
		
		if (parent instanceof XML) {
			// found parent, alter doc
			XML xmlParent = (XML) parent;
			
			if (index < length()) {
				// We're replacing the the node.
				XML xmlNode = getXmlFromAnnotation(index);
				
				if (xmlValue instanceof XML) {
					replaceNode(xmlNode, (XML) xmlValue);
					replace(index, xmlNode);
				} else if (xmlValue instanceof XMLList) {
					// Replace the first one, and add the rest on the list.
					XMLList list = (XMLList) xmlValue;
					
					if (list.length() > 0) {
						int lastIndexAdded = xmlNode.childIndex();
						replaceNode(xmlNode, (XML)list.item(0));
						replace(index, (XML)list.item(0));
						
						for (int i = 1; i < list.length(); i++) {
							xmlParent.insertChildAfter(xmlParent.getXmlChild(lastIndexAdded), list.item(i));
							lastIndexAdded++;
							insert(index + i, (XML)list.item(i));
						}
					}
				}
			} else {
				// Appending
				xmlParent.appendChild(xmlValue);
				addToList(xmlParent.getXmlChild(index));
			}
		} else {
			// Don't all have same parent, no underlying doc to alter
			if (index < length()) {
				XML xmlNode = getXML(_annos, index);
				
				if (xmlValue instanceof XML) {
					replaceNode(xmlNode, (XML) xmlValue);
					replace(index, xmlNode);
				} else if (xmlValue instanceof XMLList) {
					// Replace the first one, and add the rest on the list.
					XMLList list = (XMLList) xmlValue;
					
					if (list.length() > 0) {
						replaceNode(xmlNode, (XML)list.item(0));
						replace(index, (XML)list.item(0));
						
						for (int i = 1; i < list.length(); i++) {
							insert(index + i, (XML)list.item(i));
						}
					}
				}
			} else {
				addToList(xmlValue);
			}
		}
	}
	
	private XML getXML(XmlNode.List _annos, int index) {
		if (index >= 0 && index < length()) {
			return xmlFromNode(_annos.item(index));
		} else {
			return null;
		}
	}
	
	void deleteXMLProperty(XMLName name) {
		for (int i = 0; i < length(); i++) {
			XML xml = getXmlFromAnnotation(i);
			
			if (xml.isElement()) {
				xml.deleteXMLProperty(name);
			}
		}
	}
	
	public void delete(int index) {
		if (index >= 0 && index < length()) {
			XML xml = getXmlFromAnnotation(index);
			
			xml.remove();
			
			internalRemoveFromList(index);
		}
	}
		
	public Object[] getIds() {
		Object enumObjs[];
		
		if (isPrototype()) {
			enumObjs = new Object[0];
		} else {
			enumObjs = new Object[length()];
			
			for (int i = 0; i < enumObjs.length; i++) {
				enumObjs[i] = new Integer(i);
			}
		}
		
		return enumObjs;
	}
	
	public Object[] getIdsForDebug() {
		return getIds();
	}
	
	
	// XMLList will remove will delete all items in the list (a set delete) this differs from the XMLList delete operator.
	void remove() {
		int nLen = length();
		for (int i = nLen - 1; i >= 0; i--) {
			XML xml = getXmlFromAnnotation(i);
			if (xml != null) {
				xml.remove();
				internalRemoveFromList(i);
			}
		}
	}
	
	XML item(int index) {
		return _annos != null
			? getXmlFromAnnotation(index) : createEmptyXML();
	}
	
	private void setAttribute(XMLName xmlName, Object value) {
		for (int i = 0; i < length(); i++) {
			XML xml = getXmlFromAnnotation(i);
			xml.setAttribute(xmlName, value);
		}
	}
	
	void addToList(Object toAdd) {
		_annos.addToList(toAdd);
	}
	
	//
	//
	// Methods from section 12.4.4 in the spec
	//
	//
	
	XMLList child(int index) {
		XMLList result = newXMLList();
		
		for (int i = 0; i < length(); i++) {
			result.addToList(getXmlFromAnnotation(i).child(index));
		}
		
		return result;
	}
	
	XMLList child(XMLName xmlName) {
		XMLList result = newXMLList();
		
		for (int i = 0; i < length(); i++) {
			result.addToList(getXmlFromAnnotation(i).child(xmlName));
		}
		
		return result;
	}
	
	void addMatches(XMLList rv, XMLName name) {
		for (int i=0; i<length(); i++) {
			getXmlFromAnnotation(i).addMatches(rv, name);
		}
	}
	
	XMLList children() {
		java.util.Vector v = new java.util.Vector();
		
		for (int i = 0; i < length(); i++) {
			XML xml = getXmlFromAnnotation(i);
			
			if (xml != null) {
				Object o = xml.children();
				if (o instanceof XMLList) {
					XMLList childList = (XMLList)o;
					
					int cChildren = childList.length();
					for (int j = 0; j < cChildren; j++) {
						v.addElement(childList.item(j));
					}
				}
			}
		}
		
		XMLList allChildren = newXMLList();
		int sz = v.size();
		
		for (int i = 0; i < sz; i++) {
			allChildren.addToList(v.get(i));
		}
		
		return allChildren;
	}
	
	XMLList comments() {
		XMLList result = newXMLList();
		
		for (int i = 0; i < length(); i++) {
			XML xml = getXmlFromAnnotation(i);
			result.addToList(xml.comments());
		}
		
		return result;
	}
	
	XMLList elements(XMLName name) {
		XMLList rv = newXMLList();
		for (int i=0; i<length(); i++) {
			XML xml = getXmlFromAnnotation(i);
			rv.addToList(xml.elements(name));
		}
		return rv;
	}
	
	boolean contains(Object xml) {
		boolean result = false;
		
		for (int i = 0; i < length(); i++) {
			XML member = getXmlFromAnnotation(i);
			
			if (member.equivalentXml(xml)) {
				result = true;
				break;
			}
		}
		
		return result;
	}
	
	XMLObjectImpl copy() {
		XMLList result = newXMLList();
		
		for (int i = 0; i < length(); i++) {
			XML xml = getXmlFromAnnotation(i);
			result.addToList(xml.copy());
		}
		
		return result;
	}
	
	boolean hasOwnProperty(XMLName xmlName) {
		if (isPrototype()) {
			String property = xmlName.localName();
			return (findPrototypeId(property) != 0);
		} else {
			return (getPropertyList(xmlName).length() > 0);
		}
	}
	
	boolean hasComplexContent() {
		boolean complexContent;
		int length = length();
		
		if (length == 0) {
			complexContent = false;
		} else if (length == 1) {
			complexContent = getXmlFromAnnotation(0).hasComplexContent();
		} else {
			complexContent = false;
			
			for (int i = 0; i < length; i++) {
				XML nextElement = getXmlFromAnnotation(i);
				if (nextElement.isElement()) {
					complexContent = true;
					break;
				}
			}
		}
		
		return complexContent;
	}
	
	boolean hasSimpleContent() {
		if (length() == 0) {
			return true;
		} else if (length() == 1) {
			return getXmlFromAnnotation(0).hasSimpleContent();
		} else {
			for (int i=0; i<length(); i++) {
				XML nextElement = getXmlFromAnnotation(i);
				if (nextElement.isElement()) {
					return false;
				}
			}
			return true;
		}
	}
	
	int length() {
		int result = 0;
		
		if (_annos != null) {
			result = _annos.length();
		}
		
		return result;
	}
	
	void normalize() {
		for (int i = 0; i < length(); i++) {
			getXmlFromAnnotation(i).normalize();
		}
	}
	
	/**
	 * If list is empty, return undefined, if elements have different parents return undefined,
	 * If they all have the same parent, return that parent.
	 *
	 * @return
	 */
	Object parent() {
		if (length() == 0) return Undefined.instance;
		
		XML candidateParent = null;
		
		for (int i = 0; i < length(); i++) {
			Object currParent = getXmlFromAnnotation(i).parent();
			if (!(currParent instanceof XML)) return Undefined.instance;
			XML xml = (XML)currParent;
			if (i == 0) {
				// Set the first for the rest to compare to.
				candidateParent = xml;
			} else {
				if (candidateParent.is(xml)) {
					//	keep looking
				} else {
					return Undefined.instance;
				}
			}
		}
		return candidateParent;
	}
	
	XMLList processingInstructions(XMLName xmlName) {
		XMLList result = newXMLList();
		
		for (int i = 0; i < length(); i++) {
			XML xml = getXmlFromAnnotation(i);
			
			result.addToList(xml.processingInstructions(xmlName));
		}
		
		return result;
	}
	
	boolean propertyIsEnumerable(Object name) {
		long index;
		if (name instanceof Integer) {
			index = ((Integer)name).intValue();
		} else if (name instanceof Number) {
			double x = ((Number)name).doubleValue();
			index = (long)x;
			if ((double)index != x) {
				return false;
			}
			if (index == 0 && 1.0 / x < 0) {
				// Negative 0
				return false;
			}
		} else {
			String s = ScriptRuntime.toString(name);
			index = ScriptRuntime.testUint32String(s);
		}
		return (0 <= index && index < length());
	}
	
	XMLList text() {
		XMLList result = newXMLList();
		
		for (int i = 0; i < length(); i++) {
			result.addToList(getXmlFromAnnotation(i).text());
		}
		
		return result;
	}
	
	public String toString() {
		//	ECMA357 10.1.2
		if (hasSimpleContent()) {
			StringBuffer sb = new StringBuffer();
			
			for(int i = 0; i < length(); i++) {
				XML next = getXmlFromAnnotation(i);
				if (next.isComment() || next.isProcessingInstruction()) {
					//	do nothing
				} else {
					sb.append(next.toString());
				}
			}
			
			return sb.toString();
		} else {
			return toXMLString();
		}
	}
	
	String toXMLString() {
		//	See ECMA 10.2.1
		StringBuffer sb = new StringBuffer();
		
		for (int i=0; i<length(); i++) {
			if (getProcessor().isPrettyPrinting() && i != 0) {
				sb.append('\n');
			}
			sb.append(getXmlFromAnnotation(i).toXMLString());
		}
		return sb.toString();
	}
	
	Object valueOf() {
		return this;
	}
	
	//
	// Other public Functions from XMLObject
	//
	
	boolean equivalentXml(Object target) {
		boolean result = false;
		
		// Zero length list should equate to undefined
		if (target instanceof Undefined && length() == 0) {
			result = true;
		} else if (length() == 1) {
			result = getXmlFromAnnotation(0).equivalentXml(target);
		} else if (target instanceof XMLList) {
			XMLList otherList = (XMLList) target;
			
			if (otherList.length() == length()) {
				result = true;
				
				for (int i = 0; i < length(); i++) {
					if (!getXmlFromAnnotation(i).equivalentXml(otherList.getXmlFromAnnotation(i))) {
						result = false;
						break;
					}
				}
			}
		}
		
		return result;
	}
	
	private XMLList getPropertyList(XMLName name) {
		XMLList propertyList = newXMLList();
		XmlNode.QName qname = null;
		
		if (!name.isDescendants() && !name.isAttributeName()) {
			// Only set the targetProperty if this is a regular child get
			// and not a descendant or attribute get
			qname = name.toQname();
		}
		
		propertyList.setTargets(this, qname);
		
		for (int i = 0; i < length(); i++) {
			propertyList.addToList(
				getXmlFromAnnotation(i).getPropertyList(name));
		}
		
		return propertyList;
	}
	
	private Object applyOrCall(boolean isApply,
		Context cx, Scriptable scope,
		Scriptable thisObj, Object[] args) {
		String methodName = isApply ? "apply" : "call";
		if(!(thisObj instanceof XMLList) ||
			((XMLList)thisObj).targetProperty == null)
			throw ScriptRuntime.typeError1("msg.isnt.function",
				methodName);
		
		return ScriptRuntime.applyOrCall(isApply, cx, scope, thisObj, args);
	}
	
	protected Object jsConstructor(Context cx, boolean inNewExpr,
		Object[] args) {
		if (args.length == 0) {
			return newXMLList();
		} else {
			Object arg0 = args[0];
			if (!inNewExpr && arg0 instanceof XMLList) {
				// XMLList(XMLList) returns the same object.
				return arg0;
			}
			return newXMLListFrom(arg0);
		}
	}
	
	/**
	 * See ECMA 357, 11_2_2_1, Semantics, 3_e.
	 */
	public Scriptable getExtraMethodSource(Context cx) {
		if (length() == 1) {
			return getXmlFromAnnotation(0);
		}
		return null;
	}
	
	public Object call(Context cx, Scriptable scope, Scriptable thisObj,
		Object[] args) {
		// This XMLList is being called as a Function.
		// Let's find the real Function object.
		if(targetProperty == null)
			throw ScriptRuntime.notFunctionError(this);
		
		String methodName = targetProperty.getLocalName();
		
		boolean isApply = methodName.equals("apply");
		if(isApply || methodName.equals("call"))
			return applyOrCall(isApply, cx, scope, thisObj, args);
		
		Callable method = ScriptRuntime.getElemFunctionAndThis(
			this, methodName, cx);
		// Call lastStoredScriptable to clear stored thisObj
		// but ignore the result as the method should use the supplied
		// thisObj, not one from redirected call
		ScriptRuntime.lastStoredScriptable(cx);
		return method.call(cx, scope, thisObj, args);
	}
	
	public Scriptable construct(Context cx, Scriptable scope, Object[] args) {
		throw ScriptRuntime.typeError1("msg.not.ctor", "XMLList");
	}
}


