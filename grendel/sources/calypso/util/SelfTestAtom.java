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

class SelfTestAtom {
  public static void run()
    throws SelfTestException
  {
    // Create a bunch of atoms
    int count = 20000;
    int[] ids = new int[count];
    String[] strings = new String[count];
    Atom[] atoms = new Atom[count];
    for (int i = 0; i < count; i++) {
      strings[i] = asciify(i);
      atoms[i] = Atom.Find(strings[i]);
    }

    // Now verify that they act like atoms
    for (int i = 0; i < count; i++) {
      Atom atom = Atom.Find(strings[i]);
      if (atom == atoms[i]) {
        if (atom.toString() == strings[i]) {
          continue;
        }
        throw new SelfTestException("atom.toString() != strings[i]");
      } else {
        throw new SelfTestException("atom != atoms[i]");
      }
    }
  }

  static String asciify(int i) {
    return Integer.toString(i, 16);
  }
}
