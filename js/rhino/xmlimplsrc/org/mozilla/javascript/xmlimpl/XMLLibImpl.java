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

package org.mozilla.javascript.xmlimpl;

import java.io.Serializable;

import org.mozilla.javascript.*;
import org.mozilla.javascript.xml.*;

public final class XMLLibImpl extends XMLLib implements Serializable {
	//	TODO	Document that this only works with JDK 1.5 or backport its
	//	features to earlier versions
    private static final long serialVersionUID = 1L;
	
	//
	//	EXPERIMENTAL Java interface
	//
	
	/**
		This experimental interface is undocumented.
	 */
	public static org.w3c.dom.Node toDomNode(Object xmlObject) {
		//	Could return DocumentFragment for XMLList
		//	Probably a single node for XMLList with one element
		if (xmlObject instanceof XML) {
			return ((XML)xmlObject).toDomNode();
		} else {
			throw new IllegalArgumentException("xmlObject is not an XML object in JavaScript.");
		}
	}
	
	public static void init(Context cx, Scriptable scope, boolean sealed) {
		XMLLibImpl lib = new XMLLibImpl(scope);
		XMLLib bound = lib.bindToScope(scope);
		if (bound == lib) {
			lib.exportToScope(sealed);
		}
	}
	
    private Scriptable globalScope;

    private XML xmlPrototype;
    private XMLList xmlListPrototype;
    private Namespace namespacePrototype;
    private QName qnamePrototype;

	private XmlProcessor options = new XmlProcessor();

	private XMLLibImpl(Scriptable globalScope) {
		this.globalScope = globalScope;
	}

	/** @deprecated */
	QName qnamePrototype() {
		return qnamePrototype;
	}

	/** @deprecated */
	Scriptable globalScope() {
		return globalScope;
	}

	XmlProcessor getProcessor() {
		return options;
	}
	
	private void exportToScope(boolean sealed) {
		xmlPrototype = newXML(XmlNode.createText(options, ""));
		xmlListPrototype = newXMLList();
		namespacePrototype = Namespace.create(this.globalScope, null, XmlNode.Namespace.GLOBAL);
		qnamePrototype = QName.create(this, this.globalScope, null, XmlNode.QName.create(XmlNode.Namespace.create(""), ""));
		
		xmlPrototype.exportAsJSClass(sealed);
		xmlListPrototype.exportAsJSClass(sealed);
		namespacePrototype.exportAsJSClass(sealed);
		qnamePrototype.exportAsJSClass(sealed);
	}
	
