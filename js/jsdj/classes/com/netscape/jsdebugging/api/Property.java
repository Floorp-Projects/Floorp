/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package com.netscape.jsdebugging.api;

public interface Property
{
    // these must stay the same as the flags in netscape.jsdebug.Property.java

    public static int ENUMERATE  = 0x01;    /* visible to for/in loop */
    public static int READONLY   = 0x02;    /* assignment is error */
    public static int PERMANENT  = 0x04;    /* property cannot be deleted */
    public static int ALIAS      = 0x08;    /* property has an alias id */
    public static int ARGUMENT   = 0x10;    /* argument to function */
    public static int VARIABLE   = 0x20;    /* local variable in function */
    public static int HINTED     = 0x800;   /* found via explicit lookup */

    public boolean isNameString();
    public boolean isAliasString();

    public String getNameString();
    public int getNameInt();

    public String getAliasString();
    public int getAliasInt();

    public Value getValue();
    public int getVarArgSlot();

    public int     getFlags();
    public boolean canEnumerate();
    public boolean isReadOnly();
    public boolean isPermanent();
    public boolean hasAlias();
    public boolean isArgument();
    public boolean isVariable();
    public boolean isHinted();
}    
