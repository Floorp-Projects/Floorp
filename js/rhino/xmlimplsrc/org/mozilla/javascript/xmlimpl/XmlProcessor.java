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

import org.w3c.dom.*;

import org.mozilla.javascript.*;

//	Disambiguate from org.mozilla.javascript.Node
import org.w3c.dom.Node;

class XmlProcessor {
	private boolean ignoreComments;
	private boolean ignoreProcessingInstructions;
	private boolean ignoreWhitespace;
	private boolean prettyPrint;
	private int prettyIndent;
	
	private javax.xml.parsers.DocumentBuilderFactory dom;
	private javax.xml.transform.TransformerFactory xform;
	
	XmlProcessor() {
		setDefault();
		this.dom = javax.xml.parsers.DocumentBuilderFactory.newInstance();
		this.xform = javax.xml.transform.TransformerFactory.newInstance();
	}
	
	final void setDefault() {
		this.setIgnoreComments(true);
		this.setIgnoreProcessingInstructions(true);
		this.setIgnoreWhitespace(true);
		this.setPrettyPrinting(true);
		this.setPrettyIndent(2);		
	}
	
	final void setIgnoreComments(boolean b) {
		this.ignoreComments = b;
	}
	
	final void setIgnoreWhitespace(boolean b) {
		this.ignoreWhitespace = b;
	}
	
	final void setIgnoreProcessingInstructions(boolean b) {
		this.ignoreProcessingInstructions = b;
	}
	
	final void setPrettyPrinting(boolean b) {
		this.prettyPrint = b;
	}
	
	final void setPrettyIndent(int i) {
		this.prettyIndent = i;
	}
	
	final boolean isIgnoreComments() {
		return ignoreComments;
	}
	
	final boolean isIgnoreProcessingInstructions() {
		return ignoreProcessingInstructions;
	}
	
	final boolean isIgnoreWhitespace() {
		return ignoreWhitespace;
	}
	
	final boolean isPrettyPrinting() {
		return prettyPrint;
	}
	
	final int getPrettyIndent() {
		return prettyIndent;
	}
	
	private String toXmlNewlines(String rv) {
		StringBuffer nl = new StringBuffer();
		for (int i=0; i<rv.length(); i++) {
			if (rv.charAt(i) == '\r') {
				if (rv.charAt(i+1) == '\n') {
					//	DOS, do nothing and skip the \r
				} else {
					//	Macintosh, substitute \n
					nl.append('\n');
				}
			} else {
				nl.append(rv.charAt(i));
			}
		}
		return nl.toString();
	}
	
	private javax.xml.parsers.DocumentBuilderFactory newDomFactory() {
		return dom;
	}
	
	private void addProcessingInstructionsTo(java.util.Vector v, Node node) {
		if (node instanceof ProcessingInstruction) {
			v.add(node);
		}
		if (node.getChildNodes() != null) {
			for (int i=0; i<node.getChildNodes().getLength(); i++) {
				addProcessingInstructionsTo(v, node.getChildNodes().item(i));
			}
		}
	}
	
	private void addCommentsTo(java.util.Vector v, Node node) {
		if (node instanceof Comment) {
			v.add(node);
		}
		if (node.getChildNodes() != null) {
			for (int i=0; i<node.getChildNodes().getLength(); i++) {
				addProcessingInstructionsTo(v, node.getChildNodes().item(i));
			}
		}
	}
	
	private void addTextNodesToRemoveAndTrim(java.util.Vector toRemove, Node node) {
		if (node instanceof Text) {
			Text text = (Text)node;
			text.setData(text.getData().trim());
			if (text.getData().length() == 0) {
				toRemove.add(node);
			}
		}
		if (node.getChildNodes() != null) {
			for (int i=0; i<node.getChildNodes().getLength(); i++) {
				addTextNodesToRemoveAndTrim(toRemove, node.getChildNodes().item(i));
			}
		}
	}
	
	private void setElementDefaultNamespaces(Document document, String defaultNamespaceUri) {
		NodeList elements = document.getElementsByTagName("*");
		for (int i=0; i<elements.getLength(); i++) {
			Element element = (Element)elements.item(i);
			if (element.getPrefix() == null) {
				if (element.lookupNamespaceURI(null) == null) {
					element.getOwnerDocument().renameNode(element, defaultNamespaceUri, element.getTagName());
				}
			}
		}
	}
	
