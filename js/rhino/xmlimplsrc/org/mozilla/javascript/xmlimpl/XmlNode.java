/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Rhino DOM-only E4X implementation.
 *
 * The Initial Developer of the Original Code is
 * David P. Caldwell.
 * Portions created by David P. Caldwell are Copyright (C) 
 * 2007 David P. Caldwell. All Rights Reserved.
 *
 *
 * Contributor(s):
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

import java.util.*;

import org.w3c.dom.*;

import org.mozilla.javascript.*;

//	Disambiguate with org.mozilla.javascript
import org.w3c.dom.Node;

class XmlNode {
	private static final String XML_NAMESPACES_NAMESPACE_URI = "http://www.w3.org/2000/xmlns/";
	
	private static final String USER_DATA_XMLNODE_KEY = XmlNode.class.getName();
	
	private static final boolean DOM_LEVEL_3 = true;
	
	private static XmlNode getUserData(Node node) {
		if (DOM_LEVEL_3) {
			return (XmlNode)node.getUserData(USER_DATA_XMLNODE_KEY);
		}
		return null;
	}
	
	private static void setUserData(Node node, XmlNode wrap) {
		if (DOM_LEVEL_3) {
			node.setUserData(USER_DATA_XMLNODE_KEY, wrap, wrap.events);
		}
	}
	
	private static XmlNode createImpl(Node node) {
		if (node instanceof Document) throw new IllegalArgumentException();
		XmlNode rv = null;
		if (getUserData(node) == null) {
			rv = new XmlNode();
			rv.dom = node;
			setUserData(node, rv);
		} else {
			rv = getUserData(node);
		}
		return rv;		
	}
	
	static XmlNode newElementWithText(XmlProcessor processor, XmlNode reference, XmlNode.QName qname, String value) {
		if (reference instanceof org.w3c.dom.Document) throw new IllegalArgumentException("Cannot use Document node as reference");
		Document document = null;
		if (reference != null) {
			document = reference.dom.getOwnerDocument();
		} else {
			document = processor.newDocument();
		}
		Node referenceDom = (reference != null) ? reference.dom : null;
		Element e = document.createElementNS(qname.getUri(), qname.qualify(referenceDom));
		if (value != null) {
			e.appendChild(document.createTextNode(value));
		}
		return XmlNode.createImpl(e);
	}
	
	static XmlNode createText(XmlProcessor processor, String value) {
		return createImpl( processor.newDocument().createTextNode(value) );
	}
	
	static XmlNode createElement(XmlProcessor processor, String namespaceUri, String xml) throws org.xml.sax.SAXException {
		return createImpl( processor.toXml(namespaceUri, xml) );
	}
	
	static XmlNode createEmpty(XmlProcessor processor) {
		return createText(processor, "");
	}
	
	private static XmlNode copy(XmlNode other) {
		return createImpl( other.dom.cloneNode(true) );
	}
	
	private static final long serialVersionUID = 1L;
	
	private UserDataHandler events = new UserDataHandler() {
		public void handle(short operation, String key, Object data, Node src, Node dest) {
		}
	};
	
	private Node dom;
	
	private XML xml;
	
	private XmlNode() {
	}
	
	String debug() {
		XmlProcessor raw = new XmlProcessor();
		raw.setIgnoreComments(false);
		raw.setIgnoreProcessingInstructions(false);
		raw.setIgnoreWhitespace(false);
		raw.setPrettyPrinting(false);
		return raw.ecmaToXmlString(this.dom);
	}
	
	public String toString() {
		return "XmlNode: type=" + dom.getNodeType() + " dom=" + dom.toString();
	}
	
	XML getXml() {
		return xml;
	}
	
	void setXml(XML xml) {
		this.xml = xml;
	}
	
	int getChildCount() {
		return this.dom.getChildNodes().getLength();
	}
	
