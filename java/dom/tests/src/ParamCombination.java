/**
 * @version 1.00 06 Jul 1999
 * @author Raju Pallath
 */

package org.mozilla.dom.test;

import java.lang.*;
import java.util.*;

class ParamCombination
{
   Object arrayOfVector[] = null;
   int totalCombinations = 0;
   int currIndex = 0;

   /**
   *
   * Constructor
   *
   * @param paramLength    No. of parameters which shall serve as index
   *                       to array ofVectors
   * @return     void
   *
   */
   public ParamCombination(int paramLength)
   {
       arrayOfVector = new Object[paramLength];
       if (arrayOfVector == null)  return;
   }

   /**
   *
   * This routine adds a new Vector into arrayOfVector
   *
   * @param v    Vector class containing values in string format
   *             viz: 0/null/DUMMY_STRING
   * @return     void
   *
   */
   public void addElement(Vector v)
   {
        if (v != null)
        {
           arrayOfVector[currIndex++] = v;
           if (totalCombinations == 0) totalCombinations = v.size();
           else totalCombinations = totalCombinations * v.size();
        }

      
   } 

   /**
   *
   * This routine adds a new Vector into arrayOfVector
   *
   * @return     array of Strings containing all combinations of values in 
   *             each Vector in vector array
   *
   */
   public String[] getValueList()
   {
        if (totalCombinations == 0) return null;

	String str[] = new String[totalCombinations];

        int len = arrayOfVector.length;
        if (len == 1)
        {
            Vector v = (Vector)arrayOfVector[0];
            for (int j=0; j< v.size(); j++)
                str[j] = (String)v.elementAt(j);
            return str;
        }
        
        Vector tmpVect = (Vector)arrayOfVector[len -1];
        for (int i=arrayOfVector.length-2;  i>= 0; i--)
        {
             tmpVect = getCombination((Vector)arrayOfVector[i], tmpVect); 
        }

	for (int i=0; i< tmpVect.size(); i++)
        {
            str[i] = (String)tmpVect.elementAt(i);
	}
        return str;
   }

   /**
   *
   * Get all combinations of values in Vectors v1 and v2
   * 
   * @param v1   Vector class containing values in string format
   *             viz: 0/null/DUMMY_STRING
   * @param v2   Vector class containing values in string format
   *             viz: 0/null/DUMMY_STRING
   * @return     vector containing combinations of above values.
   *             viz: null, null
   *                  null, DUMMY_STRING
   *                  0, null...
   *
   */
   private Vector getCombination( Vector v1, Vector v2)
   {
       Vector store = new Vector();
       for (int i=0; i< v1.size(); i++)
       {
         String vstr1 = (String)v1.elementAt(i);
         for (int j=0; j< v2.size(); j++)
         {
            String vstr2 = (String)v2.elementAt(j);
            String newstr = vstr1 + ", " + vstr2;
            store.addElement(newstr);
         }
       }
       return store;
   }
}
