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
