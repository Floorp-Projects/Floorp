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

import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.DOMException;

import org.mozilla.util.ReturnRunnable;

public class NamedNodeMapImpl implements NamedNodeMap {

    private long p_nsIDOMNamedNodeMap = 0;
    // instantiated from JNI only
    private NamedNodeMapImpl() {}

    public int getLength() {
	Integer result = (Integer)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			int intResult = nativeGetLength();
			return Integer.valueOf(intResult);
		    }
		    public String toString() {
			return "NamedNodeMap.getLength";
		    }
		});
	return result.intValue();

    }
    native int nativeGetLength();

    public Node getNamedItem(String name) {
	final String finalName = name;
	Node result = (Node)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetNamedItem(finalName);
		    }
		    public String toString() {
			return "NamedNodeMap.getNamedItem";
		    }
		});
	return result;

    }
    native Node nativeGetNamedItem(String name);

    public Node item(int index) {
	final int finalIndex = index;
	Node result = (Node)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeItem(finalIndex);
		    }
		    public String toString() {
			return "NamedNodeMap.item";
		    }
		});
	return result;

    }
    native Node nativeItem(int index);

    public Node removeNamedItem(String name) {
	final String finalName = name;
	Node result = (Node)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeRemoveNamedItem(finalName);
		    }
		    public String toString() {
			return "NamedNodeMap.removeNamedItem";
		    }
		});
	return result;

    }
    native Node nativeRemoveNamedItem(String name);

    public Node setNamedItem(Node arg) {
	final Node finalArg = arg;
	Node result = (Node)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeSetNamedItem(finalArg);
		    }
		    public String toString() {
			return "NamedNodeMap.setNamedItem";
		    }
		});
	return result;

    }
    native Node nativeSetNamedItem(Node arg);

    public Node getNamedItemNS(String namespaceURI, String localName) {
        throw new UnsupportedOperationException();
    }

    public Node removeNamedItemNS(String namespaceURI, String name)
                                  throws DOMException {
        throw new UnsupportedOperationException();
    }    

    public Node         setNamedItemNS(Node arg)
	                               throws DOMException {
        throw new UnsupportedOperationException();
    }
}
