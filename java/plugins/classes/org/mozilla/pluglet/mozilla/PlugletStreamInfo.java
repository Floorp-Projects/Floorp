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

/**
 * This interface returns various information about the stream such as the
 * MIME type and whether the stream is seekable. 
 */
public interface PlugletStreamInfo {
    /**
     * Returns the MIME type for a particular stream.<p>
     * @return As stated above, returns the MIME type.
     */
    public String getContentType();
    /**
     * Indicates if a stream is seekable; that is, if it is possible
     * to move to a particular point in the stream.<p>
     * @return Returns true if the stream is seekable.
     */
    public boolean isSeekable();
    /**
     * Returns the length of a stream in bytes.
     */
    public int getLength();
    /**
     * Returns the time the data in the URL was last modified,
     * measured in seconds since 12:00 midnight, GMT, January 1, 1970.
     * <p>
     * @return Returns an integer value for the time since last
     * modification, as described above.
     */
    public int getLastModified();
    /** 
     * Specifies the URL that was used to originally request the stream.
     * <p>
     * @return Returns a String for the URL as described above.
     */ 
    public String getURL();
    /**
     * Requests reading from the input stream.
     */
    public void requestRead(ByteRanges ranges );
}


