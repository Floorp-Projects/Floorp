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
