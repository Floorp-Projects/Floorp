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

public class ComObject extends Object {
    // the XPCOM object pointer, stored as a long (for 64 bit compatibility)
    long objptr;

    // package-private constructors
    ComObject() {
    }

    ComObject(long p) {
	this.objptr = p;
	XPComUtilities.AddRef(objptr);
    }

    protected void finalize() throws Throwable {
	XPComUtilities.Release(objptr);
    }

    protected void __invokeByIndex(nsID iid, int index, Object[] args) {
	XPComUtilities.InvokeMethodByIndex(iid, this.objptr, index, args);
    }
}
