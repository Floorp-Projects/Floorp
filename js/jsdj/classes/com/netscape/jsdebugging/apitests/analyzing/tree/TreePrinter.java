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

/**
 * Prints trees, vectors of trees, etc.
 *
 * @author Alex Rakhlin
 */

public class TreePrinter {
    
    /**
     * Prints the tree down from the given node
     */
    public static void print (TreeNode node){
        System.out.println ("************* TREE START **************");
        _counter = 0;
        _pad();
        System.out.println (node);
        _printTree (node);
        System.out.println ("************* TREE END **************");
    }
    
    /**
     * Prints a vector of nodes 
     */
    public static void print (Vector list){
        for (int i = 0; i<list.size(); i++){
            TreeNode t = (TreeNode) list.elementAt (i);
            print (t);
        }
    }
    
    private static void _printTree (TreeNode node){
        if (node == null) return;
        _counter ++;
        
        TreeNode t;
        Vector children = node.getChildren();
        int num = children.size();
        for (int i = 0; i < num; i++){
            t = (TreeNode) children.elementAt (i);
            if (t == null) continue;
            _pad();
            System.out.println (t);
            _printTree (t);
        }
        
        _counter --;
    }
    
    private static void _pad (){
        String s = "";
        for (int i = 0; i<3*_counter; i++) s = s+" ";
        System.out.print (s);
    }
    
    
    private static int _counter;
}