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

package calypso.util;

import java.util.Enumeration;
import java.util.NoSuchElementException;

class WeakLinkArrayEnumeration implements Enumeration {
  WeakLink fArray[];
  int fCount;
  int fOffset;

  WeakLinkArrayEnumeration(WeakLinkArray aArray) {
    fArray = aArray.fArray;
    fCount = aArray.fCount;
  }

  public boolean hasMoreElements() {
    return fOffset < fCount;
  }

  public Object nextElement() {
    if (fOffset == fCount) {
      throw new NoSuchElementException();
    }
    return fArray[fOffset++];
  }
}
