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

public class nsXPIDLPluginTagInfo implements PlugletTagInfo {

    public nsIXPIDLPluginTagInfo tagInfo;

    public String getAttribute( String name ) {
        return tagInfo.getAttribute( name );
    }

    public java.util.Properties getAttributes() {
        // get atts
        int[] count = new int[1];
        String[][] names = new String[1][];
        String[][] values = new String[1][];
        tagInfo.getAttributes( count, names, values );

        // create props
        java.util.Properties props = new java.util.Properties();
        for( int i = 0; i < count[0]; i++ ) {
            props.setProperty( names[0][i], values[0][i] );
        }

        // return props
        return props;
    }

    public Object getDOMElement() {
        return null;
    }
}
