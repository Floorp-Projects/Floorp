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

import org.w3c.dom.CharacterData;

public class CharacterDataImpl extends NodeImpl implements CharacterData {

    // instantiated from JNI only
    protected CharacterDataImpl() {}

    public native void appendData(String arg);
    public native void deleteData(int offset, int count);
    public native String getData();
    public native int getLength();
    public native void insertData(int offset, String arg);
    public native void replaceData(int offset, int count, String arg);
    public native void setData(String data);
    public native String substringData(int offset, int count);

    protected native void finalize();
}
