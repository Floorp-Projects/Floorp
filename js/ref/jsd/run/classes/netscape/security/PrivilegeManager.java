//
// jband -- stubbed out version...
//
// PrivilegeManager.java -- Copyright 1996, Netscape Communications Corp.
// Dan Wallach and Raman Tenneti
//

package netscape.security;

/**
* <font color="red">Non-functional stubbed out version</font>
* <p>
* <B>This supports only the entry points used by the JSD system</B>
* @author  John Bandhauer
*/
public final
class PrivilegeManager {

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
    public static void enablePrivilege(String targetStr) {}
}
