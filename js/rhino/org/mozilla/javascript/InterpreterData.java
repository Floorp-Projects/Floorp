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

import java.util.Vector;

class InterpreterData {
    
    static final int INITIAL_MAX_ICODE_LENGTH = 1024;
    static final int INITIAL_STRINGTABLE_SIZE = 64;
    static final int INITIAL_NUMBERTABLE_SIZE = 64;
    
    InterpreterData(int lastICodeTop, int lastStringTableIndex, 
                    int lastNumberTableIndex, Object securityDomain)
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
        
        if (securityDomain == null && Context.isSecurityDomainRequired())
            throw new SecurityException("Required security context missing");
        this.securityDomain = securityDomain;
    }
    
    VariableTable itsVariableTable;
    
    String itsName;
    String itsSource;
    boolean itsNeedsActivation;

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

    Object securityDomain;
    
    Context itsCX;
    Scriptable itsScope;
    Scriptable itsThisObj;
    Object[] itsInArgs;
}
