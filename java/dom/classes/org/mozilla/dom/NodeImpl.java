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

    public native boolean isSupported(String feature, String version);
    public native boolean hasAttributes();
    public native Node appendChild(Node newChild) throws DOMException;
    public native Node cloneNode(boolean deep);
    public native NamedNodeMap getAttributes();
    public native NodeList getChildNodes();
    public native Node getFirstChild();
    public native Node getLastChild();
    public native Node getNextSibling();
    public native String getNodeName();
    public native short getNodeType();
    public native String getNodeValue();
    public native Document getOwnerDocument();
    public native Node getParentNode();
    public native Node getPreviousSibling();
    public native boolean hasChildNodes();
    public native Node insertBefore(Node newChild, Node refChild) throws DOMException;
    public native Node removeChild(Node oldChild) throws DOMException;
    public native Node replaceChild(Node newChild, Node oldChild) throws DOMException;
    public native void setNodeValue(String nodeValue);
    public native String getTextContent() throws DOMException;

    protected native void finalize();

    private native boolean XPCOM_equals(Object o);
    private native int XPCOM_hashCode();

    //since DOM level 2
    public native boolean supports(String feature, String version);
    public native String getNamespaceURI();
    public native String getPrefix();
    public native void setPrefix(String prefix);
    public native String getLocalName();
    
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
    
    private native long  addNativeEventListener(String type,
                                             EventListener listener,
                                             boolean useCapture);

    private native void  removeNativeEventListener(String type,
                                                long nativeListener,
                                                boolean useCapture);

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

