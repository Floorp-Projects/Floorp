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
        int top = itsLabelTableTop;
        if (itsLabelTable == null || top == itsLabelTable.length) {
            if (itsLabelTable == null) {
                itsLabelTable = new int[MIN_LABEL_TABLE_SIZE];
            }else {
                int[] tmp = new int[itsLabelTable.length * 2];
                System.arraycopy(itsLabelTable, 0, tmp, 0, top);
                itsLabelTable = tmp;
            }
        }
        itsLabelTableTop = top + 1;
        itsLabelTable[top] = -1;
        return top;
    }

    public int getLabelPC(int label) {
        if (label > itsLabelTableTop) throw new RuntimeException();
        return itsLabelTable[label];
    }

    public void markLabel(int label, int pc) {
        if (label > itsLabelTableTop || pc < 0) throw new RuntimeException();
        if (itsLabelTable[label] != -1) {
            // Can mark label only once
            throw new RuntimeException();
        }
        itsLabelTable[label] = pc;
    }

    public void addLabelFixup(int label, int fixupSite) {
        if (label > itsLabelTableTop || fixupSite < 0) {
            throw new RuntimeException();
        }
        int top = itsFixupTableTop;
        if (itsFixupTable == null || top == itsFixupTable.length) {
            if (itsFixupTable == null) {
                itsFixupTable = new long[MIN_FIXUP_TABLE_SIZE];
            }else {
                long[] tmp = new long[itsFixupTable.length * 2];
                System.arraycopy(itsFixupTable, 0, tmp, 0, top);
                itsFixupTable = tmp;
            }
        }
        itsFixupTableTop = top + 1;
        itsFixupTable[top] = ((long)label << 32) | fixupSite;
    }

    public void fixLabelGotos(byte[] codeBuffer) {
        for (int i = 0; i < itsFixupTableTop; i++) {
            long fixup = itsFixupTable[i];
            int label = (int)(fixup >> 32);
            int fixupSite = (int)fixup;
            int pc = itsLabelTable[label];
            if (pc == -1) {
                // Unlocated label
                throw new RuntimeException();
            }
            // -1 to get delta from instruction start
            int offset = pc - (fixupSite - 1);
            if ((short)offset != offset) {
                throw new RuntimeException
                    ("Program too complex: too big jump offset");
            }
            codeBuffer[fixupSite] = (byte)(offset >> 8);
            codeBuffer[fixupSite + 1] = (byte)offset;
        }
        itsFixupTableTop = 0;
    }

    public void clearLabels() {
        itsLabelTableTop = 0;
        itsFixupTableTop = 0;
    }

    private static final int MIN_LABEL_TABLE_SIZE = 32;
    private int[] itsLabelTable;
    private int itsLabelTableTop;

// itsFixupTable[i] = (label_index << 32) | fixup_site
    private static final int MIN_FIXUP_TABLE_SIZE = 40;
    private long[] itsFixupTable;
    private int itsFixupTableTop;

}
