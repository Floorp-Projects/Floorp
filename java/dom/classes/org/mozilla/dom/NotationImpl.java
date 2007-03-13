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

import org.w3c.dom.Notation;

import org.mozilla.util.ReturnRunnable;


public class NotationImpl extends NodeImpl implements Notation {

    // instantiated from JNI only
    private NotationImpl() {}
    
    public String getPublicId() {
	String result = (String)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetPublicId();
		    }
		    public String toString() {
			return "Notation.getPublicId";
		    }
		});
	return result;

    }
    native String nativeGetPublicId();

    public String getSystemId() {
	String result = (String)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetSystemId();
		    }
		    public String toString() {
			return "Notation.getSystemId";
		    }
		});
	return result;

    }
    native String nativeGetSystemId();

}
