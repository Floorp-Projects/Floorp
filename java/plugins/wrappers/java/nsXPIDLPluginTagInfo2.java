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

public class nsXPIDLPluginTagInfo2 extends nsXPIDLPluginTagInfo implements PlugletTagInfo2 {

    public nsIXPIDLPluginTagInfo2 tagInfo;

    public nsXPIDLPluginTagInfo2( nsIXPIDLPluginTagInfo2 tagInfo ) {
        this.tagInfo = tagInfo;
    }

    public String getAlignment() {
        return tagInfo.getAlignment();
    }

    public String getParameter( String name ) {
        return tagInfo.getParameter( name );
    }

    public java.util.Properties getParameters() {
        // get atts
        int[] count = new int[1];
        String[][] names = new String[1][];
        String[][] values = new String[1][];
        tagInfo.getParameters( count, names, values );

        // create props
        java.util.Properties props = new java.util.Properties();
        for( int i = 0; i < count[0]; i++ ) {
            props.setProperty( names[0][i], values[0][i] );
        }

        // return props
        return props;
    }

    public int getBorderHorizSpace() {
        return tagInfo.getBorderHorizSpace();
    }

    public int getBorderVertSpace() {
        return tagInfo.getBorderVertSpace();
    }

    public String getDocumentBase() {
        return tagInfo.getDocumentBase();
    }

    public String getDocumentEncoding() {
        return tagInfo.getDocumentEncoding();
    }

    public String getTagText() {
        return tagInfo.getTagText();
    }

    public String getTagType() {
        return tagInfo.getTagType();
    }

    public int getUniqueID() {
        return tagInfo.getUniqueID();
    }

    public int getHeight() {
        return tagInfo.getHeight();
    }

    public int getWidth() {
        return tagInfo.getWidth();
    }

    public Object getDOMElement() {
        return null;
    }
}
