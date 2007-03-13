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

import org.w3c.dom.ProcessingInstruction;

import org.mozilla.util.ReturnRunnable;

public class ProcessingInstructionImpl extends NodeImpl implements ProcessingInstruction {

    // instantiated from JNI or Document.createProcessingInstruction()
    private ProcessingInstructionImpl() {}

    public String getData() {
	String result = (String)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetData();
		    }
		    public String toString() {
			return "ProcessingInstruction.getData";
		    }
		});
	return result;

    }
    native String nativeGetData();

    public String getTarget() {
	String result = (String)
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			return nativeGetTarget();
		    }
		    public String toString() {
			return "ProcessingInstruction.getTarget";
		    }
		});
	return result;

    }
    native String nativeGetTarget();

    public void setData(String data) {
	final String finalData = data;
	DOMAccessor.getRunner().
	    pushBlockingReturnRunnable(new ReturnRunnable() {
		    public Object run() {
			nativeSetData(finalData);
			return null;
		    }
		    public String toString() {
			return "ProcessingInstruction.setData";
		    }
		});

    }
    native void nativeSetData(String data);

}
