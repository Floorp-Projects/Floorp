/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*
* 'Model' to hold info on one line of text in source viewer
*/

// when     who     what
// 06/27/97 jband   added this header to my code
//

package com.netscape.jsdebugging.ifcui;

import com.netscape.jsdebugging.api.*;

class SourceLineItemModel
{
    // possible values for type
    public static final int NO_SCRIPT        = 0;
    public static final int TOP_LEVEL_SCRIPT = 1;
    public static final int FUNCTION_BODY    = 2;

    public int          lineNumber;    // zero based
    public String       text;
    public int          type;
    public Breakpoint   bp;
    public boolean      executing;  // is this line currently executing?
    public String       adjustmentChar;
}

