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
import java.util.*;

/**
 * Keeps a pool (Vector) of data objects
 *
 * @author Alex Rakhlin
 */

public class DataPool {

    public DataPool (){
        _pool = new Vector ();
    }

    public void add (DataSerializable d){
        _pool.addElement (d);
    }

    /**
     * Given a serial number, return corresponding object in the pool
     */
    public DataSerializable lookup (int sernum){
        /* loop through all elements and find matching serial number -- optimize later */
        for (int i = 0; i < _pool.size(); i++){
            DataSerializable d = (DataSerializable) _pool.elementAt (i);
            if (d.getSerialNumber() == sernum) return d;
        }
        return null;
    }

    /**
     * Find first occurences of data object (according to equalsTo method),
     * starting at index start
     */
    public DataSerializable findFirst (DataSerializable d, int start) {
        for (int i = start; i < _pool.size(); i++){
            DataSerializable data = (DataSerializable) _pool.elementAt (i);
            if (data.equalsTo (d)) return data;
        }
        return null;
    }

    /**
     * Find first occurences of data object (according to equalsTo method), starting at 0
     */
    public DataSerializable findFirst (DataSerializable d) { return findFirst (d, 0); }


    /**
     * Find all occurences of data object (according to equalsTo method), starting at index start
     */
    public Vector find (DataSerializable d, int start) {
        Vector result = new Vector ();
        for (int i = start; i < _pool.size(); i++){
            DataSerializable data = (DataSerializable) _pool.elementAt (i);
            if (data.equalsTo (d)) result.addElement (data);
        }
        return result;
    }

    /**
     * Remove all duplicates from the pool. Comparison done according to equalsTo method
     */
    public void removeDuplicates (){
        int i = 0;
        while (i < _pool.size() - 1){
            DataSerializable d = (DataSerializable) _pool.elementAt (i);
            Vector dup = find (d, i+1);
            for (int j = 0; j < dup.size(); j++) {
                DataSerializable elem = (DataSerializable) dup.elementAt (j);
                /* setting real pointer of the data object to point to the object it's a duplicate of */
                /* this is going to be used when updating the tree with fixTree */
                elem.setRealPointer (d);
                _pool.removeElement (elem); //remove duplicate from the pool
            }
            i++;
        }
    }
    /**
     * Important: this scheme assumes that there are no duplicates in each pool.
     * So, call removeDuplicates before calling this method
     */
    public static DataPool intersectTwoPools (DataPool dp1, DataPool dp2){
        DataPool intersectPool = new DataPool ();
        int i = 0;
        while (i < dp1.getPool().size()){
            DataSerializable d1 = (DataSerializable) dp1.getPool().elementAt (i);
            DataSerializable d2  = dp2.findFirst (d1); // using "equals" method of DataXXXX
            if (d2 != null){ // object is in both pools, so remove it from both and put into intersect
                dp1.getPool().removeElement (d1);
                dp2.getPool().removeElement (d2);
                intersectPool.getPool().addElement (d1);
            }
            else i++;
        }

        return intersectPool;
    }

    public void print (){
        System.out.println ("Printing pool of "+_pool.size()+" elements");
        for (int i = 0; i<_pool.size(); i++) System.out.println ((Data) _pool.elementAt(i));
    }

    public Vector getPool () { return _pool; }

    private Vector _pool;
}
