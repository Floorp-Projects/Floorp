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

public class nsXPIDLPlugin implements nsIXPIDLPlugin {

    public PlugletFactory plugin;

    public nsXPIDLPlugin( PlugletFactory plugin ) {
        this.plugin = plugin;
    }

    public Object createPluginInstance( nsISupports aOuter,
                                        IID iid,
                                        String pluginMIMEType ) {
        return plugin.createPluglet( pluginMIMEType );
    }

    public void initialize() {
        // ??? Where can I take PlugletManager in this place?
    }

    public void shutdown() {
        plugin.shutdown();
    }

    // from nsIFactory
    public void lockFactory( boolean lock ) {
    }

    public Object createInstance( nsISupports instance, IID iid ) {
        return createPluginInstance( instance, iid, null );
    }

    // from nsISupports
    public Object queryInterface( IID iid ) {
        Object result;
        if( nsISupports.IID.equals(iid)
            || nsIXPIDLPlugin.IID.equals(iid) ) {
            result = this;
        } else {
            result = null;
        }
        return result;
    }
}
