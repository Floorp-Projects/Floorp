/* 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
 * Inc. All Rights Reserved. 
 */
package org.mozilla.pluglet.mozilla;

import java.io.*;

class PlugletInputStream extends InputStream {
    private long peer;
    private PlugletInputStream(long peer) {
	this.peer = peer;
	nativeInitialize();
    }
    public  int read() throws IOException {
	byte b[] = new byte[1];
	return read(b,0,1);
    }
    public int read(byte b[], int off, int len) throws IOException {
	if (off>0) {
	    skip(off);
	}
	return read(b,len);
    }
    public native int available();
    public native void close();
    public synchronized void mark(int readlimit) {
    }
    public synchronized void reset() throws IOException {
    }
    public boolean markSupported() {
	return false;
    }
    private native int read(byte b[], int len);
    protected void finalize() {
        nativeFinalize();
    }
    private native void nativeFinalize();
    private native void nativeInitialize(); 
}
