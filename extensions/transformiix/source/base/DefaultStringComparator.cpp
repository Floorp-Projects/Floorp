/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is XSL:P XSLT processor.
 *
 * The Initial Developer of the Original Code is Keith Visco.
 * Portions created by Keith Visco (C) 1999-2000 Keith Visco.
 * All Rights Reserved..
 *
 * Contributor(s):
 *
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 * $Id: DefaultStringComparator.cpp,v 1.1 2000/04/12 10:49:27 kvisco%ziplink.net Exp $
 */

#include "StringComparator.h"

/**
 * Creates a new DefaultStringComparator
**/
DefaultStringComparator::DefaultStringComparator() {
    //-- do nothing for now
}

/**
 * Destroys this DefaultStringComparator
**/
DefaultStringComparator::~DefaultStringComparator() {
    //-- do nothing for now
}

/**
 * Compares the given Strings. -1 is returned if str1 is less than str2,
 * 0 is returned if the two Strings are equal, and 1  is return if str1 is
 * greater than str2.
**/
int DefaultStringComparator::compare(const String& str1, const String& str2) {

    int len1 = str1.length();
    int len2 = str2.length();

    int c = 0;
    while ((c < len1) && (c < len2)) {
        Int32 ch1 = str1.charAt(c);
        Int32 ch2 = str2.charAt(c);
        if (ch1 < ch2) return -1;
        else if (ch2 < ch1) return 1;
        ++c;
    }

    if (len1 == len2) return 0;
    else if (len1 < len2) return -1;
    else return 1;

} //-- compare


