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