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

public class LabelTable {

    private static final boolean DEBUGLABELS = false;
    
    private static final int LabelTableSize = 32;
    protected Label itsLabelTable[];
    protected int itsLabelTableTop;

    public int acquireLabel()
    {
        if (itsLabelTable == null) {
            itsLabelTable = new Label[LabelTableSize];
            itsLabelTable[0] = new Label();
            itsLabelTableTop = 1;
            return 0x80000000;
        }
        else {
            if (itsLabelTableTop == itsLabelTable.length) {
                Label oldTable[] = itsLabelTable;
                itsLabelTable = new Label[itsLabelTableTop * 2];
                System.arraycopy(oldTable, 0, itsLabelTable, 0, itsLabelTableTop);
            }
            itsLabelTable[itsLabelTableTop] = new Label();
            int result = itsLabelTableTop++;
            return result | 0x80000000;
        }
    }

    public int markLabel(int theLabel, int pc)
    {
        if (DEBUGLABELS) {
            if ((theLabel & 0x80000000) != 0x80000000)
                throw new RuntimeException("Bad label, no biscuit");
        }
        theLabel &= 0x7FFFFFFF;
        if (DEBUGLABELS) {
            System.out.println("Marking label " + theLabel + " at " + pc);
        }
        itsLabelTable[theLabel].setPC((short)pc);
        return theLabel | 0x80000000;
    }

}
