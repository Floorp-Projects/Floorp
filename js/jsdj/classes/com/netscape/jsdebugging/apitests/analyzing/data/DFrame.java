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

package com.netscape.jsdebugging.apitests.analyzing.data;

import com.netscape.jsdebugging.apitests.analyzing.tree.*;
import com.netscape.jsdebugging.apitests.xml.Tags;

/**
 * Even though Frame is not something you can give a serial number for, we still want to 
 * represent it by an object. Don't put it into a pool.
 *
 * @author Alex Rakhlin
 */

public class DFrame extends Data {

    public DFrame (TreeNode head, DataPoolManager dpm){
        super (dpm);
        _is_valid = TreeUtils.getChildBooleanData (head, Tags.is_valid_tag);
        TreeNode t = TreeUtils.getFirstTagImmediate (head, Tags.pc_tag);
        /* IMPORTANT: confusing part: sometimes DFrame is sitting inside a description (eval)
           and sometimes not. If inside description, then getPointer() to a PC is not inited yet, 
           so we need to get the serial number and look up pc in the table. If it's not inside a
           description, then we can refer to getPointer because it was set after all descriptors were
           parsed */
        if (t.getPointer() != null) _pc = (DataPC) t.getPointer();
        else {
            int _sn_pc = TreeUtils.getChildIntegerData (t, Tags.serial_number_tag).intValue();
            _pc = (DataPC) _dpm.lookup (Tags.pc_tag, _sn_pc);            
        }
    }

    public boolean equalsTo (Data d){
        DFrame df = (DFrame) d;
        return (df.getPC().equalsTo (_pc) && df.getIsValid() == _is_valid);
    }
    
    public String toString () { return " Frame object: IS VALID: "+_is_valid+" has "+getPC(); }
    public String toFormattedString () { 
        return "Frame object: \n"+
                "IS VALID: "+_is_valid+"\n"+
                "  has \n"+getPC().toFormattedString();
    }

    public DataPC getPC () { return _pc; }
    public boolean getIsValid () { return _is_valid; }
    
    private DataPC _pc;
    private boolean _is_valid;
}
