/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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