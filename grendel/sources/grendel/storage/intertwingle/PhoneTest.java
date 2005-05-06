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
 * Created: Terry Weissman <terry@netscape.com>,  4 Oct 1997.
 */

package grendel.storage.intertwingle;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.io.IOException;

import java.util.Enumeration;
import java.util.NoSuchElementException;
import java.util.StringTokenizer;

public class PhoneTest {


  public static void main(String args[])
    throws IOException
  {
    String home = System.getProperties().getProperty("user.home");
    String filename = args.length == 0 ? "__phonedb__" : args[0];
    File top = new File(home, filename);
    DB db = new SimpleDB(top);
    db = new BGDB(db, new File(home, filename + ".queue"));


    BufferedReader in = new BufferedReader(new InputStreamReader(System.in));

    for (;;) {
      System.out.print("Phonetest> ");
      System.out.flush();
      try {
        Thread.sleep(1000);
      } catch (InterruptedException e) {
        break;
      }
      String line = in.readLine();
      if (line == null) break;
      if (line.length() == 0) continue;
      if (line.charAt(0) == '<') {
        BufferedReader fid = null;
        String name = null;
        int count = 0;
        try {
          fid = new BufferedReader(new InputStreamReader(new FileInputStream(line.substring(1))));
          String str;
          while ((str = fid.readLine()) != null) {
            str = str.trim();
            if (str.length() == 0) {
              name = null;
            } else if (name == null) {
              name = str;
            } else {
              int i = str.indexOf(':');
              if (i > 0) {
                String slot = str.substring(0, i).trim();
                String value = str.substring(i + 1).trim();
//                System.out.println("Adding '" + name + "','" + slot + "','" +
//                                   value + "'");
                System.out.print(".");
                count++;
//XXXrlk: doing asserts later.
//                db.assert(name, slot, value);
              } else {
                name = str;
              }
            }
          }
        } catch (IOException e) {
        }
        System.out.println("");
        System.out.println(count + " assertions made.");
        if (fid != null) fid.close();
      } else {
        String name = null;
        String slot = null;
        String value = null;


        int nameend = line.indexOf(',');
        int slotend = line.indexOf(',', nameend + 1);

        if (nameend < 0) {
          name = line;
        } else {
          name = line.substring(0, nameend);
          if (slotend < 0) {
            slot = line.substring(nameend + 1);
          } else {
            slot = line.substring(nameend + 1, slotend);
            value = line.substring(slotend + 1);
          }
        }

        if (name != null && name.length() == 0) name = null;
        if (slot != null && slot.length() == 0) slot = null;
        if (value != null && value.length() == 0) value = null;

        // System.out.println("name: '" + name + "' slot: '" + slot + "' value: '" + value + "'");

        if (slot == null) {
          System.out.println("Gotta have a slot!");
        } else if (name == null && value == null) {
          System.out.println("Gotta have a name or value!");
        } else {
          Enumeration e = null;
          if (name == null) {
            e = db.findAll(value, slot, true);
          } else if (value == null) {
            e = db.findAll(name, slot, false);
          } else {
            if (name.charAt(0) == '-') {
              db.unassert(name.substring(1), slot, value);
              System.out.println("Unasserted.");
            } else {
//XXXrlk: doing asserts later.
//              db.assert(name, slot, value);
              System.out.println("Asserted.");
            }
          }
          if (e != null) {
            while (e.hasMoreElements()) {
              String result = (String) e.nextElement();
              System.out.println(result);
            }
          }
        }
      }
    }


    if (db instanceof BGDB) {
      ((BGDB) db).flushChanges();
    }

  }

}
