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
package org.mozilla.pluglet;

import java.io.InputStream;
import org.mozilla.pluglet.mozilla.*;
public interface PlugletStreamListener {
    // matches values in modules/plugin/public/plugindefs.h
    public static final int STREAM_TYPE_NORMAL = 1;
    public static final int STREAM_TYPE_SEEK = 2;
    public static final int STREAM_TYPE_AS_FILE = 3;
    public static final int STREAM_TYPE_AS_FILE_ONLY = 4;

    /**
     * Notify the observer that the URL has started to load.  This method is
     * called only once, at the beginning of a URL load.<BR><BR>
     */
    void onStartBinding(PlugletStreamInfo plugletInfo);
    /**
     * Notify the client that data is available in the input stream.  This
     * method is called whenver data is written into the input stream by the
     * networking library...<BR><BR>
     * 
     * @param input  The input stream containing the data.  This stream can
     * be either a blocking or non-blocking stream.
     * @param length    The amount of data that was just pushed into the stream.
     */
    void onDataAvailable(PlugletStreamInfo stremInfo, InputStream input,int  length);
    void onFileAvailable(PlugletStreamInfo streamInfo, String fileName);
    /**
     * Notify the observer that the URL has finished loading. 
     */
    void onStopBinding(PlugletStreamInfo streamInfo,int status);
    int  getStreamType();
}