	XmlNode parent() {
		Node domParent = dom.getParentNode();
		if (domParent instanceof Document) return null;
		if (domParent == null) return null;
		return createImpl(domParent);
	}
	
	int getChildIndex() {
		if (this.isAttributeType()) return -1;
		if (parent() == null) return -1;
		org.w3c.dom.NodeList siblings = this.dom.getParentNode().getChildNodes();
		for (int i=0; i<siblings.getLength(); i++) {
			if (siblings.item(i) == dom) {
				return i;
			}
		}
		//	Either the parent is -1 or one of the this node's parent's children is this node.
		throw new RuntimeException("Unreachable.");
	}
	
	void removeChild(int index) {
		this.dom.removeChild( this.dom.getChildNodes().item(index) );
	}
	
	String toXmlString(XmlProcessor processor) {
		return processor.ecmaToXmlString(this.dom);
	}
	
	String ecmaValue() {
		//	TODO	See ECMA 357 Section 9.1
		if (isTextType()) {
			return ((org.w3c.dom.Text)dom).getData();
		} else if (isAttributeType()) {
			return ((org.w3c.dom.Attr)dom).getValue();
		} else if (isProcessingInstructionType()) {
			return ((org.w3c.dom.ProcessingInstruction)dom).getData();
		} else if (isCommentType()) {
			return ((org.w3c.dom.Comment)dom).getNodeValue();
		} else if (isElementType()) {
			throw new RuntimeException("Unimplemented ecmaValue() for elements.");
		} else {
			throw new RuntimeException("Unimplemented for node " + dom);
		}
	}
	
	void deleteMe() {
		if (dom instanceof Attr) {
			Attr attr = (Attr)this.dom;
			attr.getOwnerElement().getAttributes().removeNamedItemNS(attr.getNamespaceURI(), attr.getLocalName());
		} else {
			if (this.dom.getParentNode() != null) {
				this.dom.getParentNode().removeChild(this.dom);
			} else {
				//	This case can be exercised at least when executing the regression
				//	tests under https://bugzilla.mozilla.org/show_bug.cgi?id=354145
			}
		}
	}
	
	void normalize() {
		this.dom.normalize();
	}
	
	void insertChildAt(int index, XmlNode node) {
		Node parent = this.dom;
		Node child = parent.getOwnerDocument().importNode( node.dom, true );
		if (parent.getChildNodes().getLength() < index) {
			//	TODO	Check ECMA for what happens here
			throw new IllegalArgumentException("index=" + index + " length=" + parent.getChildNodes().getLength());
		}
		if (parent.getChildNodes().getLength() == index) {
			parent.appendChild(child);
		} else {
			parent.insertBefore(child, parent.getChildNodes().item(index));
		}
	}
	
	void insertChildrenAt(int index, XmlNode[] nodes) {
		for (int i=0; i<nodes.length; i++) {
			insertChildAt(index+i, nodes[i]);
		}
	}
	
	XmlNode getChild(int index) {
		Node child = dom.getChildNodes().item(index);
		return createImpl(child);
	}
	
	//	Helper method for XML.hasSimpleContent()
	boolean hasChildElement() {
		org.w3c.dom.NodeList nodes = this.dom.getChildNodes();
		for (int i=0; i<nodes.getLength(); i++) {
			if (nodes.item(i).getNodeType() == org.w3c.dom.Node.ELEMENT_NODE) return true;
		}
		return false;
	}
	
	boolean isSameNode(XmlNode other) {
		//	TODO	May need to be changed if we allow XmlNode to refer to several Node objects
		return this.dom == other.dom;
	}
	
	private String toUri(String ns) {
		return (ns == null) ? "" : ns;
	}
	
