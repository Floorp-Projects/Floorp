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
 * Describes a Script object.
 *
 * @author Alex Rakhlin
 */

public class DescScript extends DescriptorSerializable {

    public DescScript (XMLWriter xmlw, DescriptorManager dmgr){
        super (Tags.script_tag, xmlw, dmgr);
    }


    public void description (Object obj){
        Script script = (Script) obj;
        
        String URL = script.getURL();
        String func = script.getFunction();
        if (URL == null) URL = "none";
        if (func == null) func = "none";

        _xmlw.tag (Tags.url_tag, URL);
        _xmlw.tag (Tags.function_tag, func);
        _xmlw.tag (Tags.baselineno_tag, script.getBaseLineNumber());
        _xmlw.tag (Tags.line_extent_tag, script.getLineExtent());
        _xmlw.tag (Tags.is_valid_tag, script.isValid());
        ((DescSections)getDmgr().getDescriptor (DescSections.class)).describe (script);
        ((DescLines)getDmgr().getDescriptor (DescLines.class)).describe (script);
    }
    
    

}
