/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.mozilla.pluglet.mozilla;

import java.io.*;

public class PlugletInputStream extends InputStream {
    private long peer;
    private byte buf[] = new byte[1];

    public PlugletInputStream(long peer) {
	this.peer = peer;
	nativeInitialize();
    }
    public  int read() throws IOException {
	if (read(buf,0,1) < 0) {
	    return -1;
	} else {
	    return buf[0] & 0xff;
	}
    }
    public int read(byte b[], int off, int len) throws IOException {
	if (b == null) {
            throw new NullPointerException();
        } else if ((off < 0) || (off > b.length) || (len < 0) ||
                   ((off + len) > b.length) || ((off + len) < 0)) {
            throw new IndexOutOfBoundsException();
        } else if (len == 0) {
            return 0;
        }
	return nativeRead(b,off,len);
    }
    public native int available();
    public native void close();
    public synchronized void mark(int readlimit) {
    }
    public synchronized void reset() throws IOException {
	throw new IOException("PlugletInputStream does not support mark");
    }
    public boolean markSupported() {
	return false;
    }
    protected void finalize() {
        nativeFinalize();
    }
    private native void nativeFinalize();
    private native void nativeInitialize(); 
    private native int nativeRead(byte b[], int off, int len) throws IOException;
}



