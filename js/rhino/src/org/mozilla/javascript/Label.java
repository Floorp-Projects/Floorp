/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
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

