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

/** Defines an attribute for an addresscard.
 */

public class AddressCardAttribute implements IAttribute {

  private String name, value;
  private boolean newness = true;
  private boolean deleted = false;
  private boolean modified = false;

  public AddressCardAttribute(String attrName, String attrValue) {
    name = attrName;
    value = attrValue;
  }

  
  //**********************
  // Methods defined for
  // IAttribute
  //**********************
  public String getName() {
    return name;
  }

  public String getValue() {
    return value;
  }

  public boolean isNew() {
    return newness;
  }

  public boolean isDeleted() {
    return deleted;
  }

  public boolean isModified() {
    return modified;
  }


}
