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
