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
 * Created: Terry Weissman <terry@netscape.com>, 26 Sep 1997.
 */

package grendel.storage.intertwingle;

import java.io.File;
import java.io.IOException;

import java.util.Enumeration;

public class SelfTest {


  public static void main(String args[])
    throws IOException
  {
    String home = System.getProperties().getProperty("user.home");
    File top = new File(home, "__hackdb__");
//    DB db = new HackDB(top);
    DB db = new SimpleDB(top);
    db = new BGDB(db, new File(home, "__hackdb__queue"));
    db.addassert("terry", "fullname", "Terry Weissman");
    db.addassert("jwz", "fullname", "Jamie Zawinski");
    db.addassert("terry", "phone", "650-937-2756");
    db.addassert("terry", "phone", "408-338-8227");
    db.addassert("jwz", "phone", "650-937-2620");
    db.addassert("jwz", "phone", "415-ACRIDHE");

    for (int j=0 ; j<2 ; j++) {

      for (int i=0 ; i<2 ; i++) {
        String n = (i == 0 ? "terry" : "jwz");
        System.out.println(n + "'s fullname is " + db.findFirst(n, "fullname",
                                                                false));
        for (Enumeration e = db.findAll(n, "phone", false) ;
             e.hasMoreElements() ;
              ) {
          String p = (String) e.nextElement();
          System.out.println(n + "'s phone is " + p);
        }
      }

      db.unassert("jwz", "phone", "415-ACRIDHE");
      System.out.println("");
    }

    if (db instanceof BGDB) {
      ((BGDB) db).flushChanges();
    }

  }

}