	final Element parse(String defaultNamespaceUri, String xml) throws org.xml.sax.SAXException {
		//	See ECMA357 10.3.1
		javax.xml.parsers.DocumentBuilderFactory domFactory = newDomFactory();
		domFactory.setNamespaceAware(true);
		domFactory.setIgnoringComments(false);
		try {
			String syntheticXml = "<parent xmlns=\"" + defaultNamespaceUri + "\">" + xml + "</parent>";
			Document document = domFactory.newDocumentBuilder().parse( new org.xml.sax.InputSource(new java.io.StringReader(syntheticXml)) );
			if (ignoreProcessingInstructions) {
				java.util.Vector v = new java.util.Vector();
				addProcessingInstructionsTo(v, document);
				for (int i=0; i<v.size(); i++) {
					Node node = (Node)v.elementAt(i);
					node.getParentNode().removeChild(node);
				}
			}
			if (ignoreComments) {
				java.util.Vector v = new java.util.Vector();
				addCommentsTo(v, document);
				for (int i=0; i<v.size(); i++) {
					Node node = (Node)v.elementAt(i);
					node.getParentNode().removeChild(node);
				}
			}
			if (ignoreWhitespace) {
				//	Apparently JAXP setIgnoringElementContentWhitespace() has a different meaning, it appears from the Javadoc
				//	Refers to element-only content models, which means we would need to have a validating parser and DTD or schema
				//	so that it would know which whitespace to ignore.
				
				//	Instead we will try to delete it ourselves.
				java.util.Vector v = new java.util.Vector();
				addTextNodesToRemoveAndTrim(v, document);
				for (int i=0; i<v.size(); i++) {
					Node node = (Node)v.elementAt(i);
					node.getParentNode().removeChild(node);
				}
			}
			//	Maybe not necessary adding the <parent></parent> rigamarole above
			//	setElementDefaultNamespaces(document, defaultNamespaceUri);
			Element rv = (Element)document.getDocumentElement().getChildNodes().item(0);
			document.getDocumentElement().removeChild(rv);
			return rv;
		} catch (java.io.IOException e) {
			throw new RuntimeException("Unreachable.");
		} catch (javax.xml.parsers.ParserConfigurationException e) {
			throw new RuntimeException(e);
		}
	}
	
	Document newDocument() {
		try {
			//	TODO	Should this use XML settings?
			return newDomFactory().newDocumentBuilder().newDocument();
		} catch (javax.xml.parsers.ParserConfigurationException ex) {
			//	TODO	How to handle these runtime errors?
			throw new RuntimeException(ex);
		}
	}
	
	private Text newEmptyText() {
		Document dom = newDocument();
		return dom.createTextNode("");
	}
	
	//	TODO	Cannot remember what this is for, so whether it should use settings or not
	private String toString(Node node) {
		javax.xml.transform.dom.DOMSource source = new javax.xml.transform.dom.DOMSource(node);
		java.io.StringWriter writer = new java.io.StringWriter();
		javax.xml.transform.stream.StreamResult result = new javax.xml.transform.stream.StreamResult(writer);
		try {
			javax.xml.transform.Transformer transformer = xform.newTransformer();
			transformer.setOutputProperty(javax.xml.transform.OutputKeys.OMIT_XML_DECLARATION, "yes");
			transformer.setOutputProperty(javax.xml.transform.OutputKeys.INDENT, "no");			
			transformer.transform(source, result);
		} catch (javax.xml.transform.TransformerConfigurationException ex) {
			//	TODO	How to handle these runtime errors?
			throw new RuntimeException(ex);
		} catch (javax.xml.transform.TransformerException ex) {
			//	TODO	How to handle these runtime errors?
			throw new RuntimeException(ex);
		}
		return toXmlNewlines(writer.toString());
	}
	
