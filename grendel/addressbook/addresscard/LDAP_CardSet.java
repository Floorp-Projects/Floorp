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
import java.util.Enumeration;

//************************
//************************
public class LDAP_CardSet implements ICardSet, Enumeration {
    private LDAP_Server         fParentSource;
    private LDAPSearchResults   fResultSet;

    /** Constructor for LDAP server.
     */
    protected LDAP_CardSet (LDAP_Server aParentSource, LDAPSearchResults aResultSet) {
        fParentSource = aParentSource;
        fResultSet = aResultSet;
    }

    /** Add a card to the set.
     *  This only addes the card to the set NOT the source of the set.
     */
    public void add (ICard aCard) {
//        fCardList.addElement (aCard);
    }

    /** Remove a card from the set.
     *  This only removes the card from the set NOT the source of the set.
     */
    public void remove (ICard aCard) {
    }

    /** Sort the set.
     */
    public synchronized void sort(String [] sortAttrs) {
        boolean[] ascending = {true, true};
        fResultSet.sort(new LDAPCompareAttrNames(sortAttrs, ascending));
    }

    /** Return the card set enumeration.
     *  We enumerate our own cards.
     *  FIX: Not multi-threaded!
     */
    public Enumeration getEnumeration() {
        //initiailize the enumeration cursor.
        return this;
    }

    /** Enumeration methods
     */
    public boolean hasMoreElements() {
        return fResultSet.hasMoreElements();
    }

    /** Enumeration methods
     */
    public Object nextElement() {
        //return the next LDAPentry as an LDAP_Card.
        LDAPEntry entry = (LDAPEntry) fResultSet.nextElement();

        //cache the element.
//        fMyElementCache.add (entry);

        //return the LDAPEntry rapped in an ICard interface.
        return new LDAP_Card (fParentSource, entry);
    }
}
