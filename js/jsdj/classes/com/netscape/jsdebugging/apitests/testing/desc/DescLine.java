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
 * Describes a line. As a part of the description, outputs a closest PC for this line.
 *
 * @author Alex Rakhlin
 */
public class DescLine extends Descriptor {

    public DescLine (XMLWriter xmlw, DescriptorManager dmgr){
        super (Tags.line_tag, xmlw, dmgr);
    }


    public void description (Object o, int i){
        Script script = (Script) o;
        int lineno = i;

        _xmlw.tag (Tags.lineno_tag, lineno);

        _xmlw.startTag(Tags.closest_pc_tag);
        JSPC pc = script.getClosestPC (lineno);
        ((DescPC)getDmgr().getDescriptor (DescPC.class)).describe (pc);
        _xmlw.endTag(Tags.closest_pc_tag);
    }

}
