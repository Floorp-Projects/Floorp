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

public class nsXPIDLPluginManager implements PlugletManager {

    public nsIXPIDLPluginManager pluginManager;

    public nsXPIDLPluginManager() {
    }

    public nsXPIDLPluginManager( nsIXPIDLPluginManager pluginManager ) {
        this.pluginManager = pluginManager;
    }

    public void getURL( Pluglet pluglet,
                 java.net.URL url,
                 java.lang.String target,
                 PlugletStreamListener streamListener,
                 java.lang.String altHost,
                 java.net.URL referrer,
                 boolean forceJSEnabled ) {
        nsISupports pInstance = new nsXPIDLPluginInstance( pluglet );
        nsIXPIDLPluginStreamListener sListener = new nsXPIDLPluginStreamListener( streamListener );
        pluginManager.getURL( pInstance,
                              url.toString(),
                              target,
                              sListener,
                              altHost,
                              referrer.toString(),
                              forceJSEnabled );
    }

    public void postURL( Pluglet pluglet,
                         java.net.URL url,
                         int postDataLen,
                         byte[] postData,
                         boolean isFile,
                         java.lang.String target,
                         PlugletStreamListener streamListener,
                         java.lang.String altHost,
                         java.net.URL referrer,
                         boolean forceJSEnabled,
                         int postHeadersLength,
                         byte[] postHeaders) {
        nsISupports pInstance = new nsXPIDLPluginInstance( pluglet );
        nsIXPIDLPluginStreamListener sListener = new nsXPIDLPluginStreamListener( streamListener );
        pluginManager.postURL( pInstance,
                               url.toString(),
                               postDataLen,
                               postData,
                               postHeadersLength,
                               new String(postHeaders),
                               isFile,
                               target,
                               sListener,
                               altHost,
                               referrer.toString(),
                               forceJSEnabled );
    }

    public void reloadPluglets( boolean reloadPages ) {
        pluginManager.reloadPlugins( reloadPages );
    }

    public java.lang.String userAgent() {
        return pluginManager.userAgent();
    }
}
