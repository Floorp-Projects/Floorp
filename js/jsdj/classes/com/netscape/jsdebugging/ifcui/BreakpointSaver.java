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
