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
 * Describes the Stack (JSStackFrameInfo[])
 *
 * @author Alex Rakhlin
 */

public class DescStack extends Descriptor {

    public DescStack (XMLWriter xmlw, DescriptorManager dmgr){
        super (Tags.stack_tag, xmlw, dmgr);
    }
    
    
    public void description (Object o){
        ThreadStateBase debug = (ThreadStateBase) o;
        
        try {
            _xmlw.tag (Tags.count_tag, debug.countStackFrames());
        } catch (InvalidInfoException e) {
            _xmlw.tag (Tags.count_tag, -1);
        }
        _xmlw.tag (Tags.thread_state_tag, debug.getStatus());
        try {
            JSStackFrameInfo[] stack = (JSStackFrameInfo[])debug.getStack();
            int num = stack.length;
            for (int i = 0; i<num; i++){
                ((DescFrame)getDmgr().getDescriptor (DescFrame.class)).describe (stack[i]);
            }
        } catch (InvalidInfoException e) {
            System.out.println ("InvalidInfoException");
        }
    }    
    
}