	private void addNamespaces(Namespaces rv, Element element) {
		if (element == null) throw new RuntimeException("element must not be null");
		String myDefaultNamespace = toUri(element.lookupNamespaceURI(null));
		String parentDefaultNamespace = "";
		if (element.getParentNode() != null) {
			parentDefaultNamespace = toUri(element.getParentNode().lookupNamespaceURI(null));
		}
		if (!myDefaultNamespace.equals(parentDefaultNamespace) || !(element.getParentNode() instanceof Element) ) {
			rv.declare(Namespace.create("", myDefaultNamespace));
		}
		NamedNodeMap attributes = element.getAttributes();
		for (int i=0; i<attributes.getLength(); i++) {
			Attr attr = (Attr)attributes.item(i);
			if (attr.getPrefix() != null && attr.getPrefix().equals("xmlns")) {
				rv.declare(Namespace.create(attr.getLocalName(), attr.getValue()));
			}
		}
	}
	
	private Namespaces getAllNamespaces() {
		Namespaces rv = new Namespaces();
		
		Node target = this.dom;
		if (target instanceof Attr) {
			target = ((Attr)target).getOwnerElement();
		}
		while(target != null) {
			if (target instanceof Element) {
				addNamespaces(rv, (Element)target);
			}
			target = target.getParentNode();
		}
		//	Fallback in case no namespace was declared
		rv.declare(Namespace.create("", ""));
		return rv;
	}
	
	Namespace[] getInScopeNamespaces() {
		Namespaces rv = getAllNamespaces();
		return rv.getNamespaces();
	}
	
	Namespace[] getNamespaceDeclarations() {
		//	ECMA357 13.4.4.24
		if (this.dom instanceof Element) {
			Namespaces rv = new Namespaces();
			addNamespaces( rv, (Element)this.dom );
			return rv.getNamespaces();
		} else {
			return new Namespace[0];
		}
	}
	
	Namespace getNamespaceDeclaration(String prefix) {
		if (prefix.equals("") && dom instanceof Attr) {
			//	Default namespaces do not apply to attributes; see XML Namespaces section 5.2
			return Namespace.create("", "");
		}
		Namespaces rv = getAllNamespaces();
		return rv.getNamespace(prefix);
	}
	
	Namespace getNamespaceDeclaration() {
		if (dom.getPrefix() == null) return getNamespaceDeclaration("");
		return getNamespaceDeclaration(dom.getPrefix());
	}
	
	private static class Namespaces {
		private HashMap map = new HashMap();
		private HashMap uriToPrefix = new HashMap();
		
		Namespaces() {
		}
		
		void declare(Namespace n) {
			if (map.get(n.prefix) == null) {
				map.put(n.prefix, n.uri);
			}
			//	TODO	I think this is analogous to the other way, but have not really thought it through ... should local scope
			//			matter more than outer scope?
			if (uriToPrefix.get(n.uri) == null) {
				uriToPrefix.put(n.uri, n.prefix);
			}
		}
		
		Namespace getNamespaceByUri(String uri) {
			if (uriToPrefix.get(uri) == null) return null;
			return Namespace.create(uri, (String)uriToPrefix.get(uri));
		}
		
		Namespace getNamespace(String prefix) {
			if (map.get(prefix) == null) return null;
			return Namespace.create(prefix, (String)map.get(prefix));
		}
		
		Namespace[] getNamespaces() {
			Iterator i = map.keySet().iterator();
			ArrayList rv = new ArrayList();
			while(i.hasNext()) {
				String prefix = (String)i.next();
				String uri = (String)map.get(prefix);
				Namespace n = Namespace.create(prefix, uri);
				if (!n.isEmpty()) {
					rv.add( n );
				}
			}
			return (Namespace[])rv.toArray(new Namespace[0]);
		}
	}
	
	final XmlNode copy() {
		return copy( this );
	}
	
	//	Returns whether this node is capable of being a parent
	final boolean isParentType() {
		return isElementType();
	}
	
	final boolean isTextType() {
		return dom.getNodeType() == Node.TEXT_NODE || dom.getNodeType() == Node.CDATA_SECTION_NODE;
	}
	
