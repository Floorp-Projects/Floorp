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
* Info sent with SourceTyrant update events
*/

// when     who     what
// 06/27/97 jband   added this header to my code
//

package com.netscape.jsdebugging.ifcui;

import com.netscape.jsdebugging.api.*;

class SourceTyrantUpdate
{
    public static final int SELECT_CHANGED            = 0;
    public static final int VECTOR_CHANGED            = 1;
    public static final int ITEM_CHANGED              = 2;
    public static final int SEL_URL_LINE_TEXT_CHANGED = 3;
    public static final int ADJUSTMENTS_CHANGED       = 4;
    public SourceTyrantUpdate( int type, SourceTextItem item )
    {
        this.type = type;
        this.item = item;
    }
    public int              type;
    public SourceTextItem   item;
}


