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

public class LocalVariable {
    
    public LocalVariable(String name, boolean isParameter) {
        itsName = name;
        itsIsParameter = isParameter; 
    }
    
    public void setIndex(int index){ itsIndex = index; }
    public int getIndex()          { return itsIndex; }
        
    public void setIsParameter()   { itsIsParameter = true; }
    public boolean isParameter()   { return itsIsParameter; }
            
    public String getName()        { return itsName; }
        
    /**
     * Return the starting PC where this variable is live, or -1
     * if it is not a Java register.
     */
    public int getStartPC() {
        return -1;
    }

    /**
     * Return the Java register number or -1 if it is not a Java register.
     */
    public short getJRegister() {
        return -1;
    }

    /**
     * Return true if the local variable is a Java register with double type.
     */
    public boolean isNumber() {
        return false;
    }

    private String itsName;
    private int itsIndex = -1;
    
    private boolean itsIsParameter;
}