	final boolean isAttributeType() {
		return dom.getNodeType() == Node.ATTRIBUTE_NODE;
	}
	
	final boolean isProcessingInstructionType() {
		return dom.getNodeType() == Node.PROCESSING_INSTRUCTION_NODE;
	}
	
	final boolean isCommentType() {
		return dom.getNodeType() == Node.COMMENT_NODE;
	}
	
	final boolean isElementType() {
		return dom.getNodeType() == Node.ELEMENT_NODE;
	}
	
	final void renameNode(QName qname) {
		this.dom = dom.getOwnerDocument().renameNode(dom, qname.getUri(), qname.qualify(dom));
	}
	
	void invalidateNamespacePrefix() {
		if (!(dom instanceof Element)) throw new IllegalStateException();
		String prefix = this.dom.getPrefix();
		QName after = QName.create(this.dom.getNamespaceURI(), this.dom.getLocalName(), null);
		renameNode(after);
		NamedNodeMap attrs = this.dom.getAttributes();
		for (int i=0; i<attrs.getLength(); i++) {
			if (attrs.item(i).getPrefix().equals(prefix)) {
				createImpl( attrs.item(i) ).renameNode( QName.create(attrs.item(i).getNamespaceURI(), attrs.item(i).getLocalName(), null) );
			}
		}
	}
	
	private void declareNamespace(Element e, String prefix, String uri) {
		if (prefix.length() > 0) {
			e.setAttributeNS(XML_NAMESPACES_NAMESPACE_URI, "xmlns:" + prefix, uri);
		} else {
			e.setAttribute("xmlns", uri);
		}
	}
	
	void declareNamespace(String prefix, String uri) {
		if (!(dom instanceof Element)) throw new IllegalStateException();
		if (dom.lookupNamespaceURI(uri) != null && dom.lookupNamespaceURI(uri).equals(prefix)) {
			//	do nothing
		} else {
			Element e = (Element)dom;
			declareNamespace(e, prefix, uri);
		}
	}
	
	private Namespace getDefaultNamespace() {
		String prefix = "";
		String uri = (dom.lookupNamespaceURI(null) == null) ? "" : dom.lookupNamespaceURI(null);
		return Namespace.create(prefix, uri);
	}
	
	private String getExistingPrefixFor(Namespace namespace) {
		if (getDefaultNamespace().getUri().equals(namespace.getUri())) {
			return "";
		}
		return dom.lookupPrefix(namespace.getUri());
	}
	
	private Namespace getNodeNamespace() {
		String uri = dom.getNamespaceURI();
		String prefix = dom.getPrefix();
		if (uri == null) uri = "";
		if (prefix == null) prefix = "";
		return Namespace.create(prefix, uri);
	}
	
	Namespace getNamespace() {
		return getNodeNamespace();
	}
	
	void removeNamespace(Namespace namespace) {
		Namespace current = getNodeNamespace();
		
		//	Do not remove in-use namespace
		if (namespace.is(current)) return;
		NamedNodeMap attrs = this.dom.getAttributes();
		for (int i=0; i<attrs.getLength(); i++) {
			XmlNode attr = XmlNode.createImpl(attrs.item(i));
			if (namespace.is(attr.getNodeNamespace())) return;
		}
		
		//	TODO	I must confess I am not sure I understand the spec fully.  See ECMA357 13.4.4.31
		String existingPrefix = getExistingPrefixFor(namespace);
		if (existingPrefix != null) {
			if (namespace.isUnspecifiedPrefix()) {
				//	we should remove any namespace with this URI from scope; we do this by declaring a namespace with the same
				//	prefix as the existing prefix and setting its URI to the default namespace				
				declareNamespace(existingPrefix, getDefaultNamespace().getUri());
			} else {
				if (existingPrefix.equals(namespace.getPrefix())) {
					declareNamespace(existingPrefix, getDefaultNamespace().getUri());
				}
			}
		} else {
			//	the argument namespace is not declared in this scope, so do nothing.
		}
	}
	
