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

public class nsXPIDLPluginInstancePeer implements PlugletPeer {

    public nsIXPIDLPluginInstancePeer instancePeer;

    public nsXPIDLPluginInstancePeer( nsIXPIDLPluginInstancePeer instancePeer ) {
        this.instancePeer = instancePeer;
    }

    public java.lang.String getMIMEType() {
        return instancePeer.getMIMEType();
    }

    public int getMode() {
        return instancePeer.getMode();
    }

    public java.lang.String getValue(int variable) {
        return instancePeer.getValue( variable );
    }

    public java.io.OutputStream newStream(java.lang.String type, java.lang.String target) {
        return new nsXPIDLOutputStream( instancePeer.newStream( type, target ) );
    }

    public void setWindowSize( int width, int height) {
        instancePeer.setWindowSize( width, height );
    }

    public void showStatus( java.lang.String message ) {
        instancePeer.showStatus( message );
    }

    public PlugletTagInfo getTagInfo() {
        return new nsXPIDLPluginTagInfo2( instancePeer.getTagInfo() );
    }
}
