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
* Windows style Cut/Copy/Paste hotkey support
*/

// when     who     what
// 11/19/97 jband   added this class
//

package com.netscape.jsdebugging.ifcui;

import netscape.application.*;
import netscape.util.*;

public class EditKeyTextFilter
    implements TextFilter
{
    public EditKeyTextFilter(CommandTyrant commandTyrant)
    {
        _commandTyrant = commandTyrant;
    }

    // implement TextFilter interface
    public boolean acceptsEvent(Object o, KeyEvent ke , Vector vec)
    {
        if( ke.isControlKeyDown() )
        {
            switch(ke.key + 'a' - 1)
            {
            case 'c':
                Application.application().performCommandLater(
                    _commandTyrant,
                    _commandTyrant.cmdString(CommandTyrant.COPY),
                    null);
                return false;
            case 'v':
                Application.application().performCommandLater(
                    _commandTyrant,
                    _commandTyrant.cmdString(CommandTyrant.PASTE),
                    null);
                return false;
            case 'x':
                Application.application().performCommandLater(
                    _commandTyrant,
                    _commandTyrant.cmdString(CommandTyrant.CUT),
                    null);
                return false;
            default:
                break;
            }
        }
        return true;
    }

    private CommandTyrant       _commandTyrant;
}
