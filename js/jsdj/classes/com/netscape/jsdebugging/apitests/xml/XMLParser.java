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

package com.netscape.jsdebugging.apitests.xml;

import java.io.*;
import com.netscape.jsdebugging.apitests.analyzing.tree.*;

/**
 * Creates a tree from the XML file. 
 * NOTE: Current parsing relies on specific format of the file. For example, 
 * I assume that each line is either:
 * <TAG>
 * or 
 * <TAG>text</TAG>
 *
 * Anything else would crash the system.
 *
 * @author Alex Rakhlin
 */
public class XMLParser {
    
    public XMLParser (){
        _tag_handler = new TreeCreator ();
    }
    
    /**
     * parse the file into a tree. Tree can be retreived by calling getTree().
     */
    public void parse (String filename){
        RandomAccessFile _file = null;
        try {
            _file = new RandomAccessFile (filename, "r");
            String st = "";
            while ( null != (st = _file.readLine())) {
                _processLine(st);
            }
        } catch (IOException e) { System.out.println ("Error opening file"); return; }
    }
    
    private void _processLine (String st){
        int open1 = st.indexOf ('<');
        int close1 = st.indexOf ('>', open1);
        int open2 = st.indexOf ('<', close1);
        int close2 = st.indexOf ('>', open2);
        
        //System.out.println (st+" "+open1+" "+close1+" "+open2+" "+close2);

        if (open1 == -1) return;
        
        if (open2 == -1){
            if (st.charAt (open1+1) == '/') 
                _tag_handler.handleEndTag (new String (st.substring(open1+1, close1)), "");
            else _tag_handler.handleStartTag (new String (st.substring(open1+1, close1)));
        }
        else {
             _tag_handler.handleStartTag (new String (st.substring(open1+1, close1)));
             _tag_handler.handleEndTag (new String (st.substring(open2+2, close2)), 
                     XMLEscape.unescape(new String (st.substring(close1+1, open2))));
        }            
    }


    /**
     * Return the tree created from parsing
     */
    public TreeNode getTree () { return ((TreeCreator)_tag_handler).getTree(); }

    private TagHandler  _tag_handler;
}