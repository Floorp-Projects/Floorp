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

/* jband Sept 1998 - **NOT original file** - this version is stubbed out */

package netscape.security;

/**
* <font color="red">Non-functional stubbed out version</font>
* <p>
* <B>This supports only the entry points used by the JSD system</B>
* @author  John Bandhauer
*/
public
class PrivilegeManager {

    /*
    * This is a hack to fool the symantec optimizer. When compiling this 
    * stubbed out version of this class the compiler was determining that 
    * the calls to enablePrivilege() had no real effect and was 
    * optimizing them away. However, when the application was running in 
    * Navigator (with the REAL version of this class) security checks were 
    * failing because the enablePrivilege() call was not calling the real 
    * thing
    */
    private static int _bogus;

    /**
     * check to see if somebody has enabled their privileges to use this target
     * @param targetStr A Target name whose access is being checked
     * @return nothing
     * @exception netscape.security.ForbiddenTargetException thrown if
     * access is denied to the Target.
     */
    public static void checkPrivilegeEnabled(String targetStr) {}

    /**
     * This call enables privileges to the given target until the
     * calling method exits.  As a side effect, if your principal
     * does not have privileges for the target, the user may be
     * consulted to grant these privileges.  
     * 
     * @param targetStr the name of the Target whose access is being checked
     * @return nothing
     * @exception netscape.security.ForbiddenTargetException thrown if
     * access is denied to the Target.
     */
    public static void enablePrivilege(String targetStr) {_bogus++;}
}
