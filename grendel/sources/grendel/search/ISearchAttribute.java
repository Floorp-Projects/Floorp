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
 * Created: Will Scullin <scullin@netscape.com>,  9 Oct 1997.
 */

package grendel.search;

import java.awt.Component;

import javax.mail.search.SearchTerm;

public interface ISearchAttribute {
  /**
   * Returns a localized, user visible attribute string
   */

  public String getName();

  /**
   * Returns a non-localized attribute identifier
   */

  public Object getID();

  /**
   * Returns the number of available operators for this attribute
   */

  public int getOperatorCount();

  /**
   * Returns the localized, user visible operator string
   */

  public Object getOperator(int aIndex);

  /**
   * Returns the component to use for selecting a value
   */

  public Component getValueComponent();

  /**
   * Given a component, return the value represented by it.
   * The component is one returned by getValueComponent from
   * the same object.
   */

  public Object getValue(Component aComponent);

  /**
   * Returns an attribute search term.
   */

  public SearchTerm getAttributeTerm(Object aOperatorID, Object aValue);

  /**
   * Same as getName();
   */

  public String toString();
}
