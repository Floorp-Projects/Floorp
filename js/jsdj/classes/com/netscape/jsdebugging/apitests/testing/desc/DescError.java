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
import java.util.*;

/**
 * Describes error info.
 *
 * @author Alex Rakhlin
 */

public class DescError extends DescriptorSerializable {

    public DescError (XMLWriter xmlw, DescriptorManager dmgr){
        super (Tags.error_tag, xmlw, dmgr);
    }
    
    /**
     * Elements should be packed in the vector obj in the specific order.
     */
    public void description (Object obj){
        Vector error = (Vector) obj;
        /* unpack the vector */
        String msg = (String) error.elementAt (0);        
        String filename = (String) error.elementAt (1);
        int lineno = ((Integer) error.elementAt (2)).intValue(); 
        String linebuf = (String) error.elementAt (3); 
        int tokenOffset = ((Integer) error.elementAt (4)).intValue(); 
        if (msg == null) msg = "none";
        if (filename == null) filename = "none";
        if (linebuf == null) linebuf = "none";
        _xmlw.tag (Tags.message_tag, msg);
        _xmlw.tag (Tags.url_tag, filename);
        _xmlw.tag (Tags.lineno_tag, lineno);
        _xmlw.tag (Tags.linebuf_tag, linebuf);
        _xmlw.tag (Tags.token_offset_tag, tokenOffset);
        
    }

}