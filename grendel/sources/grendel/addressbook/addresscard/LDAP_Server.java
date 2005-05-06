/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Created: Lester Schueler <lesters@netscape.com>, 14 Nov 1997.
 *
 * Contributors: Christoph Toshok <toshok@netscape.com>
 */

package grendel.addressbook.addresscard;

import netscape.ldap.*;
import java.util.*;

//************************
//************************
public class LDAP_Server implements ICardSource, IQueryString {
        LDAPConnection fConnection;

    public LDAP_Server (String ServerName) {
        this (ServerName, 389, "", "");
    }

    public LDAP_Server (String aServerName, int aPort, String aUserName, String aPassword) {
        try {
                // Connect to server.
                fConnection = new LDAPConnection();
                fConnection.connect (aServerName, aPort);

                // authenticate to the directory as nobody.
                //fConnection.authenticate( aUserName, aPassword );
                } catch( LDAPException e ) {
                        System.out.println( "Error: " + e.toString() );
                }

        return;
    }

    /** Close the addres card source
     */
    public void close () {
    }

    private final static String LEFT_PAREN = "(";
    private final static String RIGHT_PARAN = ")";

    private String escape (String aStringToEscape)  {
        String escapeChars = LEFT_PAREN + RIGHT_PARAN + "*";
        //FIX
        return aStringToEscape;
    }

    public String opAnd (ITerm aLeftTerm, ITerm aRightTerm) {
        return LEFT_PAREN + "&" + aLeftTerm.getExpression (this) + aRightTerm.getExpression (this) + RIGHT_PARAN;
    }

    public String opOr (ITerm aLeftTerm, ITerm aRightTerm) {
        return LEFT_PAREN + "|" + aLeftTerm.getExpression (this) + aRightTerm.getExpression (this) + RIGHT_PARAN;
    }

    public String opEqual (IAttribute ACA) {
        return LEFT_PAREN + ACA.getName() + "=" + escape (ACA.getValue ()) + RIGHT_PARAN;
    }

    public String opNotEqual (IAttribute ACA) {
        return LEFT_PAREN + "!" + opEqual (ACA) + RIGHT_PARAN;
    }

    public String opGTE (IAttribute ACA) {
        return LEFT_PAREN + ACA.getName() + ">=" + escape (ACA.getValue ()) + RIGHT_PARAN;
    }

    public String opLTE (IAttribute ACA) {
        return LEFT_PAREN + ACA.getName() + "<=" + escape (ACA.getValue ()) + RIGHT_PARAN;
    }


    /** Retrieveing address cards as a set.
     */
    public ICardSet getCardSet (ITerm aQueryTerm, String[] anAttributesArray) {
        final String COMMON_NAME_ID  = "cn";
        final String ORGANIZATION_ID = "o";
        final String COUNTRY_ID      = "c";
        final String SIR_NAME_ID     = "sn";
        final String MAIL_ID         = "mail";

        LDAPSearchResults queryResults = null;

        try {
            String filter = aQueryTerm.getExpression(this);
                String searchBase = COUNTRY_ID + "=" + "US";

                LDAPSearchConstraints cons = fConnection.getSearchConstraints();
                cons.setBatchSize( 1 );

            queryResults = fConnection.search(  searchBase,
                                                LDAPConnection.SCOPE_ONE,
                                                filter,
                                                anAttributesArray,
                                                false,
                                                cons);
        }
                catch( LDAPException e ) {
                        System.out.println( "Error: " + e.toString() );
                }

        return new LDAP_CardSet (this, queryResults);
    }


    /** Add an entry to the server.
     */
    public void add (ICard aCard) throws AC_Exception {
        //get all attributes for this card as a set
        IAttributeSet AC_attrSet = aCard.getAttributeSet ();   //From AddressCard
        LDAPAttributeSet LDAP_attrSet = new LDAPAttributeSet();  //To LDAP

        //enumerate thru each attribute and translate from CA attributes into LDAP attributes.
        for (Enumeration enumer = AC_attrSet.getEnumeration(); enumer.hasMoreElements() ;) {

            //Get the old
            IAttribute AC_attr = (IAttribute) enumer.nextElement();
            //Create the new
            LDAPAttribute LDAP_attr = new LDAPAttribute(AC_attr.getName(), AC_attr.getValue());

            //add the attribute to the LDAP set.
            LDAP_attrSet.add( LDAP_attr );
        }

        try {
            //FIX!!!
            //create a new enry for the LDAP server
            String dn = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
            LDAPEntry anEntry = new LDAPEntry( dn, LDAP_attrSet );

            //*** ADD THE ENTRY ***
            fConnection.add( anEntry );

        } catch (LDAPException le) {
            //FIX
            throw new AC_Exception ("Couldn't add card to server.");
        }
    }

