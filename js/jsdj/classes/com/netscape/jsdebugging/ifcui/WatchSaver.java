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
* Persistence support for data Watches
*/

// when     who     what
// 09/25/97 jband   created this file
//

package com.netscape.jsdebugging.ifcui;

import netscape.application.*;
import netscape.util.*;

public class WatchSaver
    implements Codable
{
    // public ctor with no params necessary for Codable
    public WatchSaver() {}

    // implement Codable
    public void describeClassInfo(ClassInfo info)
    {
        info.addField("_watches", OBJECT_ARRAY_TYPE);
    }
    public void encode(Encoder encoder) throws CodingException
    {
        if( null != _watches && 0 != _watches.length )
            encoder.encodeObjectArray("_watches", _watches, 0, _watches.length);
    }
    public void decode(Decoder decoder) throws CodingException
    {
        _watches = decoder.decodeObjectArray("_watches");
    }
    public void finishDecoding() throws CodingException
    {
    }

    // interaction with WatchTyrant

    public void sendToTyrant( WatchTyrant wt )
    {
        if( null == _watches || 0 == _watches.length )
            return;
        for(int i = 0; i < _watches.length; i++ )
            wt.addWatchString((String)_watches[i]);
    }

    public void getFromTyrant( WatchTyrant wt )
    {
        _watches = null;
        Vector v = wt.getWatchList();
        if( null == v || 0 == v.size() )
            return;
        _watches = new Object[v.size()];
        for( int i = 0; i < v.size(); i++ )
            _watches[i] = v.elementAt(i);
    }

    private Object[] _watches;
}

