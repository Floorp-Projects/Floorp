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

public class ID {
    public ID(String id) {
        this.id = ((id == null) ? "" : id).toLowerCase();;
    }
    public boolean equals(Object obj) {
        if (! (obj instanceof IID)) { 
            return false;
        }
        boolean res = id.equals(((ID)obj).id);
        return res;
    }
    public String toString() {
        return getPrefixForToString()+id;
    }
    public int hashCode() {
        int h = id.hashCode(); 
        return h;
    }
    public String getString() {
        return id;
    }
    protected String id;
    protected String getPrefixForToString() {
        return "org.mozilla.xpcom.ID@";
    }
}




