/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil -*-
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
 */

/**
 * This takes a bunch of enumerators, and runs through them all, in the order
 * given.
 */

package calypso.util;

import java.util.Enumeration;
import java.util.NoSuchElementException;
import java.util.Vector;

public class EnumerationEnumerator implements Enumeration {
  Vector enums;
  int cur;

  public EnumerationEnumerator() {
    enums = new Vector();
    cur = 0;
  }

  public void add(Enumeration e) {
    enums.addElement(e);
  }

  public boolean hasMoreElements() {
    for (int i=cur ; i<enums.size() ; i++) {
      if (((Enumeration) enums.elementAt(i)).hasMoreElements()) {
  return true;
      }
    }
    return false;
  }

  public Object nextElement() throws NoSuchElementException {
    do {
      Enumeration e = ((Enumeration) enums.elementAt(cur));
      if (e.hasMoreElements()) {
  return e.nextElement();
      }
      cur++;
    } while (cur < enums.size());
    throw new NoSuchElementException();
  }
}
