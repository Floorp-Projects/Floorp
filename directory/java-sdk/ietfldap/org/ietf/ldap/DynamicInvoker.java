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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.ietf.ldap;

import java.lang.reflect.*;
import java.util.Hashtable;

/**
 * Utility class to dynamically find methods of a class and to invoke
 * them
 */
class DynamicInvoker {
    static Object invokeMethod( Object obj,
                                String packageName,
                                String methodName,
                                Object[] args,
                                String[] argNames )
        throws LDAPException {
        try {
            java.lang.reflect.Method m = getMethod( packageName,
                                                    methodName,
                                                    argNames );
            if ( m != null ) {
                return m.invoke( obj, args );
            } else {
                System.err.println( "No such method: " +
                                    packageName + "." + methodName );
            }
        } catch ( Exception e ) {
            e.printStackTrace();
            throw new LDAPException( "Invoking " + methodName + ": " +
                                     e.toString(),
                                     LDAPException.PARAM_ERROR );
        }

        return null;
    }

    static Method getMethod( String packageName,
                             String methodName,
                             String[] args ) throws LDAPException {
        try {
            Method method = null;
            String suffix = "";
            if ( args != null ) {
                for ( int i = 0; i < args.length; i++ ) {
                    suffix = suffix + args[i].getClass().getName();
                }
            }
            String key = packageName + "." + methodName + "." + suffix;
            if ( (method = (Method)(m_methodLookup.get(key))) != null ) {
                return method;
            }

            Class c = Class.forName( packageName );
            Method[] m = c.getMethods();
            for ( int i = 0; i < m.length; i++ ) {
                Class[] params = m[i].getParameterTypes();
                if ( m[i].getName().equals(methodName) &&
                     signatureCorrect( params, args ) ) {
                    m_methodLookup.put( key, m[i] );
                    return m[i];
                }
            }
            throw new LDAPException( "Method " +
                                     methodName +
                                     " not found in " +
                                     packageName );
        } catch ( ClassNotFoundException e ) {
            throw new LDAPException( "Class " + packageName + " not found" );
        }
    }

    static private boolean signatureCorrect( Class params[],
                                             String args[] ) {
        if ( args == null ) {
            return true;
        }
        if ( params.length != args.length ) {
            System.err.println( params.length + " method parameters vs " +
                                args.length + " expected" );
            return false;
        }
        for ( int i = 0; i < params.length; i++ ) {
            if ( !params[i].getName().equals(args[i]) ) {
                System.err.println( params[i].getName() +
                                    " method parameter vs " +
                                    args[i] + " expected" );
                return false;
            }
        }
        return true;
    }

    private static Hashtable m_methodLookup = new Hashtable();
}
