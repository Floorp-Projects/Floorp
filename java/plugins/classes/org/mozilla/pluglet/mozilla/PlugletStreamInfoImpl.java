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

class  PlugletStreamInfoImpl implements PlugletStreamInfo {
    private PlugletStreamInfoImpl(long peer) {
	this.peer = peer;
        nativeInitialize();
    }
    protected long peer;
    /**
     * Returns the MIME type.
     */
    public native String getContentType();
    public native boolean isSeekable();
    /**
     * Returns the number of bytes that can be read from this input stream
     */
    public native int getLength();
    public native int getLastModified();
    public native String getURL();
    /**
     * Request reading from input stream
     *
     * @param offset the start point for reading.
     * @param length the number of bytes to be read.
     */
    public native void requestRead(ByteRanges ranges);
    protected void finalize() {
        nativeFinalize();
    }
    private native void nativeFinalize();
    private native void nativeInitialize();
}


