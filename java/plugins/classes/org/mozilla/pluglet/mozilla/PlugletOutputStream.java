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

class PlugletOutputStream extends OutputStream {
    private long peer;
    private byte buf[] = new byte[1];
    private PlugletOutputStream(long peer) {
	this.peer = peer;
	nativeInitialize();
    }
    public void write(int b) throws IOException {
	buf[0] = (byte)b;
	write(buf,0,1);
    }
    public void write(byte b[],	int off, int len) throws IOException {
	if (b == null) {
            throw new NullPointerException();
        } else if ((off < 0) || (off > b.length) || (len < 0) ||
                   ((off + len) > b.length) || ((off + len) < 0)) {
            throw new IndexOutOfBoundsException();
        } else if (len == 0) {
            return;
        }
	nativeWrite(b,off,len);
    }

    public native void flush();
    public native void close();
    private native void nativeWrite(byte b[],	int off, int len) throws IOException;
    private native void nativeFinalize();
    private native void nativeInitialize(); 
}
