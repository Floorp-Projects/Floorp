/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 * Larry Fitzpatrick, OpenText, lef@opentext.com
 *    --
 *
 * Eric Du, duxy@leyou.com.cn
 *   -- added fix for FreeBSD
 *
 * NaN/Infinity code copied from the JS-library with permission from
 * Netscape Communications Corporation: http://www.mozilla.org/js
 * http://lxr.mozilla.org/seamonkey/source/js/src/jsnum.h
 *
 */


#ifndef MITRE_PRIMITIVES_H
#define MITRE_PRIMITIVES_H

#include "baseutils.h"
#include "TxString.h"

/*
 * Utility class for doubles
 */
class Double {
public:

    /*
     * Usefull constants
     */

    static const double NaN;
    static const double POSITIVE_INFINITY;
    static const double NEGATIVE_INFINITY;

    /*
     * Determines whether the given double represents positive or negative
     * inifinity
     */
    static MBool isInfinite(double aDbl);

    /*
     * Determines whether the given double is NaN
     */
    static MBool isNaN(double aDbl);

    /*
     * Determines whether the given double is negative
     */
    static MBool isNeg(double aDbl);

    /*
     * Converts the value of the given double to a String, and appends
     * the result to the destination String.
     * @return the given dest string
     */
    static String& toString(double aValue, String& aDest);

    /*
     * Converts the given String to a double, if the String value does not
     * represent a double, NaN will be returned
     */
    static double toDouble(const String& aStr);
};

#endif
