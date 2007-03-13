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
import org.w3c.dom.DOMException;
import org.w3c.dom.Element;
import org.w3c.dom.TypeInfo;

import org.mozilla.util.ReturnRunnable;

public class AttrImpl extends NodeImpl implements Attr {

    // instantiated from JNI or Document.createAttribute()
    private AttrImpl() {}

    public String getName() {
	String result = (String)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			
			return nativeGetName();
		    }
		    public String toString() {
			return "DOMAccessor.register";
		    }
		});
	return result;
    }
    native String nativeGetName();

    public boolean getSpecified() {
	Boolean boolResult = (Boolean)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return new Boolean(nativeGetSpecified());
		    }
		    public String toString() {
			return "DOMAccessor.register";
		    }
		});
	return boolResult.booleanValue();
    }
    native boolean nativeGetSpecified();

    public String getValue() {
	String result = (String)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetValue();
		    }
		    public String toString() {
			return "DOMAccessor.register";
		    }
		});
	return result;
    }
    native String nativeGetValue();

    public void setValue(String value) {
	final String finalValue = value;
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			nativeSetValue(finalValue);
			return null;
		    }
		    public String toString() {
			return "Attr.setValue";
		    }
		});

    }
    native void nativeSetValue(String value);

    /**
     * The <code>Element</code> node this attribute is attached to or
     * <code>null</code> if this attribute is not in use.
     * @since DOM Level 2
     */
    public Element getOwnerElement() {
	Element result = (Element)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetOwnerElement();
		    }
		    public String toString() {
			return "DOMAccessor.register";
		    }
		});
	return result;
    }
    native Element nativeGetOwnerElement();


    public boolean isId() {
        throw new UnsupportedOperationException();
    }

    public TypeInfo getSchemaTypeInfo() {
        throw new UnsupportedOperationException();
    }

}
