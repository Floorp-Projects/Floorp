/* -*- Mode: java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Igor Kushnirskiy <idk@eng.sun.com>
 */
package org.mozilla.xpcom;

public class IID {
    public IID(String iid) {
        this.iid = (iid == null) ? "" : iid;
    }
    public boolean equals(Object obj) {
        if (! (obj instanceof IID)) { 
            return false;
        }
        boolean res = iid.equals(((IID)obj).iid);
        return res;
    }
    public String toString() {
        return "org.mozilla.xpcom.IID@"+iid;
    }
    public int hashCode() {
        int h = iid.hashCode(); 
        return h;
    }
    public String getString() {
        return iid;
    }
    private String iid;
    public static Class TYPE;
    static {
        try {
            TYPE = Class.forName("org.mozilla.xpcom.Proxy");
        } catch (Exception e) {  //it could not happen
            TYPE = null;
        }
    }
    
}



