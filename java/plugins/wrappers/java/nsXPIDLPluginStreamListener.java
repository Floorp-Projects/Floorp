/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Serge Pikalev <sep@sparc.spb.su>
 */

import org.mozilla.xpcom.*;
import org.mozilla.pluglet.*;
import org.mozilla.pluglet.mozilla.*;

public class nsXPIDLPluginStreamListener implements nsIXPIDLPluginStreamListener {

    public PlugletStreamListener streamListener;

    public nsXPIDLPluginStreamListener( PlugletStreamListener streamListener ) {
        this.streamListener = streamListener;
    }

    public int getStreamType() {
        return streamListener.getStreamType();
    }

    public void onDataAvailable( nsIXPIDLPluginStreamInfo streamInfo,
                                 nsIXPIDLInputStream input,
                                 int length) {
        PlugletStreamInfo sInfo = new nsXPIDLPluginStreamInfo( streamInfo );
        java.io.InputStream iStream = new nsXPIDLInputStream( input );
        streamListener.onDataAvailable( sInfo, iStream, length );
    }

    public void onFileAvailable( nsIXPIDLPluginStreamInfo streamInfo,
                                 String fileName ) {
        PlugletStreamInfo sInfo = new nsXPIDLPluginStreamInfo( streamInfo );
        streamListener.onFileAvailable( sInfo, fileName );
    }

    public void onStartBinding( nsIXPIDLPluginStreamInfo streamInfo ) {
        PlugletStreamInfo sInfo = new nsXPIDLPluginStreamInfo( streamInfo );
        streamListener.onStartBinding( sInfo );
    }

    public void onStopBinding( nsIXPIDLPluginStreamInfo streamInfo, int status) {
        PlugletStreamInfo sInfo = new nsXPIDLPluginStreamInfo( streamInfo );
        streamListener.onStopBinding( sInfo, status );
    }

    // from nsISupports
    public Object queryInterface( IID iid ) {
        Object result;
        if( nsISupports.IID.equals(iid)
             || nsIXPIDLPluginStreamListener.IID.equals(iid) ) {
            result = this;
        } else {
            result = null;
        }
        return result;
    }
}
