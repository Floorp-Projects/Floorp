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

public final class nsID {
    // ptr to a C++ nsID
    long nsidptr;

    static {
	XPComUtilities.initXPCOM();
    }

    /** DO NOT USE: only for creating nsIDs in native code */
    nsID() {
	nsidptr = 0x0;
    }

    public nsID(int m0, short m1, short m2, 
		byte m30, byte m31, byte m32, byte m33, 
		byte m34, byte m35, byte m36, byte m37) {
	// What fun
	this.NewIDPtr(m0, m1, m2, m30, m31, m32, m33, m34, m35, m36, m37);
    }

    private native void NewIDPtr(int m0, short m1, short m2, 
				 byte m30, byte m31, byte m32, byte m33, 
				 byte m34, byte m35, byte m36, byte m37);

    public nsID(String idstring) throws NumberFormatException {
	this.NewIDPtr(idstring);
	if (this.nsidptr == 0) {
	    throw new NumberFormatException("Can't convert '" +
					    idstring
					    + "' to an nsID");
	}
    }

    private native void NewIDPtr(String idstring);

    protected native void finalize() throws Throwable;

    public native boolean equals(Object other);

    public native String toString();

    public native int hashCode();

    public static void main(String[] argv) {
	System.loadLibrary("xpjava");

	nsID iid1 = new nsID("57ecad90-ae1a-11d1-b66c-00805f8a2676");
	nsID iid2 = new nsID((int)0x57ecad90, (short)0xae1a, (short)0x11d1, 
			     (byte)0xb6, (byte)0x6c, (byte)0x00, (byte)0x80, 
			     (byte)0x5f, (byte)0x8a, (byte)0x26, (byte)0x76);

	System.out.println(iid1);
	System.out.println(iid2);

	System.out.println(iid1.equals(iid2));
	System.out.println(iid1.equals(null));
	System.out.println(iid1.hashCode());
    }
}
