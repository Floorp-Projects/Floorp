/* 
The contents of this file are subject to the Mozilla Public License
Version 1.0 (the "License"); you may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS"
basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
the License for the specific language governing rights and limitations
under the License.

The Initial Developer of the Original Code is Sun Microsystems,
Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
Inc. All Rights Reserved. 
*/

package org.mozilla.dom;

import org.w3c.dom.Node;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.NodeList;
import org.w3c.dom.Document;

public class NodeImpl implements Node {

    /* The derived classes (Attr, CharacterData, DocumentFragment,
       Document, Element, EntityReference, NamedNodeMap,
       ProcessingInstruction) will also use this native pointer, since
       the nsIDOM coreDom classes follow the same hierarchy */

    private long p_nsIDOMNode = 0;

    // instantiated from JNI only
    protected NodeImpl() {}

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

    public native Node appendChild(Node newChild);
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
    public native Node insertBefore(Node newChild, Node refChild);
    public native Node removeChild(Node oldChild);
    public native Node replaceChild(Node newChild, Node oldChild);
    public native void setNodeValue(String nodeValue);

    protected native void finalize();

    private native boolean XPCOM_equals(Object o);
    private native int XPCOM_hashCode();
}
