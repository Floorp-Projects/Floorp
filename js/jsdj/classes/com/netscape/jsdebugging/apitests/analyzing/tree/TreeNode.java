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