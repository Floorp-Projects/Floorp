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
 * Created: Jeff Galyan <talisman@anamorphic.com>, 16 Jan 1999
 */

package grendel.addressbook.addresscard;

import java.util.Enumeration;
import java.util.Vector;

/** Defines an addresscard set.
 */

public class AddressCardSet implements ICardSet {

  private Vector cards;

  public AddressCardSet() {
    cards = new Vector();
  }

  public Enumeration getCardEnumeration() {
    return getEnumeration();
  }

  //*********************************
  // Methods defined for ICardSet
  //*********************************
  public void add(ICard aCard) {
    cards.addElement(aCard);
  }

  public void remove(ICard aCard) {
    for (int i = 0; i < cards.size(); i++) {
      if (cards.elementAt(i).equals(aCard)) {
        cards.removeElementAt(i);
      }
    }
    cards.trimToSize();
  }

  public void sort(String[] anAttributeArray) {
    Enumeration enum = getEnumeration();
    while (enum.hasMoreElements()) {
    }
  }

  public Enumeration getEnumeration() {
    return cards.elements();
  }


}
