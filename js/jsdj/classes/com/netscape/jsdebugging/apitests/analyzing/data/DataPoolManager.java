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

import com.netscape.jsdebugging.apitests.xml.Tags;
import com.netscape.jsdebugging.apitests.analyzing.tree.*;
import java.util.*;

/**
 * This class manages pools of objects.
 *
 * @author Alex Rakhlin
 */

public class DataPoolManager {

    public DataPoolManager (){
        _pools = new Hashtable ();  
        
        _addPools ();
    }

    /** 
     * Add standard pools at creation time 
     */
    private void _addPools (){
        _pools.put (DataPC.class, new DataPool ());
        _pools.put (DataEval.class, new DataPool ());
        _pools.put (DataError.class, new DataPool ());
        _pools.put (DataScript.class, new DataPool ());
        _pools.put (DataSection.class, new DataPool ());
        _pools.put (DataSourceLocation.class, new DataPool());
    }
    
    /** 
     * Get a pool corresponding to a tag name. 
     */
    public DataPool getPool (String name) {
        if (name.equals (Tags.pc_tag)) return (DataPool) _pools.get (DataPC.class);
        if (name.equals (Tags.script_tag)) return (DataPool) _pools.get (DataScript.class);
        if (name.equals (Tags.eval_tag)) return (DataPool) _pools.get (DataEval.class);
        if (name.equals (Tags.error_tag)) return (DataPool) _pools.get (DataError.class);
        if (name.equals (Tags.section_tag)) return (DataPool) _pools.get (DataSection.class);
        if (name.equals (Tags.source_location_tag)) return (DataPool) _pools.get (DataSourceLocation.class);
        return null;
    }
        
    /** 
     * Read object data from the tree and put it into an object in the corresponding pool 
     */    
    public DataSerializable addNewElement (TreeNode head){
        /* determine the type of the described object by getting "TYPE" tag text. */
        String type = TreeUtils.getFirstTagImmediate (head, Tags.type_tag).getText();
        DataSerializable d = null;
        if (type.equals (Tags.pc_tag)) d = new DataPC (head, this);
        if (type.equals (Tags.script_tag)) d = new DataScript (head, this);
        if (type.equals (Tags.source_location_tag)) d = new DataSourceLocation (head, this);
        if (type.equals (Tags.section_tag)) d = new DataSection (head, this);
        if (type.equals (Tags.error_tag)) d = new DataError (head, this);
        if (type.equals (Tags.eval_tag)) d = new DataEval (head, this);
        
        if (d != null) add (d, getPool (type));

        return null;
    }

    /* static: add given object to the given pool */
    public static void add (DataSerializable d, DataPool dp){
        dp.add (d);
    }
    
    public DataSerializable lookup (String name, int sernum){
        /* lookup an object given its type and serial number */
        DataPool p = getPool (name);
        if (p == null) {
            System.err.println ("!!! Unknown tag !!!");
            return null;
        }
        return p.lookup (sernum);
    }
    
    /**
     * Reads data from the descriptions, creates corresponding objects, removes
     * descriptions from the tree, replaces serial numbers in the tree by pointers to data objects
     */
    public void getDataFromTree (TreeNode head){
        /* get all pointers to the descriptions */
        Vector v = TreeUtils.findAllTags (head, Tags.description_tag);
        
        /* Add data for all Description tags and then delete tags 
         * Recursive descriptions done non-recursively (tey all appear in the vector)
         */
        for (int i = 0; i < v.size(); i++){
            TreeNode t = (TreeNode) v.elementAt (i);
            addNewElement (t);
            t.getParent().removeChild (t);
        }
        
        /* go through the tree and replace sernumbers 
         * by pointers to objects in corresponding pools */
        TreeUtils.replaceSerialNumbersByPointers (this, head, Tags.serial_number_tag);
    }
     
    /**
     * Removes duplicates for each pool, then goes through the tree and redirects pointers so that
     * they don't point to the removed duplicates, but rather point to the corresponding remaining
     * objects. Removed duplicates should therefore be garbage collected since there are no more
     * references to them.
     */
    public void removeDuplicatesAndFixTree (TreeNode head){
        /* for each pool remove duplicates in that pool */
        Enumeration e = _pools.elements();
        while (e.hasMoreElements()){
            DataPool d = (DataPool) e.nextElement();
            d.removeDuplicates();
        }
        /* now go through the tree and update references to data objects */
        TreeUtils.fixRealPointers (head);
    }
    
    /* static: intersect all pools in two pool managers and creates a new manager with intersected pools.
     * As sets, result = (dpm1 AND dpm2); dpm1 = (dpm1 - result); dpm2 = (dpm2 - result)*/
    public static DataPoolManager intersectPools (DataPoolManager dpm1, DataPoolManager dpm2){
        DataPoolManager intersect = new DataPoolManager ();
        Enumeration e = dpm1.getPools().keys ();
        while (e.hasMoreElements()){
            Class d = (Class) e.nextElement(); // lookup by keys
            
            if (dpm2.getPools().containsKey(d) && intersect.getPools().containsKey(d)){
                DataPool dp1 = (DataPool) dpm1.getPools().get (d);
                DataPool dp2 = (DataPool) dpm2.getPools().get (d);
                
                DataPool intersectPool = DataPool.intersectTwoPools (dp1, dp2);
                intersect.getPools().remove (d);
                intersect.getPools().put (d, intersectPool); // reinsert into intersect
            }
        }
        return intersect;
    }
    
    public void print (){
        System.out.println ("************** POOLS START ************");
        Enumeration e = _pools.elements();
        while (e.hasMoreElements()) ((DataPool) e.nextElement()).print();
        System.out.println ("************** POOLS END ************");
    }
    
    public void setTestInfo (DTestInfo dtinf) { _dtinf = dtinf; }
    public DTestInfo getTestInfo () { return _dtinf; }
    
    
    public Hashtable getPools () { return _pools; }
    
    private Hashtable _pools;    
    
    private DTestInfo _dtinf;
}
