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
* Persistence support for source viewer options
*/

// when     who     what
// 08/01/97 jband   created this file
//

package com.netscape.jsdebugging.ifcui;

import netscape.application.*;
import netscape.util.*;

public class SourceViewSaver
    implements Codable
{
    // public ctor with no params necessary for Codable
    public SourceViewSaver() {}

    // implement Codable
    public void describeClassInfo(ClassInfo info)
    {
        info.addField("_showLineNumbers", BOOLEAN_TYPE);
    }
    public void encode(Encoder encoder) throws CodingException
    {
        encoder.encodeBoolean("_showLineNumbers", _showLineNumbers );
    }
    public void decode(Decoder decoder) throws CodingException
    {
        _showLineNumbers = decoder.decodeBoolean("_showLineNumbers");
    }
    public void finishDecoding() throws CodingException
    {
    }

    // interaction with 'owner'

    public void sendToManager( SourceViewManager svm )
    {
        svm.setShowLineNumbers(_showLineNumbers);
    }

    public void getFromManager( SourceViewManager svm )
    {
        _showLineNumbers = svm.getShowLineNumbers();
    }

    private boolean         _showLineNumbers;
}