	/** @deprecated */
	XMLName toAttributeName(Context cx, Object nameValue) {
		if (nameValue instanceof XMLName) {
			//	TODO	Will this always be an XMLName of type attribute name?
			return (XMLName)nameValue;
		} else if (nameValue instanceof QName) {
			return XMLName.create( ((QName)nameValue).getDelegate(), true, false );
		} else if (nameValue instanceof Boolean
			|| nameValue instanceof Number
			|| nameValue == Undefined.instance
			|| nameValue == null) {
			throw badXMLName(nameValue);
		} else {
			//	TODO	Not 100% sure that putting these in global namespace is the right thing to do
			String localName = null;
			if (nameValue instanceof String) {
				localName = (String)nameValue;
			} else {
				localName = ScriptRuntime.toString(nameValue);
			}
			if (localName != null && localName.equals("*")) localName = null;
			return XMLName.create(XmlNode.QName.create(XmlNode.Namespace.create(""), localName), true, false);
		}
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

	XMLName toXMLNameFromString(Context cx, String name) {
		return XMLName.create( getDefaultNamespaceURI(cx), name );
	}

	/** @deprecated */
	XMLName toXMLName(Context cx, Object nameValue) {
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
			|| nameValue == null) {
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

    Object addXMLObjects(Context cx, XMLObject obj1, XMLObject obj2)
    {
        XMLList listToAdd = newXMLList();

        if (obj1 instanceof XMLList) {
            XMLList list1 = (XMLList)obj1;
            if (list1.length() == 1) {
                listToAdd.addToList(list1.item(0));
            } else {
                // Might be xmlFragment + xmlFragment + xmlFragment + ...;
                // then the result will be an XMLList which we want to be an
                // rValue and allow it to be assigned to an lvalue.
                listToAdd = newXMLListFrom(obj1);
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

	private Ref xmlPrimaryReference(Context cx, XMLName xmlName, Scriptable scope) {
		XMLObjectImpl xmlObj;
		XMLObjectImpl firstXml = null;
		for (;;) {
			// XML object can only present on scope chain as a wrapper
			// of XMLWithScope
			if (scope instanceof XMLWithScope) {
				xmlObj = (XMLObjectImpl)scope.getPrototype();
				if (xmlObj.hasXMLProperty(xmlName)) {
					break;
				}
				if (firstXml == null) {
					firstXml = xmlObj;
				}
			}
			scope = scope.getParentScope();
			if (scope == null) {
				xmlObj = firstXml;
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
	
	Namespace castToNamespace(Context cx, Object namespaceObj) {
		return this.namespacePrototype.castToNamespace(namespaceObj);
	}
	
	private String getDefaultNamespaceURI(Context cx) {
		return getDefaultNamespace(cx).uri();
	}
	
	Namespace newNamespace(String uri) {
		return this.namespacePrototype.newNamespace(uri);
	}

	Namespace getDefaultNamespace(Context cx) {
		if (cx == null) {
			cx = Context.getCurrentContext();
			if (cx == null) {
				return namespacePrototype;
			}
		}
		
		Namespace result;
		Object ns = ScriptRuntime.searchDefaultNamespace(cx);
		if (ns == null) {
			return namespacePrototype;
		} else {
			if (ns instanceof Namespace) {
				return (Namespace)ns;
			} else {
				//	TODO	Clarify or remove the following comment
				// Should not happen but for now it could
				// due to bad searchDefaultNamespace implementation.
				return namespacePrototype;
			}
		}
	}

	Namespace[] createNamespaces(XmlNode.Namespace[] declarations) {
		Namespace[] rv = new Namespace[declarations.length];
		for (int i=0; i<declarations.length; i++) {
			rv[i] = this.namespacePrototype.newNamespace(declarations[i].getPrefix(), declarations[i].getUri());
		}
		return rv;
	}

	//	See ECMA357 13.3.2
	QName constructQName(Context cx, Object namespace, Object name) {
		return this.qnamePrototype.constructQName(this, cx, namespace, name);
	}

	QName newQName(String uri, String localName, String prefix) {
		return this.qnamePrototype.newQName(this, uri, localName, prefix);
	}

	QName constructQName(Context cx, Object nameValue) {
//		return constructQName(cx, Undefined.instance, nameValue);
		return this.qnamePrototype.constructQName(this, cx, nameValue);
	}
	
	QName castToQName(Context cx, Object qnameValue) {
		return this.qnamePrototype.castToQName(this, cx, qnameValue);
	}
	
	QName newQName(XmlNode.QName qname) {
		return QName.create(this, this.globalScope, this.qnamePrototype, qname);
	}
	
	XML newXML(XmlNode node) {
		XML rv = new XML();
		rv.initialize(this, this.globalScope, this.xmlPrototype);
		rv.initialize(node);
		return rv;
	}
	
	/**
		@deprecated I believe this can be replaced by ecmaToXml below.
	 */
	final XML newXMLFromJs(Object inputObject) {
		String frag;
		
		if (inputObject == null || inputObject == Undefined.instance) {
			frag = "";
		} else if (inputObject instanceof XMLObjectImpl) {
			// todo: faster way for XMLObjects?
			frag = ((XMLObjectImpl) inputObject).toXMLString();
		} else {
			frag = ScriptRuntime.toString(inputObject);
		}
		
		if (frag.trim().startsWith("<>")) {
			throw ScriptRuntime.typeError("Invalid use of XML object anonymous tags <></>.");
		}
		
		if (frag.indexOf("<") == -1) {
			//	Solo text node
			return newXML(XmlNode.createText(options, frag));
		}
		return parse(frag);
	}
	
	private XML parse(String frag) {
		try {
			return newXML(XmlNode.createElement(options, getDefaultNamespaceURI(Context.getCurrentContext()), frag));
		} catch (org.xml.sax.SAXException e) {
			throw ScriptRuntime.typeError("Cannot parse XML: " + e.getMessage());
		}		
	}
	
	final XML ecmaToXml(Object object) {
		//	See ECMA357 10.3
		if (object == null || object == Undefined.instance) throw ScriptRuntime.typeError("Cannot convert " + object + " to XML");
		if (object instanceof XML) return (XML)object;
		if (object instanceof XMLList) {
			XMLList list = (XMLList)object;
			if (list.XML() != null) {
				return list.XML();
			} else {
				throw ScriptRuntime.typeError("Cannot convert list of >1 element to XML");
			}
		}
		//	TODO	Technically we should fail on anything except a String, Number or Boolean
		//			See ECMA357 10.3
		//	Instead we just blindly cast to a String and let them convert anything.
		String s = ScriptRuntime.toString(object);
		//	TODO	Could this get any uglier?
		if (s.length() > 0 && s.charAt(0) == '<') {
			return parse(s);
		} else {
			return newXML(XmlNode.createText(options, s));
		}
	}
	
	final XML newTextElementXML(XmlNode reference, XmlNode.QName qname, String value) {
		return newXML(XmlNode.newElementWithText(options, reference, qname, value));
	}
	
	XMLList newXMLList() {
		XMLList rv = new XMLList();
		rv.initialize(this, this.globalScope, this.xmlListPrototype);
		return rv;
	}
	
	final XMLList newXMLListFrom(Object inputObject) {
		XMLList rv = newXMLList();
		
		if (inputObject == null || inputObject instanceof Undefined) {
			return rv;
		} else if (inputObject instanceof XML) {
			XML xml = (XML) inputObject;
			rv.getNodeList().add(xml);
			return rv;
		} else if (inputObject instanceof XMLList) {
			XMLList xmll = (XMLList) inputObject;			
			rv.getNodeList().add(xmll.getNodeList());
			return rv;
		} else {
			String frag = ScriptRuntime.toString(inputObject).trim();
			
			if (!frag.startsWith("<>")) {
				frag = "<>" + frag + "</>";
			}
			
			frag = "<fragment>" + frag.substring(2);
			if (!frag.endsWith("</>")) {
				throw ScriptRuntime.typeError("XML with anonymous tag missing end anonymous tag");
			}
			
			frag = frag.substring(0, frag.length() - 3) + "</fragment>";
			
			XML orgXML = newXMLFromJs(frag);
			
			// Now orphan the children and add them to our XMLList.
			XMLList children = (XMLList)orgXML.children();
			
			for (int i = 0; i < children.getNodeList().length(); i++) {
				// Copy here is so that they'll be orphaned (parent() will be undefined)
				rv.getNodeList().add(((XML) children.item(i).copy()));
			}
			return rv;
		}
	}
	
	XmlNode.QName toNodeQName(Context cx, Object namespaceValue, Object nameValue) {
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
		
		XmlNode.Namespace ns;
		if (namespaceValue == Undefined.instance) {
			if ("*".equals(localName)) {
				ns = null;
			} else {
				ns = getDefaultNamespace(cx).getDelegate();
			}
		} else if (namespaceValue == null) {
			ns = null;
		} else if (namespaceValue instanceof Namespace) {
			ns = ((Namespace)namespaceValue).getDelegate();
		} else {
			ns = this.namespacePrototype.constructNamespace(namespaceValue).getDelegate();
		}
		
		if (localName != null && localName.equals("*")) localName = null;
		return XmlNode.QName.create(ns, localName);
	}
	
	XmlNode.QName toNodeQName(Context cx, String name, boolean attribute) {
		String local = name;
		XmlNode.Namespace defaultNamespace = getDefaultNamespace(cx).getDelegate();
		if (name != null && name.equals("*")) {
			return XmlNode.QName.create(null, null);
		} else {
			if (attribute) {
				return XmlNode.QName.create(XmlNode.Namespace.GLOBAL, name);
			} else {
				return XmlNode.QName.create(defaultNamespace, name);
			}
		}
	}
	
	/** 
		@deprecated Too general; this should be split into overloaded methods.
		Is that possible?
	 */
	XmlNode.QName toNodeQName(Context cx, Object nameValue, boolean attribute) {
		if (nameValue instanceof XMLName) {
			return ((XMLName)nameValue).toQname();
		} else if (nameValue instanceof QName) {
			QName qname = (QName)nameValue;
			return qname.getDelegate();
		} else if (
			nameValue instanceof Boolean
			|| nameValue instanceof Number
			|| nameValue == Undefined.instance
			|| nameValue == null
		) {
			throw badXMLName(nameValue);
		} else {
			String local = null;
			if (nameValue instanceof String) {
				local = (String)nameValue; 
			} else {
				local = ScriptRuntime.toString(nameValue);
			}
			return toNodeQName(cx, local, attribute);
		}
	}

	//
	//	Override methods from XMLLib
	//
	
	public boolean isXMLName(Context _cx, Object nameObj) {
		return XMLName.accept(nameObj);
	}

	public Object toDefaultXmlNamespace(Context cx, Object uriValue) {
		return this.namespacePrototype.constructNamespace(uriValue);
	}
	
	public String escapeTextValue(Object o) {
		return options.escapeTextValue(o);
	}
	
	public String escapeAttributeValue(Object o) {
		return options.escapeAttributeValue(o);
	}
	
	public Ref nameRef(Context cx, Object name, Scriptable scope, int memberTypeFlags) {
		if ((memberTypeFlags & Node.ATTRIBUTE_FLAG) == 0) {
			// should only be called foir cases like @name or @[expr]
			throw Kit.codeBug();
		}
		XMLName xmlName = toAttributeName(cx, name);
		return xmlPrimaryReference(cx, xmlName, scope);
	}
	
	public Ref nameRef(Context cx, Object namespace, Object name, Scriptable scope, int memberTypeFlags) {
		XMLName xmlName = XMLName.create(toNodeQName(cx, namespace, name), false, false);
		
		//	No idea what is coming in from the parser in this case; is it detecting the "@"?
		if ((memberTypeFlags & Node.ATTRIBUTE_FLAG) != 0) {
			if (!xmlName.isAttributeName()) {
				xmlName.setAttributeName();
			}
		}
		
		return xmlPrimaryReference(cx, xmlName, scope);
	}
}
