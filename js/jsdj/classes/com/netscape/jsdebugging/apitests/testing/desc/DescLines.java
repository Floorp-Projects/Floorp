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

package com.netscape.jsdebugging.apitests.testing.desc;

import com.netscape.jsdebugging.apitests.xml.*;
import com.netscape.jsdebugging.api.*;

/**
 * Describes all the lines of the script by calling DescLine on each line
 *
 * @author Alex Rakhlin
 */
public class DescLines extends Descriptor {

    public DescLines (XMLWriter xmlw, DescriptorManager dmgr){
        super (Tags.lines_tag, xmlw, dmgr);
    }


    public void description (Object o){
        Script script = (Script) o;

        int base = script.getBaseLineNumber();
        int extent = script.getLineExtent();

        //later also test other locations
        for (int i = base; i<base+extent; i++){
            ((DescLine)getDmgr().getDescriptor (DescLine.class)).describe (script, i);
        }
    }

}
