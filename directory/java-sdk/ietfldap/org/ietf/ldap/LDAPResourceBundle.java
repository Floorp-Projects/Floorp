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

import java.util.*;
import java.io.*;

/**
 * This class represents a locale-specific resource for a property file.
 * It retrieves the property file for the given base name including the
 * absolute path name and locale. The property file has to be located in the
 * CLASSPATH and the property file's suffix is .props.
 * <p>
 * If the specified locale is en and us and the base name of the file is
 * netscape/ldap/errors/ErrorCodes, then the class loader will search for
 * the file in the following order:
 * <pre>
 *
 *   ErrorCodes_en_us.properties
 *   ErrorCodes_en.properties
 *   ErrorCodes.properties
 *
 * </pre>
 * @see java.util.Locale
 */
class LDAPResourceBundle implements java.io.Serializable {

    static final long serialVersionUID = -5903986665461157980L;
    private static final boolean _debug = false;
    private static final String _suffix = ".properties";
    private static final String _locale_separator = "_";

    /**
     * Return the property resource bundle according to the base name of the
     * property file and the locale. The class loader will find the closest match
     * with the given locale.
     * @return the property resource bundle.
     * @exception IOException Gets thrown when failed to open the resource
     *            bundle file.
     */
    static PropertyResourceBundle getBundle( String baseName )
      throws IOException {

        return getBundle( baseName, Locale.getDefault() );
    }

    /**
     * Return the property resource bundle according to the base name of the
     * property file and the locale. The class loader will find the closest match
     * with the given locale.
     * @param baseName the base name of the property file. The base name contains
     * no locale context and no . suffix.
     * @param l the locale
     * @return the property resource bundle.
     * @exception IOException Gets thrown when failed to create a property
     *            resource
     */
    static PropertyResourceBundle getBundle( String baseName, Locale l )
      throws IOException {
        String localeStr = _locale_separator + l.toString();

        InputStream fin = null;

        while ( true ) {
            if ( (fin = getStream(baseName, localeStr)) != null ) {
                PropertyResourceBundle p = new PropertyResourceBundle( fin );
                return p;
            } else {
                int index = localeStr.lastIndexOf( _locale_separator );
                if ( index == -1 ) {
                    printDebug( "File " + baseName + localeStr + _suffix +
                                " not found" );
                    return null;
                } else {
                    localeStr = localeStr.substring( 0, index );
                }
            }
        }
    }

    /**
     * Constructs the whole absolute path name of a property file and retrieves
     * an input stream on the file.
     * @param baseName the base name of the property file. The base name contains
     * no locale context and no . suffix.
     * @param the locale string to insert into the file name
     * @return the input stream of the property file.
     */
    private static InputStream getStream( String baseName, String locale ) {
        String fStr = baseName + locale + _suffix;
        return ClassLoader.getSystemResourceAsStream( fStr );
    }

    /**
     * Prints debug messages if the debug mode is on.
     * @param str the message that is printed
     */
    private static void printDebug(String str) {
        if ( _debug ) {
            System.out.println( str );
        }
    }
}

