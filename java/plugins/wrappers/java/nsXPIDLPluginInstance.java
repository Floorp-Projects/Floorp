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

public class nsXPIDLPluginInstance implements nsIXPIDLPluginInstance {

    public Pluglet instance;

    public nsXPIDLPluginInstance( Pluglet instance ) {
       this.instance = instance;
    }

    public void destroy() {
        instance.destroy();
    }

    public void initialize( nsIXPIDLPluginInstancePeer peer ) {
        PlugletPeer pPeer = new nsXPIDLPluginInstancePeer( peer );
        instance.initialize( pPeer );
    }

    public nsIXPIDLPluginStreamListener newStream() {
        return new nsXPIDLPluginStreamListener( instance.newStream() );
    }

    public void start() {
        instance.start();
    }

    public void stop() {
        instance.stop();
    }

    // from nsISupports
    public Object queryInterface( IID iid ) {
        Object result;
        if( nsISupports.IID.equals(iid)
            || nsIXPIDLPluginInstance.IID.equals(iid) ) {
            result = this;
        } else {
            result = null;
        }
        return result;
    }
}
