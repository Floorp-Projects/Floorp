/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
