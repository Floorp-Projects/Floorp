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
 * Given the head of a description subtree reconstructs fields which SourceLocation contains.
 * DataPoolManager is passed to look up serial numbers of objects inside the description.
 *
 * @author Alex Rakhlin
 */

public class DataSourceLocation extends DataSerializable {

    public DataSourceLocation (TreeNode head, DataPoolManager dpm){
        super (head, dpm);
        _lineno = TreeUtils.getChildIntegerData (head, Tags.lineno_tag).intValue();
        _url = TreeUtils.getChildStringData (head, Tags.url_tag);
    }
    
    public String toString (){
        return "SourceLocation object, LINENO: "+_lineno+" URL: "+_url;
    }
    
    public String toFormattedString (){
        return "SourceLocation object \n"+
               "LINENO: "+_lineno+"\n"+
               "URL: "+_url;
    }

    public boolean equalsTo (Data d){
        DataSourceLocation sl = (DataSourceLocation) d;
        return (sl.getLineno () == _lineno && sl.getURL().equals(_url));
    }
    
    public int getLineno () { return _lineno; }
    public String getURL () { return _url; }

    private int _lineno;
    private String _url;
}
