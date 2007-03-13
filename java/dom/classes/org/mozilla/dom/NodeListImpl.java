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
import org.w3c.dom.NodeList;

import org.mozilla.util.ReturnRunnable;


public class NodeListImpl implements NodeList {

    private long p_nsIDOMNodeList = 0;

    // instantiated from JNI or Document.createAttribute()
    private NodeListImpl() {}

    public boolean equals(Object o) {
	if (!(o instanceof NodeListImpl))
	    return false;
	else
	    return (XPCOM_equals(o));
    }

    public int hashCode(){
	return XPCOM_hashCode();
    }

    public int getLength() {
	Integer result = (Integer)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			int intResult = nativeGetLength();
			return Integer.valueOf(intResult);
		    }
		    public String toString() {
			return "NodeList.getLength";
		    }
		});
	return result;

    }
    native int nativeGetLength();

    public Node item(int index) {
	final int finalIndex = index;
	Node result = (Node)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeItem(finalIndex);
		    }
		    public String toString() {
			return "NodeList.item";
		    }
		});
	return result;

    }
    native Node nativeItem(int index);

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
			return "NodeList.XPCOM_equals";
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
			return "NodeList.XPCOM_hashCode";
		    }
		});
	return result.intValue();

    }
    private native int nativeXPCOM_hashCode();
}
