/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
package netscape.ldap;

/**
 * This static class checks if the caller is the applet running in the
 * communicator. If so, then it returns the appropriate method.
 */
class LDAPCheckComm {

    /**
     * It returns the method whose name matches the specified argument.
     * @param classPackage The class package
     * @param name The method name
     * @return The method
     * @exception LDAPException Gets thrown it the method is not found or
     *            the caller is not an applet running in the communicator
     */
    static java.lang.reflect.Method getMethod(String classPackage, String name) throws LDAPException {
      SecurityManager sec = System.getSecurityManager();

        if ( sec == null ) {
            /* Not an applet, we can do what we want to */
            return null;
        } else if ( sec.toString().startsWith("java.lang.NullSecurityManager") ) {
            /* Not an applet, we can do what we want to */
            return null;
        } else if (sec.toString().startsWith("netscape.security.AppletSecurity")) {
            /* Running as applet. Is PrivilegeManager around? */
            try {
                Class c = Class.forName(classPackage);
                java.lang.reflect.Method[] m = c.getMethods();
                for( int i = 0; i < m.length; i++ ) {
                    if ( m[i].getName().equals(name) ) {
                        return m[i];
                    }
                }
                throw new LDAPException("no enable privilege in " + classPackage);
            } catch (ClassNotFoundException e) {
                throw new LDAPException("Class not found");
            }
        }
        return null;
    }
}

