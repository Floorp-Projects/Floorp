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

import org.w3c.dom.DocumentType;
import org.w3c.dom.NamedNodeMap;
import org.mozilla.util.ReturnRunnable;


public class DocumentTypeImpl  extends NodeImpl implements DocumentType {

    // instantiated from JNI only
    private DocumentTypeImpl() {}

    public String getName() {
	String result = (String)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetName();
		    }
		    public String toString() {
			return "DocumentType.getName";
		    }
		});
	return result;

    }
    native String nativeGetName();

    public NamedNodeMap getEntities() {
	NamedNodeMap result = (NamedNodeMap)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetEntities();
		    }
		    public String toString() {
			return "DocumentType.getEntities";
		    }
		});
	return result;

    }
    native NamedNodeMap nativeGetEntities();

    public NamedNodeMap getNotations() {
	NamedNodeMap result = (NamedNodeMap)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetNotations();
		    }
		    public String toString() {
			return "DocumentType.getNotations";
		    }
		});
	return result;

    }
    native NamedNodeMap nativeGetNotations();

    
    //since DOM level 2
    public String getPublicId() {
	String result = (String)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetPublicId();
		    }
		    public String toString() {
			return "DocumentType.getPublicId";
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
			return "DocumentType.getSystemId";
		    }
		});
	return result;

    }
    native String nativeGetSystemId();

    public String getInternalSubset() {
	String result = (String)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetInternalSubset();
		    }
		    public String toString() {
			return "DocumentType.getInternalSubset";
		    }
		});
	return result;

    }
    native String nativeGetInternalSubset();

}
