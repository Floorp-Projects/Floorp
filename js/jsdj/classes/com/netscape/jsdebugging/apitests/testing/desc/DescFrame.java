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
 * Describes a JSStackFrameInfo object.
 *
 * @author Alex Rakhlin
 */

public class DescFrame extends Descriptor {

    public DescFrame (XMLWriter xmlw, DescriptorManager dmgr){
        super (Tags.frame_tag, xmlw, dmgr);
    }
    
    public void description (Object o){
        JSStackFrameInfo frame = (JSStackFrameInfo) o;
        
        try {
            JSPC pc = (JSPC) frame.getPC();
            ((DescPC)getDmgr().getDescriptor (DescPC.class)).describe (pc);
            _xmlw.tag (Tags.is_valid_tag, frame.isValid());
        } catch (InvalidInfoException e) { System.out.println ("Error"); }
    }
    
}
