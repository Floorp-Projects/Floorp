/* 
The contents of this file are subject to the Mozilla Public License
Version 1.0 (the "License"); you may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS"
basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
the License for the specific language governing rights and limitations
under the License.

The Initial Developer of the Original Code is Sun Microsystems,
Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
Inc. All Rights Reserved. 
*/

package org.mozilla.dom;

import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;

public class NamedNodeMapImpl extends NodeImpl implements NamedNodeMap {

    // instantiated from JNI only
    private NamedNodeMapImpl() {}

    public native int getLength();
    public native Node getNamedItem(String name);
    public native Node item(int index);
    public native Node removeNamedItem(String name);
    public native Node setNamedItem(Node arg);

    protected native void finalize();
}
