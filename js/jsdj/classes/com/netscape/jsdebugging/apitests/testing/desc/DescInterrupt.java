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
 * Describes interrupt info.
 *
 * @author Alex Rakhlin
 */

public class DescInterrupt extends Descriptor {

    public DescInterrupt (XMLWriter xmlw, DescriptorManager dmgr){
        super (Tags.interrupt_tag, xmlw, dmgr);
    }


    public void description (Object o){
        ThreadStateBase debug = (ThreadStateBase) o;
        PC pc = null;
        try {
            pc = debug.getCurrentFrame().getPC();
        } catch (InvalidInfoException e) { System.err.println ("error getting pc"); }
        ((DescPC) getDmgr().getDescriptor (DescPC.class)).describe (pc);
        //_xmlw.getCurrentTag().addChild (new DescThreadState (script, _xmlw.getCurrentTag(), _xmlw));
        ((DescStack) getDmgr().getDescriptor (DescStack.class)).describe (debug);
    }

}
