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
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s):  Ed Burns <edburns@acm.org>
 */

package org.mozilla.webclient.impl.wrapper_native;

import java.io.InputStream;
import java.io.IOException;

public class NativeInputStream extends InputStream {

    //
    // relationship ivars
    // 
    private int nativeInputStream = -1;

    // 
    // ctors and initializers
    //

    public NativeInputStream(int nativeInputStream) {
	this.nativeInputStream = nativeInputStream;
    }

    //
    // Methods from InputStream
    //

    public int read() throws IOException {
	byte result[] = {(byte) -1};
	read(result);
	return (int) result[0];
    }
    
    public int read(byte[] b, int off, int len) throws IOException {
	if (-1 == nativeInputStream) {
	    throw new IOException("No NativeInputStream");
	}
	return nativeRead(nativeInputStream, b, off, len);
    }

    public long skip(long n) throws IOException {
	throw new UnsupportedOperationException();
    }

    public int available() throws IOException {
	if (-1 == nativeInputStream) {
	    throw new IOException("No NativeInputStream");
	}
	return nativeAvailable(nativeInputStream);
    }

    public void close() throws IOException {
	if (-1 == nativeInputStream) {
	    throw new IOException("No NativeInputStream");
	}
	nativeClose(nativeInputStream);
	nativeInputStream = -1;
    }

    public void mark(int readlimit) {
	throw new UnsupportedOperationException();
    }
    
    public void reset() throws IOException {
	throw new UnsupportedOperationException();
    }

    public boolean markSupported() {
	return false;
    }

    private native int nativeRead(int nativeInputStream, byte[] b, int off, 
				  int len) throws IOException;

    private native int nativeAvailable(int nativeInputStream) throws IOException;

    private native void nativeClose(int nativeClose) throws IOException;


} // end of class NativeInputStream
