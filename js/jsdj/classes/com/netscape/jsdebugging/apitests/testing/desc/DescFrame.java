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
