/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 */
package org.mozilla.pluglet;

import java.security.*;
import java.util.*;

class PlugletPolicy extends Policy {
    PlugletPolicy(Policy peer) {
        this.peer = peer;
        permissions = null;
    }
    void grantPermission(CodeSource codesource, PermissionCollection per) {
        if (permissions == null) {
            permissions = new Vector(10);
        }
        Object pair[] = new Object[2];
        pair[0] = codesource; pair[1] = per;
        permissions.add(pair);
    }
    public PermissionCollection getPermissions(CodeSource codesource) {
        PermissionCollection collection = peer.getPermissions(codesource);
        if (collection == null) {
            collection = new Permissions();
        }
        for (int i = 0; i < permissions.size();i++) {
            Object[] pair = (Object[])permissions.elementAt(i);
            CodeSource code = (CodeSource) pair[0];
            if (code.implies(codesource)) {
                for (Enumeration e = ((PermissionCollection)pair[1]).elements(); e.hasMoreElements() ;) {
                    collection.add((Permission)e.nextElement());
                }
            }
        }
        return collection;
    }
    public void  refresh() {
        peer.refresh();
    }
    private Policy peer;
    private Vector permissions;
}

