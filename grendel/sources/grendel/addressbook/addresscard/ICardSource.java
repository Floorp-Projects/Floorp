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

//************************
//************************
public interface ICardSource {
    /**
     * closing the source
     */
    public void close ();

    /**
     * replication
     * Update this address card source
     */
//    public void update (ICardSource aSourceOfChangedCards);

    /**
     * retrieveing address cards
     */
//    public Enumeration      getCardEnumeration (Term QueryTerm, String[] AttributesArray);
    public ICardSet   getCardSet (ITerm QueryTerm, String[] AttributesArray);
//    public void             getCardSink (Term QueryTerm, String[] AttributesArray, AC_Sink aCardSink);

    /** add a card(s)
     */
    public void add (ICard aCard) throws AC_Exception;
//    public void add (AC_Set aCardSet) throws AC_Exception;

    /** update a card(s)
     */
    public void update (ICard aCard) throws AC_Exception;
//    public void update (AC_Set aCardSet) throws AC_Exception;

    /** delete a card(s)
     */
    public void delete (ICard aCard) throws AC_Exception;
//    public void delete (AC_Set aCardSet) throws AC_Exception;
}

