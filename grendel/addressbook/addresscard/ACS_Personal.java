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

import grendel.storage.intertwingle.*;
import java.util.Enumeration;
import java.io.File;
import java.io.IOException;
//import java.util.Vector;

//************************
//************************
public class ACS_Personal implements ICardSource, IQuerySet {
    private DB fDB;

    public ACS_Personal (String FileName, boolean CreateOnOpen) {
        File DBFile = new File(FileName);
        try {
            fDB = new SimpleDB(DBFile);
        } catch (Exception e) {}
    }

    /** Get the next card ID for this DB.
     */
    private String getNextCardID () {
        int nextCardIntID = 0;
        String nextCardStrID;

        try {
            //initalize the next card ID
            nextCardStrID = fDB.findFirst ("Control", "NextCardID", false);

            //if the value wasn't found then assume this is the first time.
            if ((null == nextCardStrID) || (nextCardStrID.equals (""))) {
                nextCardIntID = 0;
            }

            //if the value WAS found then try to extract an integer value from the text.
            else {
                try {
                    nextCardIntID = Integer.valueOf(nextCardStrID).intValue();
                } catch (NumberFormatException nfe) {
                    nextCardIntID = 0;
                }
            }
        } catch (Exception e) {
            nextCardIntID = 0;
        }

        //create a string representation of the card's ID.
        nextCardStrID = String.valueOf(++nextCardIntID);

        try {
            //write the new value out to the DB.
            fDB.assert ("Control", "NextCardID", nextCardStrID);
        } catch (IOException ioe) {
        }

        return nextCardStrID;
    }

    /**
     * closing the source
     */
    public void close () {
        try {
            ((BGDB) fDB).flushChanges();
        } catch (IOException ioe) {
        }
    }

  /** 
   * No-op implementations for now (just to get this building properly)
   * (Jeff)
   */

  public AC_IDSet opEqual(IAttribute anAttribute) { return null; }

  public AC_IDSet opNotEqual(IAttribute anAttribute) { return null; }

    /**
     * retrieving address cards
     */
    public ICardSet getCardSet (ITerm aQueryTerm, String[] anAttributesArray) {
        //get the card ID's that satisfy the query.
        AC_IDSet CardIDSet = aQueryTerm.evaluate_ACSP (this);

        //create the address card set that'll be returned.
        AddressCardSet retCardSet = new AddressCardSet ();

        for (int i = 0; i < CardIDSet.size(); i++) {
            //create an attribute set for the card.
            AddressCardAttributeSet attrSet = new AddressCardAttributeSet();

            //get all attributes for this card.
            for (int j = 0; j < anAttributesArray.length; j++) {
                String attrName = anAttributesArray[j];

                //read the attribute from the DB.
                try {
                    String attrValue = fDB.findFirst((String) CardIDSet.elementAt(i), attrName, false);

                    //if found then add to the attribute set for this card.
                    if (null != attrValue) {
                        attrSet.add (new AddressCardAttribute (attrName, attrValue));
                    }
                } catch (IOException ioe) {}
            }

            //create the new card, and add to the return card set.
            retCardSet.add (new AddressCard(this, attrSet));
        }

        //return the card set
        return retCardSet;
    }

    /** Add a single card to this addressbook.
     */
    public void add (AddressCard aCard, boolean OverWrite) {
         //give the card a new ID
        String thisCardID = getNextCardID ();
        aCard.setID (thisCardID);

        //get the set of attributes and enumerate thruough them.
        AddressCardAttributeSet attrSet = 
          (AddressCardAttributeSet)aCard.getAttributeSet();

        for (Enumeration enum = attrSet.elements (); enum.hasMoreElements(); ) {
            //get the next attribute
            AddressCardAttribute attr = (AddressCardAttribute) enum.nextElement ();

            //write the attribute to the DB
            try {
                fDB.assert (thisCardID, attr.getName(), attr.getValue());
            } catch (IOException ioe) {}
        }
    }

    /** Add a set of cards to this addressbook.
     */
    public void add (AddressCardSet aCardSet, boolean OverWrite) {
        for (Enumeration enum = aCardSet.getCardEnumeration (); enum.hasMoreElements() ;) {
            AddressCard card = (AddressCard) enum.nextElement();
            add (card, OverWrite);
        }
    }

  /** No-op implementation of the add(ICard) method from ICardSource
   */
  public void add (ICard card) { }

  /** No-op implementation of update(ICard) from ICardSource
   */
  public void update(ICard card) { }

  /** No-op implementation of delete(ICard) from ICardSource
   */
  public void delete(ICard card) { }


    //*******************************
    //**** Operational functions ****
    //*******************************

    /** Search the database for all cards that match this value
     * and return a set Adddress Card ID's (ACID's).
     */
    public AC_IDSet opEqual (AC_Attribute ACA) {
        AC_IDSet retIDSet = new AC_IDSet();

        if (null != ACA) {
         try {   //the RDFish DB returns an enumeration of a matching card ID's
            for (Enumeration enum = fDB.findAll(ACA.getName(), ACA.getValue (), false); enum.hasMoreElements() ;) {
                retIDSet.addElement (enum.nextElement());
            }
         } catch (IOException exc) {
           exc.printStackTrace();
         }
        }

        return retIDSet;
    }

    /** Search the database for all cards that DO NOT match this value
     * and return a set Adddress Card ID's (ACID's).
     */
    public AC_IDSet opNotEqual (AC_Attribute ACA) {
        AC_IDSet retIDSet = new AC_IDSet();

        return retIDSet;
    }

/*
1.Boolean RDF_HasAssertion (RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, Boolean tv);
Returns true if there is an arc in the graph whose origin is u, whose label is s, whose target is v and whose truth value is true.

2.void* RDF_GetSlotValue (RDF_Resource u, RDF_Resource s, RDF_ValueType type, Boolean inversep, Boolean tv);
Returns the target (whose datatype is type) of (any) one of the arcs whose source is u, label is s and truth value is tv. If inversep is true,
then it return the source (whose datatype is type) of (any) one of the arcs whose target is u, label is s and truth value is tv.

3.RDF_Cursor RDF_GetSlotValues (RDF_Resource u, RDF_Resource s, RDF_ValueType type, Boolean inversep, Boolean tv);
Returns a cursor that can be used to enumerate through the targets (of datatype type) of the arcs whose source is u, label is s and truth
value is tv. If inversep is true, then the cursor enumerates through the sources of the arcs whose label is s, target is u and truth value is tv.
You can store your private data on the pdata field of the cursor. Cursors are cheap.

4.void* RDF_NextValue(RDF_Cursor c);
Returns the next value from this cursor. Returns NULL if there are no more values. The fields value and type hold the value and datatype
of the value respectively.

5.RDF_Error RDF_DisposeCursor (RDF_Cursor c);
When you are done with a cursor, call this function. Remember to free your pdata before calling this.
*/

}

