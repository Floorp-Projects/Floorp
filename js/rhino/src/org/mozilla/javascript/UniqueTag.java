/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Bukanov
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

package org.mozilla.javascript;

import java.io.Serializable;

/**
Class instances represent serializable tags to mark special values of Object references.
<p>
Compatibility note: under jdk 1.1 use org.mozilla.javascript.serialize.ScriptableInputStream to read serialized instances of UniqueTag as under this JDK version the default ObjectInputStream would not restore them correctly as it lacks support for readResolve method
*/
public final class UniqueTag implements Serializable
{
    private UniqueTag(int tagId) {
        _tagId = tagId;
    }

    public Object readResolve() {
        if (_tagId == ID_NOT_FOUND) { return NOT_FOUND; }
        else if (_tagId == ID_NULL_VALUE) { return NULL_VALUE; }
        Kit.codeBug();
        return null;
    }

// Overridden for better debug printouts
    public String toString() {
        String name;
        if (_tagId == ID_NOT_FOUND) { name = "NOT_FOUND"; }
        else if (_tagId == ID_NULL_VALUE) { name = "NULL_VALUE"; }
        else { Kit.codeBug(); name = null; }
        return super.toString()+": "+name;

    }

    private static final int ID_NOT_FOUND   = 1;
    private static final int ID_NULL_VALUE  = 2;

    private int _tagId;

/**
NOT_FOUND is useful to mark non-existing values.
*/
    public static final UniqueTag NOT_FOUND = new UniqueTag(ID_NOT_FOUND);

/**
NULL_VALUE is useful to distinguish between uninitialized and null values
*/
    public static final UniqueTag NULL_VALUE = new UniqueTag(ID_NULL_VALUE);

}

