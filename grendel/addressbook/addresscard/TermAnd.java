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
public class TermAnd implements ITerm {
    ITerm fLeftTerm;
    ITerm fRightTerm;

    public TermAnd (ITerm aLeftTerm, ITerm aRightTerm) {
        fLeftTerm = aLeftTerm;
        fRightTerm = aRightTerm;
    }

    public String getExpression(IQueryString iqs) {
        return iqs.opAnd (fLeftTerm, fRightTerm);
    }

    /** Call the Address Card Source to AND the left and right sets.
     */
    public AC_IDSet getSet (IQuerySet iqs) {
        AC_IDSet leftCardIDs = fLeftTerm.getSet (iqs);
        AC_IDSet rightCardIDs = fRightTerm.getSet (iqs);

        //create the merge cardID list for return.
        AC_IDSet mergedIDs = new AC_IDSet ();

        //keep only card ID's that are in both left and right terms.
        for (int i = 0; i < leftCardIDs.size(); i++) {
            AC_ID id = (AC_ID) leftCardIDs.elementAt (i);
            if (rightCardIDs.contains (id))
                mergedIDs.addElement (id);
        }

        return mergedIDs;
    }

  /** no-op for now
   *  (Jeff)
   */
  public AC_IDSet evaluate_ACSP (IQuerySet iqs) { return null; }

}