    /** Delete an entry from the server.
     */
    public LDAP_Card checkMyCard (ICard aCard) throws AC_Exception {
        //check that the etnry came from an LDAP server.
        if (aCard instanceof LDAP_Card) {
            LDAP_Card card = (LDAP_Card) aCard;

            //check that the entry can from this LDAP server.
            if (this == card.getParent ()) {

                //everthing's fine so return the card.
                return card;
            }
            else {
                throw new AC_Exception ("Not from this server.");
            }
        }
        else {
          throw new AC_Exception ("Not from an LDAP server.");
        }
    }


    /** Delete an entry from the server.
     */
    public void delete (ICard aCard) throws AC_Exception {
        try {
            //make sure the card came from this server.
            LDAP_Card card = checkMyCard(aCard);   //throws AC_Exception.

            //FIX
            //?? What can still LOCALY happen to the card after it's deleted from the LDPA server??
            try {
                //*** DELETE THE ENTRY ***
                fConnection.delete ( card.getDN() );
            } catch (LDAPException le) {
                //FIX
                throw new AC_Exception ("Couldn't delete card from server.");
            }
        }
        //BAD card. The card didn't come from this server.
        catch (AC_Exception ace) {
            //rethrow the exception
            throw ace;
        }
    }

    /** Update an entries attributes on the server.
     */
    public void update (ICard aCard) throws AC_Exception {
        try {
            //make sure the card came from this server.
            LDAP_Card card = checkMyCard(aCard);   //throws AC_Exception.

            //Ok, the card came from us so lets try to update the server.
            //Get the set of attributes that have changed.
            IAttributeSet AC_attrSet = card.getAttributeSet ();

            //Create a modifcation set to provide to the LDAP server.
            LDAPModificationSet LDAP_modSet = new LDAPModificationSet();

            //enumerate thru each attribute
            for (Enumeration enumer = AC_attrSet.getEnumeration() ; enumer.hasMoreElements() ;) {
                IAttribute AC_attr = (IAttribute) enumer.nextElement();
                int LDAP_mod = 0;
                boolean attributeModified = false;

                //Has the attribute been added?
                if (true == AC_attr.isNew ()) {
                    LDAP_mod = LDAPModification.ADD;
                    attributeModified = true;
                }
                //Has the attribute been deleted?
                else if (true == AC_attr.isDeleted ()) {
                    LDAP_mod = LDAPModification.DELETE;
                    attributeModified = true;
                }
                //Has the attribute been changed?
                else if (true == AC_attr.isModified ()) {
                    LDAP_mod = LDAPModification.REPLACE;
                    attributeModified = true;
                }

                //if modified then add to mod set.
                if (true == attributeModified) {
                    LDAPAttribute LDAP_attr = new LDAPAttribute(AC_attr.getName(), AC_attr.getValue());
                    LDAP_modSet.add (LDAP_mod, LDAP_attr);
                }
            }

            //Did we get any modifcations to send to the server?
            if (0 < LDAP_modSet.size()) {
                //submit the modifcation set to the server
                try {

                    //*** MODIFY THE ENTRY ***
                    fConnection.modify (card.getDN(), LDAP_modSet);

                } catch (LDAPException le) {
                   //FIX
                    throw new AC_Exception ("Couldn't update server.");
                }
            }
        }

        //BAD card. The card didn't come from this server.
        catch (AC_Exception ace) {
            //rethrow the exception
            throw ace;
        }
//        //else the card didn't come from this server so throw excpetion
//        else {
//            //FIX
//            //Need to consider weather is ok for a card from one connection to update
//            //to another connection of both connections are talking to the same server.
//            throw new AC_Exception ("Address Card didn't originate from this connection.");
//        }
    }
}
