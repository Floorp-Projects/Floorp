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
import java.util.*;

/**
 * Given the head of a description subtree reconstructs fields which Eval contains.
 * DataPoolManager is passed to look up serial numbers of objects inside the description.
 *
 * @author Alex Rakhlin
 */

public class DataEval extends DataSerializable {

    public DataEval (TreeNode head, DataPoolManager dpm){
        super (head, dpm);        
        TreeNode t = (TreeNode) TreeUtils.getFirstTagImmediate (head, Tags.frame_tag);
        _frame = new DFrame (t, dpm);

        Vector results_tags = TreeUtils.findAllTags (head, Tags.results_tag);
        
        _eval_strings = new Vector ();
        _results = new Vector ();
        
        for (int i = 0; i < results_tags.size(); i++) {
            TreeNode results = (TreeNode) results_tags.elementAt (i);
            String eval_str = TreeUtils.getChildStringData (results, Tags.eval_string_tag);
            String result = TreeUtils.getChildStringData (results, Tags.result_tag);
            _eval_strings.addElement (eval_str);
            _results.addElement (result);
        }
    }

    public String toString (){
        String result = "";
        for (int i = 0; i < _eval_strings.size(); i++) 
            result = result + " eval ("+getEvalString (i) +") = "+getResult (i);
        return result + getFrame().toString();
    }

    public String toFormattedString (){
        String result = "";
        for (int i = 0; i < _eval_strings.size(); i++) 
            result = result + "eval ("+getEvalString (i) +") = "+getResult (i)+"\n";
        return result + getFrame().toFormattedString();
    }

    public boolean equalsTo (Data d){
        DataEval eval = (DataEval) d;
        return (eval.getFrame().equalsTo (_frame) && _equalVectors (eval.getEvalStrings(), _eval_strings) && 
                                                     _equalVectors (eval.getResults(), _results));
    }
    
    private static boolean _equalVectors (Vector v1, Vector v2){
        if (v1.size() != v2.size()) return false;
        for (int i = 0; i < v1.size(); i ++) {
            String s1 = (String) v1.elementAt (i);
            String s2 = (String) v2.elementAt (i);
            if (!s1.equals (s2)) return false;
        }
        return true;
    }

    public DFrame getFrame () { return _frame; }
    private DFrame _frame;
    
    public Vector getEvalStrings () { return _eval_strings; }
    public Vector getResults () { return _results; }
    
    public String getEvalString (int i) { 
        if (i < _eval_strings.size()) return (String) _eval_strings.elementAt (i);
        return "";
    }
    
    public String getResult (int i) { 
        if (i < _results.size()) return (String) _results.elementAt (i);
        return "";
    }
    
    private Vector _eval_strings;
    private Vector _results;
}
