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
  
package com.netscape.jsdebugging.apitests.xml;
 
import com.netscape.jsdebugging.apitests.analyzing.tree.*;
import java.util.*;

/**
 * Creates a tree by handling start and end tags. This can be simplified because I'm no longer using
 * ibm xml parser.
 *
 * @author Alex Rakhlin
 */

public class TreeCreator implements TagHandler{

    public TreeCreator (){
        _processedHead = false;
    }

    public void handleStartTag (String tag){
        //System.out.println ("Start tag "+tag);
        if (!_processedHead) { //should happen only once
            _processedHead = true;
            _head = new TreeNode (null, tag);
            _current = _head;
        }
        else  _current = _current.insertChild (tag);
    }


    public void handleEndTag (String tag, String text){
        //System.out.println ("End tag "+ tag+" text "+text);
        // this is a hack which relies on the assumption that tags which have text have no children.
        if (_current.getChildren().size() == 0) _current.setText (text);
        _current = _current.getParent();
    }
    
    public TreeNode getTree () { return _head; }

    private TreeNode _head, _current;
    private boolean _processedHead;
}