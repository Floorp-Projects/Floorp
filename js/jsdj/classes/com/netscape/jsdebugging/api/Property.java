/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
