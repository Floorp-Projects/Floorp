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
