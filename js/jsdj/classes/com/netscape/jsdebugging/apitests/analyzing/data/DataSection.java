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
 * Given the head of a description subtree reconstructs fields which Section contains.
 * DataPoolManager is passed to look up serial numbers of objects inside the description.
 *
 * @author Alex Rakhlin
 */

public class DataSection extends DataSerializable {

    public DataSection (TreeNode head, DataPoolManager dpm){
        super (head, dpm);
        _baselineno = TreeUtils.getChildIntegerData (head, Tags.baselineno_tag).intValue();
        _line_extent = TreeUtils.getChildIntegerData (head, Tags.line_extent_tag).intValue();
    }

    public String toString (){
        return " Section object, BASE: "+_baselineno+" EXTENT: "+_line_extent;
    }
    
    public boolean equalsTo (Data d){
        DataSection s = (DataSection) d;
        return (s.getBaselineno () == _baselineno && s.getLineExtent() == _line_extent);
    }


    public int getBaselineno () { return _baselineno; }
    public int getLineExtent () { return _line_extent; }

    private int _baselineno;
    private int _line_extent;
}
