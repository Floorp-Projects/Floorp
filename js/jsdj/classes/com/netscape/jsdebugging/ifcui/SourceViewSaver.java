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
