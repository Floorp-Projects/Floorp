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

/** Defines an attribute set.
 */

public class AddressCardAttributeSet implements IAttributeSet {

  private Vector attrVec;

  public AddressCardAttributeSet() {
    attrVec = new Vector();
  }

  public void add(IAttribute anAttribute) {
    attrVec.addElement(anAttribute);
  }

  public Enumeration elements() {
    return getEnumeration();
  }

  //***********************************
  // Methods defined for IAttributeSet
  //***********************************
  public Enumeration getEnumeration() {
    return attrVec.elements();
  }

  public IAttribute getAttribute(String anAttributeName) {
    IAttribute anAttribute = null;
    for (int i = 0; i < attrVec.size(); i++) {
      if (((AddressCardAttribute)attrVec.elementAt(i)).getName() == anAttributeName) {
        anAttribute = (IAttribute)attrVec.elementAt(i);
      }
    }
    return anAttribute;
  }

  public int size() {
    return attrVec.size();
  }
  

}
