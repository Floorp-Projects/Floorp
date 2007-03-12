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

public class NamedNodeMapImpl implements NamedNodeMap {

    private long p_nsIDOMNamedNodeMap = 0;
    // instantiated from JNI only
    private NamedNodeMapImpl() {}

    public int getLength() {
	return nativeGetLength();
    }
    native int nativeGetLength();

    public Node getNamedItem(String name) {
	return nativeGetNamedItem(name);
    }
    native Node nativeGetNamedItem(String name);

    public Node item(int index) {
	return nativeItem(index);
    }
    native Node nativeItem(int index);

    public Node removeNamedItem(String name) {
	return nativeRemoveNamedItem(name);
    }
    native Node nativeRemoveNamedItem(String name);

    public Node setNamedItem(Node arg) {
	return nativeSetNamedItem(arg);
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
