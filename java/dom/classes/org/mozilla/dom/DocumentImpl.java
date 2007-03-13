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
import org.w3c.dom.CDATASection;
import org.w3c.dom.Comment;
import org.w3c.dom.DOMConfiguration;
import org.w3c.dom.Document;
import org.w3c.dom.DocumentFragment;
import org.w3c.dom.Element;
import org.w3c.dom.EntityReference;
import org.w3c.dom.ProcessingInstruction;
import org.w3c.dom.Text;
import org.w3c.dom.DocumentType;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.w3c.dom.DOMImplementation;
import org.w3c.dom.events.DocumentEvent;
import org.w3c.dom.events.Event;
import org.w3c.dom.DOMException;

import org.mozilla.util.ReturnRunnable;

public class DocumentImpl extends NodeImpl implements Document, DocumentEvent {

    // instantiated from JNI only
    private DocumentImpl() {}

    public Attr createAttribute(String name) {
	final String finalName = name;
	Attr result = (Attr)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeCreateAttribute(finalName);
		    }
		    public String toString() {
			return "Document.createAttribute";
		    }
		});
	return result;

    }
    native Attr nativeCreateAttribute(String name);

    public CDATASection createCDATASection(String data) {
	final String finalData = data;
	CDATASection result = (CDATASection)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeCreateCDATASection(finalData);
		    }
		    public String toString() {
			return "Document.createCDATASection";
		    }
		});
	return result;

    }
    native CDATASection nativeCreateCDATASection(String data);

    public Comment createComment(String data) {
	final String finalData = data;
	Comment result = (Comment)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeCreateComment(finalData);
		    }
		    public String toString() {
			return "Document.createComment";
		    }
		});
	return result;

    }
    native Comment nativeCreateComment(String data);

    public DocumentFragment createDocumentFragment() {
	DocumentFragment result = (DocumentFragment)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeCreateDocumentFragment();
		    }
		    public String toString() {
			return "Document.createDocumentFragment";
		    }
		});
	return result;
    }
    native DocumentFragment nativeCreateDocumentFragment();

    public Element createElement(String tagName) {
	final String finalTagName = tagName;
	Element result = (Element)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeCreateElement(finalTagName);
		    }
		    public String toString() {
			return "Document.createElement";
		    }
		});
	return result;

    }
    native Element nativeCreateElement(String tagName);

    public EntityReference createEntityReference(String name) {
	final String finalName = name;
	EntityReference result = (EntityReference)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeCreateEntityReference(finalName);
		    }
		    public String toString() {
			return "Document.createEntityReference";
		    }
		});
	return result;

    }
    native EntityReference nativeCreateEntityReference(String name);

    public ProcessingInstruction createProcessingInstruction(String target, String data) {
	final String finalTarget = target;
	final  String finalData = data;
	ProcessingInstruction result = (ProcessingInstruction)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeCreateProcessingInstruction(finalTarget, finalData);
		    }
		    public String toString() {
			return "Document.createProcessingInstruction";
		    }
		});
	return result;

    }
    native ProcessingInstruction nativeCreateProcessingInstruction(String target, String data);

    public Text createTextNode(String data) {
	final String finalData = data;
	Text result = (Text)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeCreateTextNode(finalData);
		    }
		    public String toString() {
			return "Document.createTextNode";
		    }
		});
	return result;

    }
    native Text nativeCreateTextNode(String data);

    public DocumentType getDoctype() {
	DocumentType result = (DocumentType)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetDoctype();
		    }
		    public String toString() {
			return "Document";
		    }
		});
	return result;
    }
    native DocumentType nativeGetDoctype();

    public Element getDocumentElement() {
	Element result = (Element)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetDocumentElement();
		    }
		    public String toString() {
			return "Document";
		    }
		});
	return result;
    }
    native Element nativeGetDocumentElement();

    public NodeList getElementsByTagName(String tagName) {
	final String finalTagName = tagName;
	NodeList result = (NodeList)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetElementsByTagName(finalTagName);
		    }
		    public String toString() {
			return "Document.getElementsByTagName";
		    }
		});
	return result;

    }
    native NodeList nativeGetElementsByTagName(String tagName);

    public DOMImplementation getImplementation() {
	DOMImplementation result = (DOMImplementation)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetImplementation();
		    }
		    public String toString() {
			return "Document";
		    }
		});
	return result;
    }
    native DOMImplementation nativeGetImplementation();

    public Event createEvent(String type) {
	final String finalType = type;
	Event result = (Event)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeCreateEvent(finalType);
		    }
		    public String toString() {
			return "Document.createEvent";
		    }
		});
	return result;

    }
    native Event nativeCreateEvent(String type);

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
			return "Document.getElementsByTagNameNS";
		    }
		});
	return result;

    }
    native NodeList nativeGetElementsByTagNameNS(String namespaceURI, String localName);

    public Element getElementById(String elementId) {
	final String finalElementId = elementId;
	Element result = (Element)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetElementById(finalElementId);
		    }
		    public String toString() {
			return "Document.getElementById";
		    }
		});
	return result;

    }
    native Element nativeGetElementById(String elementId);

    public String getDocumentURI() {
	String result = (String)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetDocumentURI();
		    }
		    public String toString() {
			return "Document.getDocumentURI";
		    }
		});
	return result;
    }
    native String nativeGetDocumentURI();

    public Node importNode(Node importedNode, boolean deep) throws DOMException {
        throw new UnsupportedOperationException();
    }

    public Element createElementNS(String namespaceURI, String qualifiedName)
                                            throws DOMException {
        throw new UnsupportedOperationException();
    } 

    public Attr createAttributeNS(String namespaceURI, String qualifiedName)
                                              throws DOMException {
        throw new UnsupportedOperationException();
    } 

    public DOMConfiguration getDomConfig() {
        throw new UnsupportedOperationException();
    }

    public String getInputEncoding() {
        throw new UnsupportedOperationException();
    }

    public boolean getStrictErrorChecking() {
        throw new UnsupportedOperationException();
    }

    public String getXmlEncoding() {
        throw new UnsupportedOperationException();
    }

    public boolean getXmlStandalone() {
        throw new UnsupportedOperationException();
    }

    public String getXmlVersion() {
        throw new UnsupportedOperationException();
    }

    public void normalizeDocument() {
        throw new UnsupportedOperationException();
    }

    public Node renameNode(Node n, String namespaceURI, String qualifiedName) throws DOMException {
        throw new UnsupportedOperationException();
    }

    public void setXmlVersion(String xmlVersion) throws DOMException {
        throw new UnsupportedOperationException();
    }

    public void setDocumentURI(String documentURI) {
        throw new UnsupportedOperationException();
    }

    public Node adoptNode(Node source) throws DOMException {
        throw new UnsupportedOperationException();
    }

    public void setStrictErrorChecking(boolean strictErrorChecking) {
        throw new UnsupportedOperationException();
    }

    public void setXmlStandalone(boolean xmlStandalone) throws DOMException {
        throw new UnsupportedOperationException();
    }
}
