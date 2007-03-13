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

import org.mozilla.util.ReturnRunnable;

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
	final String finalFeature = feature;
	final  String finalVersion = version;
	Boolean result = (Boolean)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			boolean booleanResult = nativeIsSupported(finalFeature, finalVersion);
			return booleanResult ? Boolean.TRUE : Boolean.FALSE;
		    }
		    public String toString() {
			return "Node.isSupported";
		    }
		});
	return result.booleanValue(); 

    }
    native boolean nativeIsSupported(String feature, String version);

    public boolean hasAttributes() {
	Boolean result = (Boolean)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			boolean booleanResult = nativeHasAttributes();
			return booleanResult ? Boolean.TRUE : Boolean.FALSE;
		    }
		    public String toString() {
			return "Node.hasAttributes";
		    }
		});
	return result.booleanValue();

    }
    native boolean nativeHasAttributes();

    public Node appendChild(Node newChild) throws DOMException {
	final Node finalNewChild = newChild;
	Node result = (Node)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeAppendChild(finalNewChild);
		    }
		    public String toString() {
			return "Node.appendChild";
		    }
		});
	return result;

    }
    native Node nativeAppendChild(Node newChild) throws DOMException;

    public Node cloneNode(boolean deep) {
	final boolean finalDeep = deep;
	Node result = (Node)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeCloneNode(finalDeep);
		    }
		    public String toString() {
			return "Node.cloneNode";
		    }
		});
	return result;

    }
    native Node nativeCloneNode(boolean deep);

    public NamedNodeMap getAttributes() {
	NamedNodeMap result = (NamedNodeMap)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetAttributes();
		    }
		    public String toString() {
			return "Node.getAttributes";
		    }
		});
	return result;

    }
    native NamedNodeMap nativeGetAttributes();

    public NodeList getChildNodes() {
	NodeList result = (NodeList)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetChildNodes();
		    }
		    public String toString() {
			return "Node.getChildNodes";
		    }
		});
	return result;

    }
    native NodeList nativeGetChildNodes();

    public Node getFirstChild() {
	Node result = (Node)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetFirstChild();
		    }
		    public String toString() {
			return "Node.getFirstChild";
		    }
		});
	return result;

    }
    native Node nativeGetFirstChild();

    public Node getLastChild() {
	Node result = (Node)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetLastChild();
		    }
		    public String toString() {
			return "Node.getLastChild";
		    }
		});
	return result;

    }
    native Node nativeGetLastChild();

    public Node getNextSibling() {
	Node result = (Node)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetNextSibling();
		    }
		    public String toString() {
			return "Node.getNextSibling";
		    }
		});
	return result;

    }
    native Node nativeGetNextSibling();

    public String getNodeName() {
	String result = (String)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetNodeName();
		    }
		    public String toString() {
			return "Node.getNodeName";
		    }
		});
	return result;

    }
    native String nativeGetNodeName();

    public short getNodeType() {
	Short result = (Short)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			short shortResult = nativeGetNodeType();
			return Short.valueOf(shortResult);
		    }
		    public String toString() {
			return "Node.getNodeType";
		    }
		});
	return result.shortValue();

    }
    native short nativeGetNodeType();

    public String getNodeValue() {
	String result = (String)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetNodeValue();
		    }
		    public String toString() {
			return "Node.getNodeValue";
		    }
		});
	return result;

    }
    native String nativeGetNodeValue();

    public Document getOwnerDocument() {
	Document result = (Document)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetOwnerDocument();
		    }
		    public String toString() {
			return "Node.getOwnerDocument";
		    }
		});
	return result;

    }
    native Document nativeGetOwnerDocument();

    public Node getParentNode() {
	Node result = (Node)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetParentNode();
		    }
		    public String toString() {
			return "Node.getParentNode";
		    }
		});
	return result;

    }
    native Node nativeGetParentNode();

    public Node getPreviousSibling() {
	Node result = (Node)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetPreviousSibling();
		    }
		    public String toString() {
			return "Node.getPreviousSibling";
		    }
		});
	return result;

    }
    native Node nativeGetPreviousSibling();

    public boolean hasChildNodes() {
	Boolean result = (Boolean)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			boolean booleanResult = nativeHasChildNodes();
			return booleanResult ? Boolean.TRUE : Boolean.FALSE;
		    }
		    public String toString() {
			return "Node.hasChildNodes";
		    }
		});
	return result.booleanValue();

    }
    native boolean nativeHasChildNodes();

    public Node insertBefore(Node newChild, Node refChild) throws DOMException {
	final Node finalNewChild = newChild;
	final  Node finalRefChild = refChild;
	Node result = (Node)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeInsertBefore(finalNewChild, finalRefChild);
		    }
		    public String toString() {
			return "Node.insertBefore";
		    }
		});
	return result; 

    }
    native Node nativeInsertBefore(Node newChild, Node refChild) throws DOMException;

    public Node removeChild(Node oldChild) throws DOMException {
	final Node finalOldChild = oldChild;
	Node result = (Node)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeRemoveChild(finalOldChild);
		    }
		    public String toString() {
			return "Node.removeChild";
		    }
		});
	return result;

    }
    native Node nativeRemoveChild(Node oldChild) throws DOMException;

    public Node replaceChild(Node newChild, Node oldChild) throws DOMException {
	final Node finalNewChild = newChild;
	final  Node finalOldChild = oldChild;
	Node result = (Node)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeReplaceChild(finalNewChild, finalOldChild);
		    }
		    public String toString() {
			return "Node.replaceChild";
		    }
		});
	return result; 

    }
    native Node nativeReplaceChild(Node newChild, Node oldChild) throws DOMException;

    public void setNodeValue(String nodeValue) {
	final String finalNodeValue = nodeValue;
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			nativeSetNodeValue(finalNodeValue);
			return null;
		    }
		    public String toString() {
			return "Node.setNodeValue";
		    }
		});


    }
    native void nativeSetNodeValue(String nodeValue);

    public String getTextContent() throws DOMException {
	String result = (String)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetTextContent();
		    }
		    public String toString() {
			return "Node.getTextContent";
		    }
		});
	return result;

    }
    native String nativeGetTextContent() throws DOMException;

    protected void finalize() {
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			nativeFinalize();
			return null;
		    }
		    public String toString() {
			return "Node.finalize";
		    }
		});

    }
    protected native void nativeFinalize();

    private boolean XPCOM_equals(Object o) {
	final Object finalO = o;
	Boolean bool = (Boolean)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			boolean booleanResult = nativeXPCOM_equals(finalO);
			return booleanResult ? Boolean.TRUE : Boolean.FALSE;
		    }
		    public String toString() {
			return "Node.XPCOM_equals";
		    }
		});
	return bool.booleanValue();

    }
    native boolean nativeXPCOM_equals(Object o);

    private int XPCOM_hashCode() {
	Integer result = (Integer)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			int intResult = nativeXPCOM_hashCode();
			return Integer.valueOf(intResult);
		    }
		    public String toString() {
			return "Node.XPCOM_hashCode";
		    }
		});
	return result.intValue();

    }
    private native int nativeXPCOM_hashCode();

    //since DOM level 2
    public boolean supports(String feature, String version) {
	final String finalFeature = feature;
	final  String finalVersion = version;
	Boolean result = (Boolean)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			boolean booleanResult = 
			    nativeSupports(finalFeature, finalVersion);
			return booleanResult ? Boolean.TRUE : Boolean.FALSE;
		    }
		    public String toString() {
			return "Node.supports";
		    }
		});
	return result.booleanValue(); 

    }
    native boolean nativeSupports(String feature, String version);

    public String getNamespaceURI() {
	String result = (String)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetNamespaceURI();
		    }
		    public String toString() {
			return "Node.getNamespaceURI";
		    }
		});
	return result;

    }
    native String nativeGetNamespaceURI();

    public String getPrefix() {
	String result = (String)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetPrefix();
		    }
		    public String toString() {
			return "Node.getPrefix";
		    }
		});
	return result;

    }
    native String nativeGetPrefix();

    public void setPrefix(String prefix) {
	final String finalPrefix = prefix;
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			nativeSetPrefix(finalPrefix);
			return null;
		    }
		    public String toString() {
			return "Node.setPrefix";
		    }
		});
    }
    native void nativeSetPrefix(String prefix);

    public String getLocalName() {
	String result = (String)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetLocalName();
		    }
		    public String toString() {
			return "Node.getLocalName";
		    }
		});
	return result;

    }
    native String nativeGetLocalName();

    
    public void addEventListener(String type, EventListener listener, boolean useCapture) {
	final String finalType = type;
	final  EventListener finalListener = listener;
	final boolean finalUseCapture = useCapture;

	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			long nativeListener = 
			    addNativeEventListener(finalType, finalListener, 
						   finalUseCapture);
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
			    
			    NodeEventListener l = new NodeEventListener(finalType, 
									finalListener, 
									finalUseCapture, 
									nativeListener);
			    listeners.add(l);
			} 
			return null;
		    }
		    public String toString() {
			return "Node.addEventListener";
		    }
		});

    }

    public void removeEventListener(String type, EventListener listener, boolean useCapture) {
	final String finalType = type;
	final  EventListener finalListener = listener;
	final boolean finalUseCapture = useCapture;
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			Vector listeners = (Vector) javaDOMlisteners.get(new Long(p_nsIDOMNode));
			
			if (listeners == null) 
			    return null;
			
			NodeEventListener l = new NodeEventListener(finalType, 
								    finalListener, 
								    finalUseCapture, 0);

			int idx = listeners.indexOf(l);
			
			//if trying to remove non-existing listener then return 
			if (idx == -1) 
			    return null;

			l = (NodeEventListener) listeners.remove(idx);
			
			removeNativeEventListener(finalType, l.nativeListener, 
						  finalUseCapture);
			return null;
		    }
		    public String toString() {
			return "Node.removeEventListener";
		    }
		});

    }

    public boolean dispatchEvent(Event evt) throws DOMException {
        throw new UnsupportedOperationException();
    }
    
    private long  addNativeEventListener(String type, EventListener listener, boolean useCapture) {
	final String finalType = type;
	final  EventListener finalListener = listener;
	final boolean finalUseCapture = useCapture;
	Long result = (Long)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			long longResult =
			    nativeAddNativeEventListener(finalType, 
							 finalListener, 
							 finalUseCapture);
			return Long.valueOf(longResult);
		    }
		    public String toString() {
			return "Node.addNativeEventListener";
		    }
		});
	return result.longValue();

    }
    private native long nativeAddNativeEventListener(String type, EventListener listener, boolean useCapture);

    private void  removeNativeEventListener(String type, long nativeListener, boolean useCapture) {
	final String finalType = type;
	final  long finalNativeListener = nativeListener;
	final boolean finalUseCapture = useCapture;
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			nativeRemoveNativeEventListener(finalType, 
							finalNativeListener, 
							finalUseCapture);
			return null;
		    }
		    public String toString() {
			return "Node.removeNativeEventListener";
		    }
		});

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

