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
