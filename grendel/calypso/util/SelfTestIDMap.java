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

class SelfTestIDMap {
  public static void run()
    throws SelfTestException
  {
    int count = 20000;
    int[] ids = new int[count];
    String[] strings = new String[count];
    for (int i = 0; i < count; i++) {
      strings[i] = asciify(i);
    }

    // Fill the map
    long start = System.currentTimeMillis();
    IDMap map = new IDMap();
    for (int i = 0; i < count; i++) {
      ids[i] = map.stringToID(strings[i]);
    }
    long end = System.currentTimeMillis();
    System.out.println("stringToID: took " + (end - start) + "ms");

    // Test the map
    start = System.currentTimeMillis();
    for (int i = 0; i < count; i++) {
      int id = map.stringToID(strings[i]);
      if (ids[i] != id) {
        throw new SelfTestException("whoops: id=" + id + " ids[i]=" + ids[i]);
      }
    }
    end = System.currentTimeMillis();
    System.out.println("stringToID: took " + (end - start) + "ms");

    start = System.currentTimeMillis();
    for (int i = 0; i < count; i++) {
      String s = map.stringToString(strings[i]);
      if (s != strings[i]) {
        throw new SelfTestException("whoops: s=" + s + " strings[i]=" + strings[i]);
      }
    }
    end = System.currentTimeMillis();
    System.out.println("stringToString: took " + (end - start) + "ms");

    // Verify that id's map back to strings properly
    start = System.currentTimeMillis();
    for (int i = 0; i < count; i++) {
      String s = map.idToString(ids[i]);
      if (!s.equals(strings[i])) {
        throw new SelfTestException("whoops: s=" + s + " strings[i]=" + strings[i]);
      }
    }
    end = System.currentTimeMillis();
    System.out.println("idToString: took " + (end - start) + "ms");
  }

  static String asciify(int i) {
    return Integer.toString(i, 16);
  }
}
