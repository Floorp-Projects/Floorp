/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
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
 * Roger Lawrence
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

public class LabelTable {

    public int acquireLabel()
    {
        int top;
        if (itsLabelTable == null) {
            top = 0;
            itsLabelTable = new short[MIN_LABEL_TABLE_SIZE];
        }else {
            top = itsLabelTableTop;
            if (top == itsLabelTable.length) {
                short[] tmp = new short[top * 2];
                System.arraycopy(itsLabelTable, 0, tmp, 0, top);
                itsLabelTable = tmp;
            }
        }
        itsLabelTableTop = top + 1;
        itsLabelTable[top] = (short)-1;
        return top | 0x80000000;
    }

    public short getLabelPC(int theLabel) {
        return itsLabelTable[theLabel];
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
        if (itsLabelTable[theLabel] != -1) {
            // Can mark label only once
            throw new RuntimeException();
        }
        itsLabelTable[theLabel] = (short)pc;
        return theLabel | 0x80000000;
    }

    public void addLabelFixup(int theLabel, int fixupSite) {
        int top;
        if (itsFixupTable == null) {
            top = 0;
            itsFixupTable = new long[MIN_FIXUP_TABLE_SIZE];
        }else {
            top = itsFixupTableTop;
            if (top == itsFixupTable.length) {
                long[] tmp = new long[top * 2];
                System.arraycopy(itsFixupTable, 0, tmp, 0, top);
                itsFixupTable = tmp;
            }
        }
        itsFixupTableTop = top + 1;
        itsFixupTable[top] = ((long)theLabel << 32) | fixupSite;
    }

    public void fixLabelGotos(byte[] codeBuffer) {
        for (int i = 0; i < itsFixupTableTop; i++) {
            long fixup = itsFixupTable[i];
            int label = (int)(fixup >> 32);
            int fixupSite = (int)fixup;
            short pc = itsLabelTable[label];
            if (pc == -1) {
                // Unlocated label
                throw new RuntimeException();
            }
            // -1 to get delta from instruction start
            short offset = (short)(pc - (fixupSite - 1));
            codeBuffer[fixupSite] = (byte)(offset >> 8);
            codeBuffer[fixupSite + 1] = (byte)offset;
        }
        itsFixupTableTop = 0;
    }

    public void clearLabels() {
        itsLabelTableTop = 0;
        itsFixupTableTop = 0;
    }

    private static final boolean DEBUGLABELS = false;

    private static final int MIN_LABEL_TABLE_SIZE = 32;
    private short[] itsLabelTable;
    private int itsLabelTableTop;

// itsFixupTable[i] = (label_index << 32) | fixup_site
    private static final int MIN_FIXUP_TABLE_SIZE = 40;
    private long[] itsFixupTable;
    private int itsFixupTableTop;

}
