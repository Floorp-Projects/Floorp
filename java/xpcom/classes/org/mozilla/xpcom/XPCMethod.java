/*
 * The contents of this file are subject to the Mozilla Public License 
 * Version 1.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at 
 * http://www.mozilla.org/MPL/ 
 *
 * Software distributed under the License is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for 
 * the specific language governing rights and limitations under the License.
 *
 * Contributors:
 *    Frank Mitchell (frank.mitchell@sun.com)
 */
package org.mozilla.xpcom;

public final class XPCMethod extends Object {
    // PENDING: symbolic names for type codes

    // PENDING: use an nsID equivalent
    nsID iid;
    String methodName;

    // Pointer to XPCOM MethodInfo ptr
    long infoptr;
    // Pointer to C++ argument array template
    long frameptr;
    // Vtable offset
    int  offset;
    // Number of parameters
    int  count;

    // PENDING: Add lookup by name
    // PENDING: Add metadata (in, out, retval, types)
    public // PENDING: make nonpublic ASAP
    XPCMethod(String iidString, String methodName)
	throws XPCOMException {
	this.iid = new nsID(iidString);
	this.methodName = methodName;
	offset = this.init(this.iid, this.methodName);
    }

    public // PENDING: make nonpublic ASAP
    XPCMethod(nsID iid, String methodName)
	throws XPCOMException {
	this.iid = iid;
	this.methodName = methodName;
	offset = this.init(this.iid, methodName);
    }

    private native int init(nsID iid, String methodName) 
	throws XPCOMException;

    protected void finalize() throws Throwable {
	infoptr = 0;
	destroyPeer(frameptr);
	frameptr = 0;
    }

    private native void destroyPeer(long ptr) throws XPCOMException;

    public int getNumberOfParameters() {
	return count;
    }

    public native int getParameterType(int index) 
	throws IndexOutOfBoundsException, XPCOMException;

    public native int getParameterClass(int index) 
	throws IndexOutOfBoundsException, XPCOMException;

    public native void invoke(ComObject target, Object[] params)
	throws XPCOMException;
}
