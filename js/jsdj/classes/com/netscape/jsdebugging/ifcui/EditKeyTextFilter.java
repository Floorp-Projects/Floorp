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
