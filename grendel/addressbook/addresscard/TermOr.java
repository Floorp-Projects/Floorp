/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 *
 * Created: Lester Schueler <lesters@netscape.com>, 14 Nov 1997.
 */

package grendel.addressbook.addresscard;

//************************
//************************
public class TermOr implements ITerm {
    ITerm fLeftTerm;
    ITerm fRightTerm;

    public TermOr (ITerm aLeftTerm, ITerm aRightTerm) {
        fLeftTerm = aLeftTerm;
        fRightTerm = aRightTerm;
    }

    public String getExpression(IQueryString iqs) {
        return iqs.opOr (fLeftTerm, fRightTerm);
    }

    /** Logically or the left and right sets.
     */
    public AC_IDSet getSet (IQuerySet iqs) {
        AC_IDSet leftCardIDs = fLeftTerm.getSet (iqs);
        AC_IDSet rightCardIDs = fRightTerm.getSet (iqs);

        //merge and sort the left and right lists.
        AC_IDSet mergedIDs = new AC_IDSet (leftCardIDs);
        mergedIDs.addSet (rightCardIDs);
        mergedIDs.sort(AC_IDSet.ASCENDING);

        //Don't process any more if the merged set is emtpy.
        if (0 == mergedIDs.size())
            return mergedIDs;   //return an empty merged set.

        //prime the pump.
        String lastID = (String) mergedIDs.elementAt(0);

        //remove any duplicates from the SORTED list.
        for (int i = 0; i < mergedIDs.size(); ) {
            String currentID = (String) mergedIDs.elementAt(i);

            //if the last equals this (this list is sorted) then remove this duplicate.
            if (lastID.equals (currentID)) {
                mergedIDs.removeElementAt (i);
            }
            else {
                lastID = currentID;
                i++;    //advance to the next cardID
            }
        }

        return mergedIDs;
    }

  /** No-op for now 
   *  (Jeff)
   */
  public AC_IDSet evaluate_ACSP (IQuerySet iqs) { return null; }


}

