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
import java.util.*;

/**
 * Describes eval info.
 *
 * @author Alex Rakhlin
 */

public class DescEval extends DescriptorSerializable {

    public DescEval (XMLWriter xmlw, DescriptorManager dmgr){
        super (Tags.eval_tag, xmlw, dmgr);
    }

    /**
     * Elements should be packed in the vector obj in the specific order.
     */
    public void description (Object obj){
        Vector v = (Vector) obj;
        /* unpack the vector */
        JSStackFrameInfo frame = (JSStackFrameInfo) v.elementAt (0);
        ((DescFrame)getDmgr().getDescriptor (DescFrame.class)).describe (frame);
        int i = 1;
        while (i < v.size()-1){
            /* v(i) should have a string of what was evaled, and v(i+1) should be the result */
            String result_string = "none";
            String eval_string = (String) v.elementAt (i);
            
            if (v.elementAt(i+1) != null) result_string = (String) v.elementAt (i+1);
            
            _xmlw.startTag (Tags.results_tag);
            _xmlw.tag (Tags.eval_string_tag, eval_string);
            _xmlw.tag (Tags.result_tag, result_string);
            _xmlw.endTag (Tags.results_tag);
            i = i + 2;
        }
    }

}