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

public class AttrImpl extends NodeImpl implements Attr {

    // instantiated from JNI or Document.createAttribute()
    private AttrImpl() {}

    public String getName() {
	return nativeGetName();
    }
    native String nativeGetName();

    public boolean getSpecified() {
	return nativeGetSpecified();
    }
    native boolean nativeGetSpecified();

    public String getValue() {
	return nativeGetValue();
    }
    native String nativeGetValue();

    public void setValue(String value) {
	nativeSetValue(value);
    }
    native void nativeSetValue(String value);

    /**
     * The <code>Element</code> node this attribute is attached to or
     * <code>null</code> if this attribute is not in use.
     * @since DOM Level 2
     */
    public Element getOwnerElement() {
	return nativeGetOwnerElement();
    }
    native Element nativeGetOwnerElement();


    public boolean isId() {
        throw new UnsupportedOperationException();
    }

    public TypeInfo getSchemaTypeInfo() {
        throw new UnsupportedOperationException();
    }

}