	private void setProcessingInstructionName(String localName) {
		org.w3c.dom.ProcessingInstruction pi = (ProcessingInstruction)this.dom;		
		//	We cannot set the node name; Document.renameNode() only supports elements and attributes.  So we replace it
		pi.getParentNode().replaceChild(
			pi,
			pi.getOwnerDocument().createProcessingInstruction(localName, pi.getData())
		);		
	}

	final void setLocalName(String localName) {
		if (dom instanceof ProcessingInstruction) {
			setProcessingInstructionName(localName);
		} else {
			String prefix = dom.getPrefix();
			if (prefix == null) prefix = "";
			this.dom = dom.getOwnerDocument().renameNode(dom, dom.getNamespaceURI(), QName.qualify(prefix, localName));
		}
	}
	
	final QName getQname() {
		String uri = (dom.getNamespaceURI()) == null ? "" : dom.getNamespaceURI();
		String prefix = (dom.getPrefix() == null) ? "" : dom.getPrefix();
		return QName.create( uri, dom.getLocalName(), prefix );
	}
	
	void addMatchingChildren(XMLList result, XmlNode.Filter filter) {
		Node node = this.dom;
		NodeList children = node.getChildNodes();
		for(int i=0; i<children.getLength(); i++) {
			Node childnode = children.item(i);
			XmlNode child = XmlNode.createImpl(childnode);
			if (filter.accept(childnode)) {
				result.addToList(child);
			}
		}
	}
	
	XmlNode[] getMatchingChildren(Filter filter) {
		ArrayList rv = new ArrayList();
		NodeList nodes = this.dom.getChildNodes();
		for (int i=0; i<nodes.getLength(); i++) {
			Node node = nodes.item(i);
			if (filter.accept(node)) {
				rv.add( createImpl(node) );
			}
		}
		return (XmlNode[])rv.toArray(new XmlNode[0]);
	}
	
	XmlNode[] getAttributes() {
		NamedNodeMap attrs = this.dom.getAttributes();
		//	TODO	Or could make callers handle null?
		if (attrs == null) throw new IllegalStateException("Must be element.");
		XmlNode[] rv = new XmlNode[attrs.getLength()];
		for (int i=0; i<attrs.getLength(); i++) {
			rv[i] = createImpl( attrs.item(i) );
		}
		return rv;
	}
	
	String getAttributeValue() {
		return ((Attr)dom).getValue();
	}
	
	void setAttribute(QName name, String value) {
		if (!(dom instanceof Element)) throw new IllegalStateException("Can only set attribute on elements.");
		name.setAttribute( (Element)dom, value );
	}
	
	void replaceWith(XmlNode other) {
		Node replacement = other.dom;
		if (replacement.getOwnerDocument() != this.dom.getOwnerDocument()) {
			replacement = this.dom.getOwnerDocument().importNode(replacement, true);
		}
		this.dom.getParentNode().replaceChild(replacement, this.dom);
	}
	
	String ecmaToXMLString(XmlProcessor processor) {
		if (this.isElementType()) {
			Element copy = (Element)this.dom.cloneNode(true);
			Namespace[] inScope = this.getInScopeNamespaces();
			for (int i=0; i<inScope.length; i++) {
				declareNamespace(copy, inScope[i].getPrefix(), inScope[i].getUri());
			}
			return processor.ecmaToXmlString(copy);
		} else {
			return processor.ecmaToXmlString(dom);
		}
	}
	
	static class Namespace {
		static Namespace create(String prefix, String uri) {
			if (prefix == null) throw new IllegalArgumentException("Empty string represents default namespace prefix");
			if (uri == null) throw new IllegalArgumentException("Namespace may not lack a URI");
			Namespace rv = new Namespace();
			rv.prefix = prefix;
			rv.uri = uri;
			return rv;
		}
		
