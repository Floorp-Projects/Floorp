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

import java.util.Enumeration;

public class SelfTest {
    public SelfTest () {
    }

    ////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////
    //example #0
    //List all cards and their attributes.
    public static void main(String argv[]) {
        //open the LDAP address card source ldap.infospace.com
        ICardSource Four11AddressBook = new LDAP_Server ("ldap.whowhere.com");

        //define the card query.
        ITerm query = new TermEqual (new AC_Attribute ("sn", "Lester"));

        //Define the attributes to return
        String[] attributes = {"sn", "cn", "o", "mail", "city"};

        //query the LDAP server.
        ICardSet cardSet = Four11AddressBook.getCardSet (query, attributes);

        //Sort the list.
        String[] sortOrder = {"sn", "mail"};
        cardSet.sort (sortOrder);

        //Enumerate thru the cards.
        for (Enumeration cardEnum = cardSet.getEnumeration(); cardEnum.hasMoreElements(); ) {
            ICard card = (ICard) cardEnum.nextElement();

            System.out.println ("==== Card ====");
            IAttributeSet attrSet = card.getAttributeSet ();

            //Enumerate thru the attributes.
            for (Enumeration attEnum = attrSet.getEnumeration(); attEnum.hasMoreElements(); ) {
                IAttribute attr = (IAttribute) attEnum.nextElement();

                System.out.println ("Attr: " + attr.getName() + " = " + attr.getValue());
            }
        }

        //AC_Sources close when they fall out of scope.
//        LDAP_Server.close ();
    }

/*
    //example #1
    //Tranfering card from LDAP server to local address book using an "AddressSet".
    public static void main(String argv[]) {
        //open the LDAP address card source
        IAC_Source Four11AddressBook = new LDAP_Server ("ldap.whowhere.com");
//        IAC_Source Four11AddressBook = new LDAP_Server ("lpad.four11.com", "lesters", "SecretPassword");

        //Create the search
            FullName

            NickName
            FirstName
            MiddleName
            LastName
            Organization
            Locality
            Region
            EmailAddress
            Comment
            HTMLMail
            Title
            PostOfficeBox
            Address
            ZipCode
            Country
            WorkPhone
            HomePhone
            FaxPhone
            ConferenceAddress
            ConferenceServer
            DistinguishedName

        TermEqual (AC_Attribute);
        TermNotEqual (AC_Attribute);
        TermGreaterThan (AC_Attribute);
        TermGreaterThanOrEqual (AC_Attribute);
        TermLessThan (AC_Attribute);
        TermLessThanOrEqual (AC_Attribute);

 //       AB.Search.Term query = new TermOr ( new AC_Attribute ("givenname", "Lester"),
//                                            new AC_Attribute ("xmozillanickname", "Les"));

        ITerm query = new TermEqual (new AC_Attribute ("sn", "Lester"));

        //Define the attributes to return
//        String[] attributes = {"givenname", "surname", "o", "mail"};
        String[] attributes = {"cn", "o", "mail"};
        String[] sortOrder = {"givenname", "surname"};

        //query the LDAP server.
        AC_Set cardSet = Four11AddressBook.getCardSet (query, attributes);

        //Sort the list.
        cardSet.sort (sortOrder);

        //print the list of cards and their attributes.
        for (Enumeration cardEnum = cardSet.getCardEnumeration(); cardEnum.hasMoreElements(); ) {
            AC card = (AC) cardEnum.nextElement();

            System.out.println ("==== Card ====");
            AC_AttributeSet attrSet = card.getAttributeSet ();

            for (Enumeration attEnum = attrSet.elements(); attEnum.hasMoreElements(); ) {
                AC_Attribute attr = (AC_Attribute) attEnum.nextElement();

                System.out.println ("Attr: Name = " + attr.getName() + " Value = " + attr.getValue());
            }
        }

        //open the local address card source
        //  (true = create if needed)
        ACS_Personal myLocalAddressBook = new ACS_Personal ("MyAddressBook.nab", true);

        //copy over the address cards
        //  true = overwrite if collision)
        myLocalAddressBook.add (cardSet, true);

        //AC_Sources close when they fall out of scope.
        myLocalAddressBook.close ();
    }

    //example #2
    //Transfer cards from LDAP server to local address book using enumeration.
    main () {
        //open the LDAP address card source
        AC_Source Four11AddressBook = new LDAPServer ("lpad.four11.com", "lesters", "SecretPassword");

        //Create the search
        AB.Search.Term query = new TermOr ( new AC_Attribute ("givenname", "Lester"),
                                            new AC_Attribute ("xmozillanickname", "Les"));

        //Define the attributes to return
        String[] attributes = {"givenname", "surname", "o", "mail"};

        //open the local address card source
        //  (true = create if needed)
        AC_Source myLocalAddressBook = new LocalStore ("MyAddressBook.nab", true);

        //query the LDAP server.
        for (Enumeration enum = Four11AddressBook.getCardEnumeration (query, attributes); enum.hasMoreElements() ;) {
            AC card = (AC_) enum.nextElement();

            //only copy the netscape people.
            if (true == card.getAttribute ("o").getValue().equals ("Netscape")) {
                //  true = overwrite if collision)
                myLocalAddressBook.add (card, true);
            }
       }

        //AC_Sources close when they fall out of scope.
    }


    //example #3
    //Transfer cards from LDAP server to local address book using an "AddressList" and enumeration.
    main () {
        //open the LDAP address card source
        AC_Source Four11AddressBook = new LDAPServer ("lpad.four11.com", "lesters", "SecretPassword");

        //Create the search
        AB.Search.Term query = new OrTerm ( new AC_Attribute ("givenname", "Lester"),
                                            new AC_Attribute ("xmozillanickname", "Les"));

        //Define the attributes to return
        String[] attributes = {"givenname", "surname", "o", "mail"};
        String[] sortOrder = {"givenname", "surname"};

        //query the LDAP server.
        AC_Set cardSet = Four11AddressBook.getCardSet (query, attributes);

        //Sort the list.
        cardSet.sort (sortOrder);

        //open the local address card source
        //  (true = create if needed)
        AC_Source myLocalAddressBook = new LocalStore ("MyAddressBook.nab", true);

        //Step thru the card list.
        for (Enumeration enum = cardSet.getCardEnumeration (); enum.hasMoreElements() ;) {
            AC card = (AC) enum.nextElement();

            //only copy netscape people.
            if (true == card.getAttribute ("o").equals ("Netscape")) {
                //  true = overwrite if collision)
                myLocalAddressBook.add (card, true);
            }
       }

        //AC_Sources close when they fall out of scope.
    }

    //example #4
    //Deleting card from the local addres book
    main () {
        //open the local address card source
        //  (false = fail if not already created)
        AC_Source myLocalAddressBook = new LocalStore ("MyAddressBook.nab", false);

        AB.Search.Term query = new AC_Attribute ("givenname", "Lester");

        //query the AC_Source
        AC_Set cardSet = myLocalAddressBook.getCardSet (query);

        //delete the cards
        myLocalAddressBook.delete(cardSet);

        //AC_Sources close when they fall out of scope.
    }


    //example #5
    //Tranfering card from LDAP server to local address book using enumeration and collision detection.
    main () {
        //open the LDAP address card source
        AC_Source Four11AddressBook = new LDAPServer ("lpad.four11.com", "lesters", "SecretPassword");

        //Create the search
        AB.Search.Term query = new AC_Attribute ("givenname", "Lester");

        //open the local address card source
        //  (true = create if needed)
        AC_Source myLocalAddressBook = new LocalStore ("MyAddressBook.nab", true);

        //query the LDAP server.
        for (Enumeration enum = Four11AddressBook.getCardEnumeration (query, attributes); enum.hasMoreElements() ;) {
            try {
                //  false = throw exception if collision
                myLocalAddressBook.add ((AC_) enum.nextElement(), false);

                // Alternate form (test the set).
                // if (false == myLocalAddressBook.willOverwrite ((AC_) enum.nextElement())) {
                //     myLocalAddressBook.add ((AC_) enum.nextElement(). false);
                // }

            } catch (CardOverwriteException coe) {
            }
       }

        //AC_Sources close when they fall out of scope.
    }

    //example #6
    //Find all the attributes of a card source?
    main () {
        //open the LDAP address card source
        AC_Source Four11AddressBook = new LDAPServer ("lpad.four11.com", "lesters", "SecretPassword");

        AC_AttributeSet attrs = Four11AddressBook.getAttributeSet();

        for (Enumeration enumAttrs = attrs.getAttributes(); ; enumAttrs.hasMoreElements() ;) {
                AC_Attribute anAttr = (AC_Attribute) enumAttrs.nextElement();

                System.out.println( "Attribute Name: " + anAttr.getName() );
        }

        //AC_Sources close when they fall out of scope.
    }

    //example #7
    //Update the two address books
    main () {
        //open the LDAP address card source
        AC_Source Four11AddressBook = new LDAPServer ("lpad.four11.com", "lesters", "SecretPassword");

        //open the local address card source
        AC_Source myLocalAddressBook = new LocalStore ("MyAddressBook.nab", true);

        //update the local address book from the LDAp server.
        myLocalAddressBook.update (Four11AddressBook);

        //update the LDAP server from the local address book.
        Four11AddressBook.update (myLocalAddressBook);

        //AC_Sources close when they fall out of scope.
    }

    //example #8
    //Transfer cards from serveral LDAP servers to local address book using an "AddressSet".
    main () {
        //open the LDAP address card source
        AC_SourceSet lpadServers = new AC_SourceSet();

        //add the LDAP servers to the set. Order is important.
        lpadServers.add (new LDAPServer ("lpad.four11.com", "lesters", "SecretPassword"));
        lpadServers.add (new LDAPServer ("lpad.infoseek.com", null, null));

        //get all cards that have a department of "k8040" or "K8050".
        Term query = new AC_Attribute ("xdepartment", {"k8040", "k8050"});

        //Define the attributes to return
        String[] attributes = {"givenname", "surname", "o", "mail", "xdepartment"};

        //query the LDAP servers.
        AC_Set cardSet = lpadServers.getCardSet (query, attributes);

        //open the local address card source
        //  (true = create if needed)
        AC_Source myLocalAddressBook = new LocalStore ("MyAddressBook.nab", true);

        //copy over the address cards
        //  true = overwrite if collision)
        myLocalAddressBook.add (cardSet, true);

        //AC_Sources close when they fall out of scope.
    }


    //example #9
    //Add a new card to the local address book then change it then delete it.
    main () {
        //Define the attributes to return
        String[] attributes = {"givenname", "surname", "o", "mail"};

        //open the local address card source
        //  (true = create if needed)
        AC_Source myLocalAddressBook = new LocalStore ("MyAddressBook.nab", true);
        AC newCard = new AC ();
        newCard.add (new AC_Attribute ("givenname", "Robert"));
        newCard.add (new AC_Attribute ("mail", "robert@ford.com"));
        newCard.add (new AC_Attribute ("o", "Ford Motor Company"));
        newCard.add (new AC_Attribute ("ou", "Audio"));
        newCard.add (new AC_Attribute ("c", "US"));

        //try to store the new card
        try {
            myLocalAddressBook.addCard (newCard);
        }
        catch (AC_Exception ace) {
            //1) No write access to DB
            //2) All required attributes not provided
            //3) Network connection down
            //4) Card already in DB
        }

        //try to change the card
        try {
            newCard.updateAttribute ("ou", "Electronics Division");
            myLocalAddressBook.updateCard (newCard);
        }
        catch (AC_Exception ace) {
            //1) No write access to DB or this card.
            //2) All required attributes not provided
            //3) Network connection down
        }

        //try to delete an attribute from the card
        try {
            newCard.deleteAttribute ("ou");
            myLocalAddressBook.updateCard (newCard);
        }
        catch (AC_Exception ace) {
            //1) No write access to DB or this card
            //2) Network connection down
            //3) Card NOT in DB
        }

        //try to delete the card
        try {
            myLocalAddressBook.deleteCard (newCard);
        }
        catch (AC_Exception ace) {
            //1) No write access to DB or this card
            //2) Network connection down
            //3) Card NOT in DB
        }

        //AC_Sources close when they fall out of scope.
    }



    //example #
    //List all cards and their attributes.
    public static void main(String argv[]) {
        //open the LDAP address card source
        IAC_Source Four11AddressBook = new LDAP_Server ("ldap.whowhere.com");



        //define the card query.
        ITerm query = new TermEqual (new AC_Scope ("ObjectType", "Room"));


        AB.Search.Term query = new AC_Attribute ("objectclass", "resource");
        AB.Search.Term query = new AC_Attribute ("objectclass", "person");


        //All object of type room on the second floor.
        ITerm query = new AC_Scope ("Room", new TermEqual (new AC_Attribute ("Floor", "2")));



        //Define the attributes to return
        String[] attributes = {"cn", "o", "mail"};
        String[] sortOrder = {"givenname", "surname"};

        //query the LDAP server.
        AC_Set cardSet = Four11AddressBook.getCardSet (query, attributes);

        //Sort the list.
        cardSet.sort (sortOrder);

        //Enumerate thru the cards.
        for (Enumeration cardEnum = cardSet.getCardEnumeration(); cardEnum.hasMoreElements(); ) {
            AC card = (AC) cardEnum.nextElement();

            System.out.println ("==== Card ====");
            AC_AttributeSet attrSet = card.getAttributeSet ();

            //Enumerate thru the attributes.
            for (Enumeration attEnum = attrSet.elements(); attEnum.hasMoreElements(); ) {
                AC_Attribute attr = (AC_Attribute) attEnum.nextElement();

                System.out.println ("Attr: Name = " + attr.getName() + " Value = " + attr.getValue());
            }
        }

        //AC_Sources close when they fall out of scope.
//        LDAP_Server.close ();
    }




    public static void main() {
                try {
            AC_Source Four11AddressBook = new LDAPServer ("lpad.four11.com", "lesters", "SecretPassword");

                        // search for all entries with surname of Jensen
                        String MY_FILTER = "sn=lester";
//                      String MY_SEARCHBASE = "o=Ace Industry, c=US";
                        String MY_SEARCHBASE = "c=US";

                        LDAPSearchConstraints cons = ld.getSearchConstraints();
                        cons.setBatchSize( 1 );
                        AC_Set cardSet = ld.search( MY_SEARCHBASE,
                                                                                                MY_FILTER,
                                                                                                null,
                                                                                                false,
                                                                                                cons );

                        // Loop on results until finished
                        while ( cardSet.hasMoreElements() ) {

                                // Next entry
                                AC card = cardSet.nextElement();

                                // Get the attributes of the entry
                                AC_AttributeSet attrs = card.getAttributeSet();
                                Enumeration enumAttrs = attrs.getAttributes();

                                // Loop on attributes
                                while ( enumAttrs.hasMoreElements() ) {
                                        AC_Attribute anAttr = (AC_Attribute) enumAttrs.nextElement();
                                        String attrName = anAttr.getName();
                                        System.out.println( "\t\t" + attrName );

                                        // Loop on values for this attribute
                                        Enumeration enumVals = anAttr.getStringValues();
                                        while ( enumVals.hasMoreElements() ) {
                                                String aVal = ( String )enumVals.nextElement();
                                                System.out.println( "\t\t\t" + aVal );
                                        }
                                }
                        }
                }
                catch( Exception e ) {
                        System.out.prin
        */
}
