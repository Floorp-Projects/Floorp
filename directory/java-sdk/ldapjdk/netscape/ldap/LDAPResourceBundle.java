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
 *   ErrorCodes_en_us.props
 *   ErrorCodes_en.props
 *   ErrorCodes.props
 *
 * </pre>
 * @see java.util.Locale
 */
class LDAPResourceBundle {

    private static final boolean m_debug = false;
    private static final String m_suffix = ".props";
    private static final String m_locale_separator = "_";

    /**
     * Return the property resource bundle according to the base name of the
     * property file and the locale. The class loader will find the closest match
     * with the given locale.
     * @return The property resource bundle
     * @exception IOException Gets thrown when failed to open the resource
     *            bundle file.
     */
    static PropertyResourceBundle getBundle(String baseName)
      throws IOException {

        return getBundle(baseName, Locale.getDefault());
    }

    /**
     * Return the property resource bundle according to the base name of the
     * property file and the locale. The class loader will find the closest match
     * with the given locale.
     * @param baseName The base name of the property file, ie, the name contains
     *        no locale context and no . suffix
     * @param l The locale
     * @return The property resource bundle
     * @exception IOException Gets thrown when failed to create a property
     *            resource
     */
    static PropertyResourceBundle getBundle(String baseName, Locale l)
      throws IOException {
        String localeStr = m_locale_separator+l.toString();

        InputStream fin = null;

        while (true) {
            if ((fin=getStream(baseName, localeStr)) != null) {
                PropertyResourceBundle p = new PropertyResourceBundle(fin);
                return p;
            } else {

                int index = localeStr.lastIndexOf(m_locale_separator);
                if (index == -1) {
                    printDebug("File "+baseName+localeStr+m_suffix+" not found");
                    return null;
                } else
                    localeStr = localeStr.substring(0, index);
            }
        }
    }

    /**
     * Constructs the whole absolute path name of a property file and retrieves
     * an input stream on the file.
     * @param baseName The base name of the property file, ie, the name contains
     *        no locale context and no . suffix
     * @param The locale string being inserted within the file name.
     * @return The input stream on the property file.
     */
    private static InputStream getStream(String baseName, String locale) {
        String fStr = baseName+locale+m_suffix;
        return (ClassLoader.getSystemResourceAsStream(fStr));
    }

    /**
     * Prints debug messages if the debug mode is on.
     * @param str The given message being printed.
     */
    private static void printDebug(String str) {
        if (m_debug)
            System.out.println(str);
    }
}

