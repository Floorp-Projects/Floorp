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
* Persistence support for breakpoints
*/

// when     who     what
// 07/31/97 jband   created this file
//

package com.netscape.jsdebugging.ifcui;

import netscape.application.*;
import netscape.util.*;

public class BreakpointSaver
    implements Codable
{
    // public ctor with no params necessary for Codable
    public BreakpointSaver() {}

    // implement Codable
    public void describeClassInfo(ClassInfo info)
    {
        info.addField("_bps", OBJECT_ARRAY_TYPE);
    }
    public void encode(Encoder encoder) throws CodingException
    {
        if( null != _bps && 0 != _bps.length )
            encoder.encodeObjectArray("_bps", _bps, 0, _bps.length);
    }
    public void decode(Decoder decoder) throws CodingException
    {
        _bps = decoder.decodeObjectArray("_bps");
    }
    public void finishDecoding() throws CodingException
    {
    }

    // interaction with BreakpointTyrant

    public void sendToTyrant( BreakpointTyrant bpt )
    {
        if( null != _bps && 0 != _bps.length )
        {
            for(int i = 0; i < _bps.length; i++ )
            {
                Breakpoint bp = (Breakpoint) _bps[i];

                if( null == bpt.findBreakpoint(bp) )
                {
                    String condition = bp.getBreakCondition();
                    bp = bpt.addBreakpoint(bp.getLocation());
                    if( null != bp && null != condition )
                        bp.setBreakCondition(condition);
                }
            }
        }
    }

    public void getFromTyrant( BreakpointTyrant bpt )
    {
        _bps = bpt.getAllBreakpoints();
    }

    private Object[] _bps;
}
