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
  
package com.netscape.jsdebugging.apitests.analyzing.tree;

import java.util.*;
import com.netscape.jsdebugging.apitests.analyzing.data.*;
import com.netscape.jsdebugging.apitests.xml.Tags;

/**
 * TreeNode represents a node in the tree corresponding to a tag, text, and children
 * in the xml document we want to parse.
 *
 * @author Alex Rakhlin
 */

public class TreeNode {

    public TreeNode (TreeNode parent, String tag, String text){
        _parent = parent;
        _tag = Tags.encode (tag);
        _text = text;
        _children = new Vector();
        _pointer = null;
    }

    public TreeNode (TreeNode parent, String tag){
        _parent = parent;
        _tag = Tags.encode (tag);
        _text = "";
        _children = new Vector();
    }

    public void insertChild (TreeNode child){
        _children.addElement (child);
    }

    public void removeChild (TreeNode child){
        _children.removeElement (child);
    }

    public TreeNode insertChild (String tag){
        TreeNode result = new TreeNode (this, tag);
        insertChild (result);
        return result;
    }

    public TreeNode insertChild (String tag, String text){
        TreeNode result = new TreeNode (this, tag, text);
        insertChild (result);
        return result;
    }
    
    public void setText (String text){ _text = text; }
    
    /**
     * sets pointer to point to a Data object. This is used when descriptions are replaced
     * by actual references to objects
     */
    public void setPointer (DataSerializable pointer) { _pointer = pointer; }
    public DataSerializable getPointer () { return _pointer; }
    
    public String getTag () { return Tags.decode(_tag); }
    public String getText (){ return _text;}
    public Vector getChildren() { return _children; }
    public TreeNode getParent() { return _parent; }
    
    
    public String toString (){
        String s = "TAG: "+ getTag() +"   DATA: " + getText();
        if (_pointer != null) s = s + _pointer;
        return s;
    }
    
    public boolean equalNodeTags (TreeNode t){
        return (t.getTag().equals(Tags.decode (_tag)));
    }
    
    public boolean equalNodeTexts (TreeNode t){
        return (t.getText().equals(_text));
    }
    

    private TreeNode _parent;
    private int _tag;
    private String _text;
    private Vector _children;
    private DataSerializable _pointer;
}