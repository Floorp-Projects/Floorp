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
 * $Id: StringComparator.cpp,v 1.1 2000/04/12 10:49:38 kvisco%ziplink.net Exp $
 */

#include "StringComparator.h"


// const declarations
const String StringComparator::EN_LANG = "en";

/**
 * Returns an instance of the StringComparator which handles the given Language
 * <BR>
 * Note: Remember to destroy instance when done.
**/
StringComparator* StringComparator::getInstance(const String& lang) {

    if (EN_LANG.isEqual(lang)) return new DefaultStringComparator();
    /*
    else if(....) return new ....StringComparator();
    */
    //-- use default
    return new DefaultStringComparator();
} //-- getInstance









