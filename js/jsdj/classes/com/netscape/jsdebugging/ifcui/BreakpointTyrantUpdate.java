/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/*
* Information to describe a breakpoint update event
*/

// when     who     what
// 06/27/97 jband   added this header to my code
//

package com.netscape.jsdebugging.ifcui;

import com.netscape.jsdebugging.api.*;

class BreakpointTyrantUpdate
{
    public static final int ADD_ONE             = 0;
    public static final int REMOVE_ONE          = 1;
    public static final int REMOVE_ALL          = 2;
    public static final int REMOVE_ALL_FOR_URL  = 3;
    public static final int REFRESH_ALL         = 4;
    public static final int ACTIVATED_ONE       = 5; // these not always called
    public static final int DEACTIVATED_ONE     = 6; // these not always called
    public static final int MODIFIED_ONE        = 7;

    public BreakpointTyrantUpdate( int type, Breakpoint bp )
    {
        this.type = type;
        this.bp   = bp;
    }

    public int          type;
    public Breakpoint   bp;
}