		static Namespace create(String uri) {
			Namespace rv = new Namespace();
			rv.uri = uri;
			return rv;
		}
		
		static final Namespace GLOBAL = create("", "");
		
		private String prefix;
		private String uri;
		
		private Namespace() {
		}
		
		public String toString() {
			if (prefix == null) return "XmlNode.Namespace [" + uri + "]";
			return "XmlNode.Namespace [" + prefix + "{" + uri + "}]";
		}
		
		boolean isUnspecifiedPrefix() {
			return prefix == null;
		}
		
		boolean is(Namespace other) {
			return this.prefix != null && other.prefix != null && this.prefix.equals(other.prefix) && this.uri.equals(other.uri);
		}
		
		boolean isEmpty() {
			return prefix != null && prefix.equals("") && uri.equals("");
		}
		
		boolean isDefault() {
			return prefix != null && prefix.equals("");
		}
		
		boolean isGlobal() {
			return uri != null && uri.equals("");
		}
		
		//	Called by QName
		//	TODO	Move functionality from QName lookupPrefix to here
		private void setPrefix(String prefix) {
			if (prefix == null) throw new IllegalArgumentException();
			this.prefix = prefix;
		}
		
		String getPrefix() {
			return prefix;
		}
		
		String getUri() {
			return uri;
		}
	}
	
	//	TODO	Where is this class used?  No longer using it in QName implementation
	static class QName {
		static QName create(Namespace namespace, String localName) {
			//	A null namespace indicates a wild-card match for any namespace
			//	A null localName indicates "*" from the point of view of ECMA357
			if (localName != null && localName.equals("*")) throw new RuntimeException("* is not valid localName");
			QName rv = new QName();
			rv.namespace = namespace;
			rv.localName = localName;
			return rv;
		}
		
		/** @deprecated */
		static QName create(String uri, String localName, String prefix) {
			return create(Namespace.create(prefix, uri), localName);
		}
		
		static String qualify(String prefix, String localName) {
			if (prefix == null) throw new IllegalArgumentException("prefix must not be null");
			if (prefix.length() > 0) return prefix + ":" + localName;
			return localName;
		}
		
		private Namespace namespace;
		private String localName;
		
		private QName() {
		}
		
		public String toString() {
			return "XmlNode.QName [" + localName + "," + namespace + "]";
		}
		
		private boolean equals(String one, String two) {
			if (one == null && two == null) return true;
			if (one == null || two == null) return false;
			return one.equals(two);
		}
		
		private boolean namespacesEqual(Namespace one, Namespace two) {
			if (one == null && two == null) return true;
			if (one == null || two == null) return false;
			return equals(one.getUri(), two.getUri());
		}
		
		final boolean isEqualTo(QName other) {
			if (!namespacesEqual(this.namespace, other.namespace)) return false;
			if (!equals(this.localName, other.localName)) return false;
			return true;
		}

		void lookupPrefix(org.w3c.dom.Node node) {
			if (node == null) throw new IllegalArgumentException("node must not be null");
			String prefix = node.lookupPrefix(namespace.getUri());
			if (prefix == null) {
				//	check to see if we match the default namespace
				String defaultNamespace = node.lookupNamespaceURI(null);
				if (defaultNamespace == null) defaultNamespace = "";
				String nodeNamespace = namespace.getUri();
				if (nodeNamespace.equals(defaultNamespace)) {
					prefix = "";
				}
			}
			int i = 0;
			while(prefix == null) {
				String generatedPrefix = "e4x_" + i++;
				String generatedUri = node.lookupNamespaceURI(generatedPrefix);
				if (generatedUri == null) {
					prefix = generatedPrefix;
					org.w3c.dom.Node top = node;
					while(top.getParentNode() != null && top.getParentNode() instanceof org.w3c.dom.Element) {
						top = top.getParentNode();
					}
					((org.w3c.dom.Element)top).setAttributeNS("http://www.w3.org/2000/xmlns/", "xmlns:" + prefix, namespace.getUri());
				}
			}
			namespace.setPrefix(prefix);
		}
	
