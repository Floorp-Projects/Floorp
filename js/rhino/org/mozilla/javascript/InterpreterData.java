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
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Norris Boyd
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

import java.util.Vector;

class InterpreterData {
    
    static final int INITIAL_MAX_ICODE_LENGTH = 1024;
    static final int INITIAL_STRINGTABLE_SIZE = 64;
    static final int INITIAL_NUMBERTABLE_SIZE = 64;
    
    InterpreterData(int lastICodeTop, int lastStringTableIndex, 
                    int lastNumberTableIndex, Object securityDomain,
                    boolean useDynamicScope)
    {
        itsICodeTop = lastICodeTop == 0 
                      ? INITIAL_MAX_ICODE_LENGTH
                      : lastICodeTop * 2;
        itsICode = new byte[itsICodeTop];

        itsStringTable = new String[lastStringTableIndex == 0
                                    ? INITIAL_STRINGTABLE_SIZE
                                    : lastStringTableIndex * 2];

        itsNumberTable = new Number[lastNumberTableIndex == 0
                                    ? INITIAL_NUMBERTABLE_SIZE
                                    : lastNumberTableIndex * 2];
        
        itsUseDynamicScope = useDynamicScope;
        if (securityDomain == null)
            Context.checkSecurityDomainRequired();
        this.securityDomain = securityDomain;
    }
    
    public boolean placeBreakpoint(int line) { // XXX throw exn?
        int offset = getOffset(line);
        if (offset != -1 && (itsICode[offset] == (byte)TokenStream.LINE ||
                             itsICode[offset] == (byte)TokenStream.BREAKPOINT))
        {
            itsICode[offset] = (byte) TokenStream.BREAKPOINT;
            return true;
        }
        return false;
    }
    
    public boolean removeBreakpoint(int line) {
        int offset = getOffset(line);
        if (offset != -1 && itsICode[offset] == (byte) TokenStream.BREAKPOINT)
        {
            itsICode[offset] = (byte) TokenStream.LINE;
            return true;
        }
        return false;
    }
    
    private int getOffset(int line) {
        Object offset = itsLineNumberTable.get(new Integer(line));
        if (offset != null && offset instanceof Integer) {
            int i = ((Integer)offset).intValue();
            if (i >= 0 && i < itsICode.length)
            {
                return i;
            }
        }
        return -1;
    }    
    
    VariableTable itsVariableTable;
    
    String itsName;
    String itsSource;
    String itsSourceFile;
    boolean itsNeedsActivation;
    boolean itsFromEvalCode;
    boolean itsUseDynamicScope;
    byte itsFunctionType;

    String[] itsStringTable;
    int itsStringTableIndex;

    Number[] itsNumberTable;
    int itsNumberTableIndex;
    
    InterpretedFunction[] itsNestedFunctions;
    
    Object[] itsRegExpLiterals;

    byte[] itsICode;
    int itsICodeTop;
    
    int itsMaxLocals;
    int itsMaxArgs;
    int itsMaxStack;
    int itsMaxTryDepth;
    
    java.util.Hashtable itsLineNumberTable;

    Object securityDomain;
}