	String escapeAttributeValue(Object value) {
		String text = ScriptRuntime.toString(value);
		
		if (text.length() == 0) return "\"\"";
		
		Document dom = newDocument();
		Element e = dom.createElement("a");
		e.setAttribute("b", text);
		String elementText = toString(e);
		int begin = elementText.indexOf('"');
		int end = elementText.lastIndexOf('"');
		return elementText.substring(begin+1,end);
	}

	String escapeTextValue(Object value) {
		if (value instanceof XMLObjectImpl) {
			return ((XMLObjectImpl)value).toXMLString();
		}
		
		String text = ScriptRuntime.toString(value);
		
		if (text.length() == 0) return text;
		
		Document dom = newDocument();
		Element e = dom.createElement("a");
		e.setTextContent(text);
		String elementText = toString(e);
		
		int begin = elementText.indexOf('>') + 1;
		int end = elementText.lastIndexOf('<');
		return (begin < end) ? elementText.substring(begin, end) : "";
	}

	private String escapeElementValue(String s) {
		//	TODO	Check this
		return escapeTextValue(s);
	}
	
	private String elementToXmlString(Element element) {
		//	TODO	My goodness ECMA is complicated (see 10.2.1).  We'll try this first.
		Element copy = (Element)element.cloneNode(true);
		if (prettyPrint) {
			beautifyElement(copy, 0);
		}
		return toString(copy);
	}
	
	final String ecmaToXmlString(Node node) {
		//	See ECMA 357 Section 10.2.1
		StringBuffer s = new StringBuffer();
		int indentLevel = 0;
		if (prettyPrint) {
			for (int i=0; i<indentLevel; i++) {
				s.append(' ');
			}
		}
		if (node instanceof Text) {
			String data = ((Text)node).getData();
			//	TODO Does Java trim() work same as XMLWhitespace?
			String v = (prettyPrint) ? data.trim() : data;
			s.append(escapeElementValue(v));
			return s.toString();
		}
		if (node instanceof Attr) {
			String value = ((Attr)node).getValue();
			s.append(escapeAttributeValue(value));
			return s.toString();
		}
		if (node instanceof Comment) {
			s.append("<!--" + ((Comment)node).getNodeValue() + "-->");
			return s.toString();
		}
		if (node instanceof ProcessingInstruction) {
			ProcessingInstruction pi = (ProcessingInstruction)node;
			s.append("<?" + pi.getTarget() + " " + pi.getData() + "?>");
			return s.toString();
		}
		s.append(elementToXmlString((Element)node));
		return s.toString();		
	}
	
	private void beautifyElement(Element e, int indent) {
		StringBuffer s = new StringBuffer();
		s.append('\n');
		for (int i=0; i<indent; i++) {
			s.append(' ');
		}
		String afterContent = s.toString();
		for (int i=0; i<prettyIndent; i++) {
			s.append(' ');
		}
		String beforeContent = s.toString();
		
		//	We "mark" all the nodes first; if we tried to do this loop otherwise, it would behave unexpectedly (the inserted nodes
		//	would contribute to the length and it might never terminate).
		java.util.Vector toIndent = new java.util.Vector();
		boolean indentChildren = false;
		for (int i=0; i<e.getChildNodes().getLength(); i++) {
			if (i == 1) indentChildren = true;
			if (e.getChildNodes().item(i) instanceof Text) {
				toIndent.add(e.getChildNodes().item(i));
			} else {
				indentChildren = true;
				toIndent.add(e.getChildNodes().item(i));
			}
		}
		if (indentChildren) {			
			for (int i=0; i<toIndent.size(); i++) {
				e.insertBefore( e.getOwnerDocument().createTextNode(beforeContent), (Node)toIndent.elementAt(i) );
			}
		}
		NodeList nodes = e.getChildNodes();
		java.util.Vector v = new java.util.Vector();
		for (int i=0; i<nodes.getLength(); i++) {
			if (nodes.item(i) instanceof Element) {
				v.add( nodes.item(i) );
			}
		}
		for (int i=0; i<v.size(); i++) {
			beautifyElement( (Element)v.elementAt(i), indent + prettyIndent );
		}
		if (indentChildren) {
			e.appendChild( e.getOwnerDocument().createTextNode(afterContent) );
		}
	}
}
