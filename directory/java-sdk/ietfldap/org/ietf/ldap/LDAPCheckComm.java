/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.ietf.ldap;

/**
 * This static class checks if the caller is an applet running in
 * Netscape Communicator. If so, it returns the appropriate method.
 */
class LDAPCheckComm {

    /**
     * Returns the method whose name matches the specified argument.
     * @param classPackage the class package
     * @param name the method name
     * @return the method.
     * @exception LDAPException Gets thrown if the method is not found or
     *            the caller is not an applet running in Netscape
     *            Communicator.
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

