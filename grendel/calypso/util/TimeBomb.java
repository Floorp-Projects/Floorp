/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil -*-
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
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

// original timebomb code to search for.

    // This will expire on April 1st at 12pm local time
    //java.util.Date before = new Date(97, 9, 1, 0, 0);
    //java.util.Date now = new Date();
    //java.util.Date then = new Date(97, 11, 25, 12, 00);
    //if (now.before(before) || now.after(then)) {
    //  System.err.println("This software has expired");
    //  System.exit(-1);
    //}

public class TimeBomb
{
    public static final boolean Ticking = true;

    // Before is 12-01-97

    public static final int BeforeYear  = 97;
    public static final int BeforeMonth = 11; // Zero based
    public static final int BeforeDate  = 1;

    // Expire is 4-01-98

    public static final int ExpiresYear  = 98;
    public static final int ExpiresMonth = 3; // Zero based
    public static final int ExpiresDate  = 1;
    public static final int ExpiresHour  = 12;
}
