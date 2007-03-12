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

import org.w3c.dom.CharacterData;

public class CharacterDataImpl extends NodeImpl implements CharacterData {

    // instantiated from JNI only
    protected CharacterDataImpl() {}

    public void appendData(String arg) {
	nativeAppendData(arg);
    }
    native void nativeAppendData(String arg);

    public void deleteData(int offset, int count) {
	nativeDeleteData(offset, count);
    }
    native void nativeDeleteData(int offset, int count);


    public String getData() {
	return nativeGetData();
    }
    native String nativeGetData();

    public int getLength() {
	return nativeGetLength();
    }
    native int nativeGetLength();

    public void insertData(int offset, String arg) {
	nativeInsertData(offset, arg);
    }
    native void nativeInsertData(int offset, String arg);

    public void replaceData(int offset, int count, String arg) {
	nativeReplaceData(offset, count, arg);
    }
    native void nativeReplaceData(int offset, int count, String arg);

    public void setData(String data) {
	nativeSetData(data);
    }
    native void nativeSetData(String data);

    public String substringData(int offset, int count) {
	return nativeSubstringData(offset, count);
    }
    native String nativeSubstringData(int offset, int count);
}
