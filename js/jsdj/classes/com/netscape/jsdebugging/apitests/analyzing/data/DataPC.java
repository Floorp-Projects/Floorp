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
 * Given the head of a description subtree reconstructs fields which PC contains.
 * DataPoolManager is passed to look up serial numbers of objects inside the description.
 *
 * @author Alex Rakhlin
 */

public class DataPC extends DataSerializable {

    public DataPC (TreeNode head, DataPoolManager dpm){
        super (head, dpm);
        _is_valid = TreeUtils.getChildBooleanData (head, Tags.is_valid_tag);
        
        TreeNode t = TreeUtils.getFirstTagImmediate (head, Tags.source_location_tag);
        _sn_source_loc = TreeUtils.getChildIntegerData (t, Tags.serial_number_tag).intValue();
        
        _source_location = (DataSourceLocation) _dpm.lookup (Tags.source_location_tag, _sn_source_loc);
    }
    
    public String toString (){
        return " PC object, IS VALID: "+_is_valid +" has "+ _source_location;
    }

    public String toFormattedString (){
        return "PC object \n"+
               "IS VALID: "+_is_valid +"\n"+
               "   has \n"+ _source_location.toFormattedString();
    }
    
    public boolean equalsTo (Data d){
        DataPC pc = (DataPC) d;
        return (pc.getIsValid () == _is_valid &&  _source_location.equalsTo (pc.getSourceLocation()));
    }
    
    public boolean getIsValid () { return _is_valid; }
    public DataSourceLocation getSourceLocation () { return _source_location; }
    
    private boolean _is_valid;
    private int _sn_source_loc;
    
    private DataSourceLocation _source_location;
}
