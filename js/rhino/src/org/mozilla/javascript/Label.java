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
 * May 6, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

public class Label {
    
    private static final int FIXUPTABLE_SIZE = 8;

    private static final boolean DEBUG = true;

    public Label()
    {
        itsPC = -1;
    }

    public short getPC()
    {
        return itsPC;
    }
    
    public void fixGotos(byte theCodeBuffer[])
    {
        if (DEBUG) {
            if ((itsPC == -1) && (itsFixupTable != null))
                throw new RuntimeException("Unlocated label");
        }
        if (itsFixupTable != null) {
            for (int i = 0; i < itsFixupTableTop; i++) {
                int fixupSite = itsFixupTable[i];
                // -1 to get delta from instruction start
                short offset = (short)(itsPC - (fixupSite - 1));
                theCodeBuffer[fixupSite++] = (byte)(offset >> 8);
                theCodeBuffer[fixupSite] = (byte)offset;
            }
        }
        itsFixupTable = null;
    }

    public void setPC(short thePC)
    {
        if (DEBUG) {
            if ((itsPC != -1) && (itsPC != thePC))
                throw new RuntimeException("Duplicate label");
        }
        itsPC = thePC;
    }

    public void addFixup(int fixupSite)
    {
        if (itsFixupTable == null) {
            itsFixupTableTop = 1;
            itsFixupTable = new int[FIXUPTABLE_SIZE];
            itsFixupTable[0] = fixupSite;            
        }
        else {
            if (itsFixupTableTop == itsFixupTable.length) {
                int oldLength = itsFixupTable.length;
                int newTable[] = new int[oldLength + FIXUPTABLE_SIZE];
                System.arraycopy(itsFixupTable, 0, newTable, 0, oldLength);
                itsFixupTable = newTable;
            }
            itsFixupTable[itsFixupTableTop++] = fixupSite;            
        }
    }

    private short itsPC;
    private int itsFixupTable[];
    private int itsFixupTableTop;

}

