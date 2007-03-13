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

import org.mozilla.util.ReturnRunnable;

public class CharacterDataImpl extends NodeImpl implements CharacterData {

    // instantiated from JNI only
    protected CharacterDataImpl() {}

    public void appendData(String arg) {
	final String finalArg = arg;
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			nativeAppendData(finalArg);
			return null;
		    }
		    public String toString() {
			return "CharacterData.appendData";
		    }
		});


    }
    native void nativeAppendData(String arg);

    public void deleteData(int offset, int count) {
	final int finalOffset = offset;
	final int finalCount = count;
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			nativeDeleteData(finalOffset, finalCount);
			return null;
		    }
		    public String toString() {
			return "CharacterData.deleteData";
		    }
		});

    }
    native void nativeDeleteData(int offset, int count);


    public String getData() {
	String result = (String)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetData();
		    }
		    public String toString() {
			return "CharacterData.getData";
		    }
		});
	return result;
    }
    native String nativeGetData();

    public int getLength() {
	int result;
	Integer integerResult = (Integer)
	    DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetLength();
		    }
		    public String toString() {
			return "CharacterData.getLength";
		    }
		});
	result = integerResult.intValue();
	return result;
    }
    native int nativeGetLength();

    public void insertData(int offset, String arg) {
	final int finalOffset = offset;
	final String finalArg = arg;
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			nativeInsertData(finalOffset, finalArg);
			return null;
		    }
		    public String toString() {
			return "CharacterData.insertData";
		    }
		});

    }
    native void nativeInsertData(int offset, String arg);

    public void replaceData(int offset, int count, String arg) {
	final int finalOffset = offset;
	final  int finalCount = count;
	final String finalArg = arg;
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			nativeReplaceData(finalOffset, finalCount, finalArg);
			return null;
		    }
		    public String toString() {
			return "CharacterData.replaceData";
		    }
		});

    }
    native void nativeReplaceData(int offset, int count, String arg);

    public void setData(String data) {
	final String finalData = data;
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			nativeSetData(finalData);
			return null;
		    }
		    public String toString() {
			return "CharacterData.setData";
		    }
		});


    }
    native void nativeSetData(String data);

    public String substringData(int offset, int count) {
	final int finalOffset = offset;
	final  int finalCount = count;
	String result = (String)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeSubstringData(finalOffset, finalCount);
		    }
		    public String toString() {
			return "CharacterData.substringData";
		    }
		});
	return result;

    }
    native String nativeSubstringData(int offset, int count);
}