		String qualify(org.w3c.dom.Node node) {
			if (namespace.getPrefix() == null) {
				if (node != null) {
					lookupPrefix(node);
				} else {
					if (namespace.getUri().equals("")) {
						namespace.setPrefix("");
					} else {
						//	TODO	I am not sure this is right, but if we are creating a standalone node, I think we can set the
						//			default namespace on the node itself and not worry about setting a prefix for that namespace.
						namespace.setPrefix("");
					}
				}
			}
			return qualify(namespace.getPrefix(), localName);
		}

		void setAttribute(org.w3c.dom.Element element, String value) {
			if (namespace.getPrefix() == null) lookupPrefix(element);
			element.setAttributeNS(namespace.getUri(), qualify(namespace.getPrefix(), localName), value);		
		}
	
		/** @deprecated Use getNamespace() */
		String getUri() {
			return namespace.getUri();
		}
		
		/** @deprecated Use getNamespace() */
		String getPrefix() {
			return namespace.getPrefix();
		}
		
		Namespace getNamespace() {
			return namespace;
		}
		
		String getLocalName() {
			return localName;
		}
	}
			
	static class List {
		private java.util.Vector v;
		
		List() {
			v = new java.util.Vector();
		}
		
		private void _add(XmlNode n) {
			v.add(n);
		}
		
		XmlNode item(int index) {
			return (XmlNode)(v.get(index));
		}
		
		void remove(int index) {
			v.remove(index);
		}
		
		void add(List other) {
			for (int i=0; i<other.length(); i++) {
				_add(other.item(i));
			}
		}
		
		void add(List from, int startInclusive, int endExclusive) {
			for (int i=startInclusive; i<endExclusive; i++) {
				_add(from.item(i));
			}
		}
		
		void add(XmlNode node) {
			_add(node);
		}
		
		/** @deprecated */
		void add(XML xml) {
			_add(xml.getAnnotation());
		}
		
		/** @deprecated */
		void addToList(Object toAdd) {
			if (toAdd instanceof Undefined) {
				// Missing argument do nothing...
				return;
			}
			
			if (toAdd instanceof XMLList) {
				XMLList xmlSrc = (XMLList)toAdd;
				for (int i = 0; i < xmlSrc.length(); i++) {
					this._add(((XML)xmlSrc.item(i)).getAnnotation());
				}
			} else if (toAdd instanceof XML) {
				this._add(((XML)(toAdd)).getAnnotation());
			} else if (toAdd instanceof XmlNode) {
				this._add((XmlNode)toAdd);
			}
		}
		
		int length() {
			return v.size();
		}
	};
	
	static abstract class Filter {
		static final Filter COMMENT = new Filter() {
			boolean accept(Node node) {
				return node.getNodeType() == Node.COMMENT_NODE;
			}
		};
		static final Filter TEXT = new Filter() {
			boolean accept(Node node) {
				return node.getNodeType() == Node.TEXT_NODE;
			}
		};
		static Filter PROCESSING_INSTRUCTION(final XMLName name) {
			return new Filter() {
				boolean accept(Node node) {
					if (node.getNodeType() == Node.PROCESSING_INSTRUCTION_NODE) {
						ProcessingInstruction pi = (ProcessingInstruction)node;
						return name.matchesLocalName(pi.getTarget());
					}
					return false;
				}
			};
		}
		static Filter ELEMENT = new Filter() {
			boolean accept(Node node) {
				return node.getNodeType() == Node.ELEMENT_NODE;
			}
		};
		static Filter TRUE = new Filter() {
			boolean accept(Node node) {
				return true;
			}
		};
		abstract boolean accept(Node node);
	}
	
	//	Support experimental Java interface
	org.w3c.dom.Node toDomNode() {
		return this.dom;
	}
}
