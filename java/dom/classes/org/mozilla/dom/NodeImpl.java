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

import org.w3c.dom.Node;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.NodeList;
import org.w3c.dom.Document;
import org.w3c.dom.DOMException;
import org.w3c.dom.UserDataHandler;
import org.w3c.dom.events.Event;
import org.w3c.dom.events.EventTarget;
import org.w3c.dom.events.EventListener;
import java.util.Vector;
import java.util.Hashtable;

class NodeEventListener {
	EventListener listener = null;
	String type = null;
	boolean useCapture = false;
	long nativeListener = 0;

	NodeEventListener(String aType, EventListener aListener, 
                          boolean aUseCapture, long aNativeListener) {
		type = aType;
		listener = aListener;
		useCapture = aUseCapture;
		nativeListener = aNativeListener;
	}

	public boolean equals(Object o) {
		if (!(o instanceof NodeEventListener))
			return false;
		else { 
			NodeEventListener n = (NodeEventListener) o;
			if ((useCapture != n.useCapture)  
                            || (type == null) || !type.equals(n.type) 
			    || (listener == null) || !listener.equals(n.listener)) 
				return false;
			else 
				return true;
		}
	}
}

public class NodeImpl implements Node, EventTarget {

    /* The derived classes (Attr, CharacterData, DocumentFragment,
       Document, Element, EntityReference, NamedNodeMap,
       ProcessingInstruction) will also use this native pointer, since
       the nsIDOM coreDom classes follow the same hierarchy */

    private long p_nsIDOMNode = 0;

    // this map stores association of java DOM listeners 
    // with corresponding native Nodes
    static private Hashtable javaDOMlisteners = new Hashtable();

    // instantiated from JNI only
    protected NodeImpl() { }
    protected NodeImpl(long p) {
	p_nsIDOMNode = p;
    }

    public boolean equals(Object o) {
	if (!(o instanceof NodeImpl))
	    return false;
	else
	    return (XPCOM_equals(o));
	}

    public int hashCode(){
	return XPCOM_hashCode();
    }

    public String toString() {
	return "<" + getNodeName() + 
	    " t=" + nodeTypeString(getNodeType()) + 
	    " c=org.mozilla.dom.NodeImpl p=" + 
	    Long.toHexString(p_nsIDOMNode) + ">";
    }

    private static String nodeTypeString(int type) {
	switch (type) {
	case Node.ELEMENT_NODE: return "ELEMENT";
	case Node.ATTRIBUTE_NODE: return "ATTRIBUTE";
	case Node.TEXT_NODE: return "TEXT";
	case Node.CDATA_SECTION_NODE: return "CDATA_SECTION";
	case Node.ENTITY_REFERENCE_NODE: return "ENTITY_REFERENCE";
	case Node.ENTITY_NODE: return "ENTITY";
	case Node.PROCESSING_INSTRUCTION_NODE: return "PROCESSING_INSTRUCTION";
	case Node.COMMENT_NODE: return "COMMENT";
	case Node.DOCUMENT_NODE: return "DOCUMENT";
	case Node.DOCUMENT_TYPE_NODE: return "DOCUMENT_TYPE";
	case Node.DOCUMENT_FRAGMENT_NODE: return "DOCUMENT_FRAGMENT";
	case Node.NOTATION_NODE: return "NOTATION";
	}
	return "ERROR";
    }

    public boolean isSupported(String feature, String version) {
	return nativeIsSupported(feature, version);
    }
    native boolean nativeIsSupported(String feature, String version);
    public boolean hasAttributes() {
	return nativeHasAttributes();
    }
    native boolean nativeHasAttributes();

    public Node appendChild(Node newChild) throws DOMException {
	return nativeAppendChild(newChild);
    }
    native Node nativeAppendChild(Node newChild) throws DOMException;

    public Node cloneNode(boolean deep) {
	return nativeCloneNode(deep);
    }
    native Node nativeCloneNode(boolean deep);

    public NamedNodeMap getAttributes() {
	return nativeGetAttributes();
    }
    native NamedNodeMap nativeGetAttributes();

    public NodeList getChildNodes() {
	return nativeGetChildNodes();
    }
    native NodeList nativeGetChildNodes();

    public Node getFirstChild() {
	return nativeGetFirstChild();
    }
    native Node nativeGetFirstChild();

    public Node getLastChild() {
	return nativeGetLastChild();
    }
    native Node nativeGetLastChild();

    public Node getNextSibling() {
	return nativeGetNextSibling();
    }
    native Node nativeGetNextSibling();

    public String getNodeName() {
	return nativeGetNodeName();
    }
    native String nativeGetNodeName();

    public short getNodeType() {
	return nativeGetNodeType();
    }
    native short nativeGetNodeType();

    public String getNodeValue() {
	return nativeGetNodeValue();
    }
    native String nativeGetNodeValue();

    public Document getOwnerDocument() {
	return nativeGetOwnerDocument();
    }
    native Document nativeGetOwnerDocument();

    public Node getParentNode() {
	return nativeGetParentNode();
    }
    native Node nativeGetParentNode();

    public Node getPreviousSibling() {
	return nativeGetPreviousSibling();
    }
    native Node nativeGetPreviousSibling();

