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

/**
 * Various  utilities for working with trees.
 *
 * @author Alex Rakhlin
 */

public class TreeUtils {
    
    /**
     * Goes through the tree, sets pointers of nodes to point to the objects corresponding
     * to serial numbers, removes the serial number tag altoghether.
     */
    public static void replaceSerialNumbersByPointers (DataPoolManager dpm, TreeNode head, String sn_tag){
        if (head == null) return;
        Vector children = head.getChildren();
        for (int i = 0; i<children.size(); i++){
            TreeNode t = (TreeNode) children.elementAt (i);
            if (t.getTag().equals (sn_tag)) {
                head.setPointer (dpm.lookup (head.getTag(), Integer.valueOf(t.getText()).intValue()));
                t.getParent().removeChild (t);
            }
            else  replaceSerialNumbersByPointers (dpm, t, sn_tag);
        }
    }
    
    /**
     * OK. So, we can have two nodes in the tree point to two different objects in the pools
     * which actually have the same data. When we remove duplicates, we leave only one, and set
     * the "real_pointer" pointer of the second object to point to the first. Now we traverse the
     * tree and change pointers of the nodes to point to the objects which were not removed.
     * After this, duplicate objects, which were removed from pools should be gc'd.
     */
    public static void fixRealPointers (TreeNode head){
        if (head == null) return;
        Vector children = head.getChildren();
        for (int i = 0; i<children.size(); i++){
            TreeNode t = (TreeNode) children.elementAt (i);
            if (t.getPointer () != null) {
                DataSerializable redirect = t.getPointer().getRealPointer();
                if (redirect != null) {
                    //System.out.println ("Redirecting pointer from "+t.getPointer()+" to "+redirect);
                    t.setPointer(redirect);
                }
            }
            fixRealPointers (t);
        }
    }
    
    /**
     * Get the String data of the node
     */
    public static String getStringData (TreeNode head){
        return head.getText();
    }

    /**
     * Get the boolean data of the node
     */
    public static boolean getBooleanData (TreeNode head){
        String s = head.getText();
        if (s.equalsIgnoreCase ("true")) return true;
        return false;
    }

    /**
     * Get the integer data of the node
     */
    public static Integer getIntegerData (TreeNode head){
        Integer i = null;
        String s = head.getText();
        try{
            i = new Integer (s);
        } catch (NumberFormatException e) { System.out.println ("Invalid number format"); }
        return i;
    }
    
    /**
     * Same as above, but returns data of the first child with childTag tag
     */
    public static String getChildStringData (TreeNode head, String childTag){
        TreeNode child = getFirstTagImmediate (head, childTag);
        if (child == null) return null;
        return child.getText();
    }

    /**
     * Same as above, but returns data of the first child with childTag tag
     */
    public static Integer getChildIntegerData (TreeNode head, String childTag){
        TreeNode child = getFirstTagImmediate (head, childTag);
        if (child == null) return null;
        return getIntegerData (child);
    }

    /**
     * Same as above, but returns data of the first child with childTag tag
     */
    public static boolean getChildBooleanData (TreeNode head, String childTag){
        TreeNode child = getFirstTagImmediate (head, childTag);
        if (child == null) return false;
        return getBooleanData (child);
    }

    /**
     * Returns a vector of immediate children with the given tag
     */
    public static Vector searchImmediateChildren (TreeNode head, String tag){
        Vector result = new Vector();
        Vector children = head.getChildren();
        int size = children.size();
        for (int i = 0; i<size; i++){
            TreeNode t = (TreeNode) children.elementAt (i);
            if (t.getTag ().equals (tag)) result.addElement (t);
        }
        return result;
    }
    
    /**
     * Get the first immediate child with the given tag
     */
    public static TreeNode getFirstTagImmediate (TreeNode head, String tag){
        Vector children = head.getChildren();
        int size = children.size();
        for (int i = 0; i<size; i++){
            TreeNode t = (TreeNode) children.elementAt (i);
            if (t.getTag ().equals (tag)) return t;
        }
        return null;
    }
    
    /* Finds first occurence
    */
    public static TreeNode searchDepthFirst (TreeNode head, String tag){
        if (head == null) return null;
        Vector children = head.getChildren();
        for (int i = 0; i<children.size(); i++){
            TreeNode t = (TreeNode) children.elementAt (i);
            if (t.getTag().equals (tag)) return t;
            TreeNode result = searchDepthFirst (t, tag);
            if (result != null) return result;
        }
        return null;
    }
    
    /**
     * Returns all nodes with the specified tag. Children come before parents 
     */
    public static Vector findAllTags (TreeNode head, String tag){
        Vector list = new Vector();
        _findAllTags (head, tag, list);
        return list;
    }
    
    private static void _findAllTags (TreeNode head, String tag, Vector list){
        if (head == null) return;
        Vector children = head.getChildren ();
        for (int i = 0; i<children.size(); i++){
            TreeNode t = (TreeNode) children.elementAt (i);
            _findAllTags (t, tag, list);
            if (t.getTag().equals (tag)) list.addElement (t);
        }
    }
    
    /** 
     * Returns all nodes with the specified tag and specified child tag
     * Leaving tag="" outputs all tags with specified child tag 
     */
    public static Vector findAllTagsWithChild (TreeNode head, String tag, String childTag){
        Vector list = new Vector();
        _findAllTagsWithChild (head, tag, childTag, list);
        return list;
    }
    
    private static void _findAllTagsWithChild (TreeNode head, String tag, String childTag, Vector list){
        if (head == null) return;
        Vector children = head.getChildren ();
        for (int i = 0; i<children.size(); i++){
            TreeNode t = (TreeNode) children.elementAt (i);
            _findAllTagsWithChild (t, tag, childTag, list);
            if (t.getTag().equals (tag) || tag.equals("")) {
                Vector v = t.getChildren();
                if (searchImmediateChildren (t, childTag).size() != 0)
                    list.addElement (t);
            }
        }
    }
    
}