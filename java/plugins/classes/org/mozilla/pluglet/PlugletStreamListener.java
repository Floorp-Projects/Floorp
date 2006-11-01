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
package org.mozilla.pluglet;

import java.io.InputStream;
import org.mozilla.pluglet.mozilla.*;

/**
 * This is a stream listener that provides various methods, such as
 * notification that data is available in the input stream, that the URL 
 * has started to load, that it has stopped loading, etc.
 */
public interface PlugletStreamListener {
    // matches values in modules/plugin/public/plugindefs.h
    /**
     * Indicates normal stream type. This is a fixed integer value = 1.<p>
     * In this mode, the browser "pushes" data to the Pluglet as it 
     * arrives from the network. 
     */
    public static final int STREAM_TYPE_NORMAL = 1;
    /**
     * Indicates seek stream type. This is a fixed integer value = 2.<p>
     * In this mode, the Pluglet can randomly access ("pull") stream data.
     */
    public static final int STREAM_TYPE_SEEK = 2;
    /**
     * Indicates file stream type. This is a fixed integer value = 3.<p>
     * In this mode, the browser delivers ("pushes") data to 
     * the Pluglet as data is saved to a local file. The data
     * is delivered to the Pluglet via a series of write calls.
     */
    public static final int STREAM_TYPE_AS_FILE = 3;
    /**
     * Indicates file-only stream type. This is a fixed integer value = 4.<p>
     * In this mode, the browser saves stream data to a local file
     * and when it is done, it sends the full path of the file 
     * to the Pluglet (which can then "pull" the data).
     */
    public static final int STREAM_TYPE_AS_FILE_ONLY = 4;

    /**
     * This would be called by the browser to indicate that the URL has 
     * started to load.  This method is called only once -- 
     * when the URL starts to load.<p>
     * @param plugletInfo This is the interface (of type <code>PlugletStreamInfo</code>) 
     * through which the listener can get specific information about the stream, such as 
     * MIME type, URL, date modified, etc.<p>
     */
    void onStartBinding(PlugletStreamInfo streamInfo);
    /**
     * This would be called by the browser to indicate that data is available 
     * in the input stream.  This method is called whenever data is written 
     * into the input stream by the networking library -- unless the 
     * stream type is <code>STREAM_TYPE_AS_FILE_ONLY</code>. In the latter case,
     * <code>onFileAvailable</code> returns the path to the saved stream, and the
     * Pluglet can then "pull" the data.<p>
     * 
     * @param streamInfo This is the interface (of type <code>PlugletStreamInfo</code>) 
     * through which the listener can get specific information about the stream, such as 
     * MIME type, URL, date modified, etc.<p>
     * @param input  The input stream containing the data.  This stream can
     * be either a blocking or non-blocking stream.<p>
     * @param length    The amount of data that was just pushed into the stream.
     */
    void onDataAvailable(PlugletStreamInfo streamInfo, InputStream input,int  length);
    /**
     * This would be called by the browser to indicate the availability of a local 
     * file name for the stream data.<p>
     * @param streamInfo This is the interface (of type <code>PlugletStreamInfo</code>)  
     * through which the listener can get specific information about the stream, such as 
     * MIME type, URL, date modified, etc.<p>
     * @param fileName This specifies the full path to the file.
     */ 
    void onFileAvailable(PlugletStreamInfo streamInfo, String fileName);
    /**
     * This would be called by the browser to indicate that the URL has finished loading.
     *<p>
     * @param streamInfo This is the interface (of type <code>PlugletStreamInfo</code>)  
     * through which the listener can get specific information about the stream, such as 
     * MIME type, URL, date modified, etc.<p>
     * @param status This is an <code>int</code> (integer) to indicate the success 
     * or failure of the load. <code>0</code> (<code>NS_OK</code>) indicates successful 
     * loading; any other value indicates failure.
     */
    void onStopBinding(PlugletStreamInfo streamInfo,int status);
    /**
     * Returns the type of stream.<p>
     * @param int This is an integer representing the stream type:<p>
     * 1 for STREAM_TYPE_NORMAL<br> 
     * 2 for STREAM_TYPE_SEEK<br> 
     * 3 for STREAM_TYPE_AS_FILE<br> 
     * 4 for STREAM_TYPE_AS_FILE_ONLY 
     */
    int  getStreamType();
}