    public boolean hasChildNodes() {
	return nativeHasChildNodes();
    }
    native boolean nativeHasChildNodes();

    public Node insertBefore(Node newChild, Node refChild) throws DOMException {
	return nativeInsertBefore(newChild, refChild);
    }
    native Node nativeInsertBefore(Node newChild, Node refChild) throws DOMException;
    public Node removeChild(Node oldChild) throws DOMException {
	return nativeRemoveChild(oldChild);
    }
    native Node nativeRemoveChild(Node oldChild) throws DOMException;

    public Node replaceChild(Node newChild, Node oldChild) throws DOMException {
	return nativeReplaceChild(newChild, oldChild);
    }
    native Node nativeReplaceChild(Node newChild, Node oldChild) throws DOMException;
    public void setNodeValue(String nodeValue) {
	nativeSetNodeValue(nodeValue);
    }
    native void nativeSetNodeValue(String nodeValue);

    public String getTextContent() throws DOMException {
	return nativeGetTextContent();
    }
    native String nativeGetTextContent() throws DOMException;

    protected void finalize() {
	nativeFinalize();
    }
    protected native void nativeFinalize();

    private boolean XPCOM_equals(Object o) {
	return nativeXPCOM_equals(o);
    }
    native boolean nativeXPCOM_equals(Object o);

    private int XPCOM_hashCode() {
	return nativeXPCOM_hashCode();
    }
    private native int nativeXPCOM_hashCode();

    //since DOM level 2
    public boolean supports(String feature, String version) {
	return nativeSupports(feature, version);
    }
    native boolean nativeSupports(String feature, String version);
    public String getNamespaceURI() {
	return nativeGetNamespaceURI();
    }
    native String nativeGetNamespaceURI();

    public String getPrefix() {
	return nativeGetPrefix();
    }
    native String nativeGetPrefix();

    public void setPrefix(String prefix) {
	nativeSetPrefix(prefix);
    }
    native void nativeSetPrefix(String prefix);

    public String getLocalName() {
	return nativeGetLocalName();
    }
    native String nativeGetLocalName();

    
    public void addEventListener(String type, 
                                 EventListener listener, 
                                 boolean useCapture) {

        long nativeListener = addNativeEventListener(type, listener, useCapture);

	Long lnode = new Long(p_nsIDOMNode);

        Vector listeners;
        
        //in conjunction with internal synchronization of Hashtable and Vector
        // this should be enough for safe concurrent access
        synchronized (javaDOMlisteners) {
            listeners = (Vector) javaDOMlisteners.get(lnode);
        
            if (listeners == null) {
              listeners = new Vector();
                javaDOMlisteners.put(lnode, listeners);
            }
        }
        
        if (nativeListener != 0) {

            NodeEventListener l = new NodeEventListener(type, 
                                                        listener, 
                                                        useCapture, 
                                                        nativeListener);
            listeners.add(l);
	} 
    }

    public void removeEventListener(String type, 
                                    EventListener listener, 
                                    boolean useCapture) {

        Vector listeners = (Vector) javaDOMlisteners.get(new Long(p_nsIDOMNode));

        if (listeners == null) 
            return;

        NodeEventListener l = new NodeEventListener(type, 
                                                    listener, useCapture, 0);

        int idx = listeners.indexOf(l);

	//if trying to remove non-existing listener then return 
        if (idx == -1) 
            return;

    	l = (NodeEventListener) listeners.remove(idx);
	
	    removeNativeEventListener(type, l.nativeListener, useCapture);
    }

    public boolean dispatchEvent(Event evt) throws DOMException {
        throw new UnsupportedOperationException();
    }
    
    private long  addNativeEventListener(String type, EventListener listener, boolean useCapture) {
	return nativeAddNativeEventListener(type, listener, useCapture);
    }
    private native long nativeAddNativeEventListener(String type, EventListener listener, boolean useCapture);

    private void  removeNativeEventListener(String type, long nativeListener, boolean useCapture) {
	nativeRemoveNativeEventListener(type, nativeListener, useCapture);
    }
    private native void  nativeRemoveNativeEventListener(String type, long nativeListener, boolean useCapture);

    public void         normalize() {
        throw new UnsupportedOperationException();
    };


public short compareDocumentPosition(Node other)  {
  throw new UnsupportedOperationException();
}
public String getBaseURI()  {
  throw new UnsupportedOperationException();
}
public Object getFeature(String feature, String version)  {
  throw new UnsupportedOperationException();
}
public Object getUserData(String key)  {
  throw new UnsupportedOperationException();
}
public boolean isDefaultNamespace(String namespaceURI) {
  throw new UnsupportedOperationException();
}
public boolean isEqualNode(Node arg)  {
  throw new UnsupportedOperationException();
}
public boolean isSameNode(Node other) {
  throw new UnsupportedOperationException();
}
public String lookupNamespaceURI(String prefix)  {
  throw new UnsupportedOperationException();
}
public String lookupPrefix(String namespaceURI)  {
  throw new UnsupportedOperationException();
}
public void setTextContent(String textContent)  {
  throw new UnsupportedOperationException();
}
public Object setUserData(String key, Object data, UserDataHandler handler)  {
  throw new UnsupportedOperationException();
}


}

