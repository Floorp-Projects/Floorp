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
