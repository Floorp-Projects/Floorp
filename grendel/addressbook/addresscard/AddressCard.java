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

/** Defines an addresscard for the addressbook.
 */

public class AddressCard implements ICard {

  private String ID;
  private ICardSource mySource;
  private AddressCardAttributeSet myAttrSet;

  public AddressCard(ICardSource source, IAttributeSet attrSet) {
    mySource = source;
    myAttrSet = (AddressCardAttributeSet)attrSet;
  }

  public void setID(String thisCardID) {
    ID = thisCardID;
  }
  
  //******************************
  // Methods defined for ICard
  //******************************
  public ICardSource getParent() {
    return mySource;
  }

  public void addAttribute(IAttribute anAttribute) {
    myAttrSet.add(anAttribute);
  }

  public IAttribute getAttribute(String anAttributeName) {
    return myAttrSet.getAttribute(anAttributeName);
  }

  public IAttributeSet getAttributeSet() {
    return myAttrSet;
  }


}
