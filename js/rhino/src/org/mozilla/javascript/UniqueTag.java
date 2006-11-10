/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Igor Bukanov
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 or later (the "GPL"), in which
 * case the provisions of the GPL are applicable instead of those above. If
 * you wish to allow use of your version of this file only under the terms of
 * the GPL and not to allow others to use your version of this file under the
 * MPL, indicate your decision by deleting the provisions above and replacing
 * them with the notice and other provisions required by the GPL. If you do
 * not delete the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.javascript;

import java.io.Serializable;

/**
 * Class instances represent serializable tags to mark special Object values.
 * <p>
 * Compatibility note: under jdk 1.1 use
 * org.mozilla.javascript.serialize.ScriptableInputStream to read serialized
 * instances of UniqueTag as under this JDK version the default
 * ObjectInputStream would not restore them correctly as it lacks support
 * for readResolve method
 */
public final class UniqueTag implements Serializable
{
    static final long serialVersionUID = -4320556826714577259L;

    private static final int ID_NOT_FOUND    = 1;
    private static final int ID_NULL_VALUE   = 2;
    private static final int ID_DOUBLE_MARK  = 3;

    /**
     * Tag to mark non-existing values.
     */
    public static final UniqueTag
        NOT_FOUND = new UniqueTag(ID_NOT_FOUND);

    /**
     * Tag to distinguish between uninitialized and null values.
     */
    public static final UniqueTag
        NULL_VALUE = new UniqueTag(ID_NULL_VALUE);

    /**
     * Tag to indicate that a object represents "double" with the real value
     * stored somewhere else.
     */
    public static final UniqueTag
        DOUBLE_MARK = new UniqueTag(ID_DOUBLE_MARK);

    private final int tagId;

    private UniqueTag(int tagId)
    {
        this.tagId = tagId;
    }

    public Object readResolve()
    {
        switch (tagId) {
          case ID_NOT_FOUND:
            return NOT_FOUND;
          case ID_NULL_VALUE:
            return NULL_VALUE;
          case ID_DOUBLE_MARK:
            return DOUBLE_MARK;
        }
        throw new IllegalStateException(String.valueOf(tagId));
    }

// Overridden for better debug printouts
    public String toString()
    {
        String name;
        switch (tagId) {
          case ID_NOT_FOUND:
            name = "NOT_FOUND";
            break;
          case ID_NULL_VALUE:
            name = "NULL_VALUE";
            break;
          case ID_DOUBLE_MARK:
            name = "DOUBLE_MARK";
            break;
          default:
            throw Kit.codeBug();
        }
        return super.toString()+": "+name;
    }

}

