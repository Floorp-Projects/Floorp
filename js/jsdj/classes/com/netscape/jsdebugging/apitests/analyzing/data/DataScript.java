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

import com.netscape.jsdebugging.apitests.xml.Tags;
import com.netscape.jsdebugging.apitests.analyzing.tree.*;
import java.util.*;

/**
 * Given the head of a description subtree reconstructs fields which Script contains.
 * DataPoolManager is passed to look up serial numbers of objects inside the description.
 *
 * @author Alex Rakhlin
 */
public class DataScript extends DataSerializable {

    public DataScript (TreeNode head, DataPoolManager dpm){
        super (head, dpm);
        _url = TreeUtils.getChildStringData (head, Tags.url_tag);
        _fun = TreeUtils.getChildStringData (head, Tags.function_tag);
        _baselineno = TreeUtils.getChildIntegerData (head, Tags.baselineno_tag).intValue();
        _line_extent = TreeUtils.getChildIntegerData (head, Tags.line_extent_tag).intValue();
        _is_valid = TreeUtils.getChildBooleanData (head, Tags.is_valid_tag);
        
        TreeNode t = TreeUtils.getFirstTagImmediate (head, Tags.section_list_tag);
        Vector sec = TreeUtils.findAllTags (t, Tags.section_tag);
        _sections = new Vector();
        for (int i = 0; i < sec.size(); i++){
            TreeNode s = (TreeNode) sec.elementAt (i);
            DataSection d = (DataSection) _dpm.lookup (Tags.section_tag, TreeUtils.getChildIntegerData (s, Tags.serial_number_tag).intValue());
            _sections.addElement (d);
        }
    }
    
    public String toString (){
        return " Script object, URL: "+_url+" FUNCTION: "+_fun+" BASE: "+_baselineno+" EXTENT: "+_line_extent+" IS VALID: "+_is_valid;
    }
    
    public String toFormattedString (){
        return "Script object \n"+
               "URL: "+_url+"\n"+
               "FUNCTION: "+_fun+"\n"+
               "BASE: "+_baselineno+"\n"+
               "LINE EXTENT: "+_line_extent+"\n"+
               "IS VALID: "+_is_valid;
    }

    public boolean equalsTo (Data d) {
        DataScript s = (DataScript) d;
        return (s.getURL().equals (_url) && s.getFunction().equals (_fun) && s.getBaselineno() == _baselineno
             && s.getLineExtent() == _line_extent && s.getIsValid () == _is_valid); 
    }

    public String getURL () { return _url; }
    public String getFunction () { return _fun; }
    public int getBaselineno () { return _baselineno; }
    public int getLineExtent () { return _line_extent; }
    public boolean getIsValid () { return _is_valid; }
    
    private String _url;
    private String _fun;
    private int _baselineno;
    private int _line_extent;
    private boolean _is_valid;
    
    private Vector _sections;
}
