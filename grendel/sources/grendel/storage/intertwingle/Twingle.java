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
 * Created: Terry Weissman <terry@netscape.com>,  7 Oct 1997.
 */

package grendel.storage.intertwingle;
import javax.mail.Folder;

import javax.mail.internet.InternetHeaders;

import calypso.util.Preferences;
import calypso.util.PreferencesFactory;

import java.io.File;
import java.io.IOException;

import java.util.StringTokenizer;
import java.util.Vector;

/** This is the glue between the storage of intertwingle data and the rest
  of the mail system.  It will probably get thrown out or extensively
  rewritten; I'm still playing. */

public class Twingle implements Runnable {
  static final private boolean DEBUG = false;
  static void Spew(String s) {
    if (DEBUG) System.err.println("Twingle: " + s);
  }


  static private Twingle Singleton = null;
  static private boolean Initialized = false;

  static public Twingle GetSingleton() {
    if (!Initialized) {
      synchronized (Twingle.class) {
        if (!Initialized) {
          File f = PreferencesFactory.Get().getFile("twingle.database", null);
          if (f != null) {
            try {
              Singleton = new Twingle(f);
            } catch (IOException e) {
              // ### What to do...
              System.out.println("Twingle.GetSingleton() failed: " + e);
            }
          }
          Initialized = true;
        }
      }
    }
    return Singleton;
  }

  protected DB db;
  protected Thread thread;
  protected Vector queue = new Vector();

  Twingle(File f) throws IOException {
    db = new BGDB(new SimpleDB(f), new File(f.getPath() + ".queue"));
    thread = new Thread(this);
    thread.setDaemon(true);
    thread.start();
  }

  void addassert(String name, String slot, String value) {
    if (name != null && value != null) {
      try {
        db.addassert(name, slot, value);
      } catch (IOException e) {
        // ### What to do...
        System.out.println("db.addassert() failed in Twingle.assert: " + e);
      }
    }
  }

  class AddCommand {
    InternetHeaders headers;
    Folder folder;
    AddCommand(InternetHeaders h, Folder f) {
      headers = h;
      folder = f;
    }
  }



  public void add(InternetHeaders headers, Folder folder) {
    synchronized (queue) {
      queue.addElement(new AddCommand(headers, folder));
      queue.notifyAll();
    }
  }


  /** Find the given header ("to" or "cc"), and assert an entry for each
    address in it.  This needs the code to parse the zillions of different
    kinds of addresses; for now, we just assume everything is of the form
    "Full name <mail@addr.com>". */

  protected void hackAddressList(String id, InternetHeaders headers,
                                 String slot) {
    String hh[] = headers.getHeader(slot);
    if (hh == null || hh.length == 0) return;
    String list = "";
    for (int i = 0; i < hh.length; i++) {
      if (i > 0) list += ",\r\n\t";
      list += hh[i];
    }
    StringTokenizer st = new StringTokenizer(list, ",");
    while (st.hasMoreTokens()) {
      String str = st.nextToken();
      String name;
      String addr;
      int at = str.indexOf('@');
      if (at < 0) continue;     // Some garbage that isn't an email address.
      int lt = str.indexOf('<');
      int gt = lt > 0 ? str.indexOf('>', lt) : -1;
      if (lt > 0 && lt < at && gt > at) {
         name = str.substring(0, lt).trim();
        addr = str.substring(lt + 1, gt).trim();
      } else {
        name = null;
        addr = str.trim();
      }
      if (name != null) {
        addassert(addr, "fullname", name);
      }
      addassert(id, slot, addr);
    }
  }


  public void run() {
    for (;;) {
      Object cmd;
      synchronized(queue) {
        while (queue.size() == 0) {
          try {
            Spew("bg: waiting");
            queue.wait();
            Spew("bg: awake");
          } catch (InterruptedException e) {
            return;
          }
        }
        cmd = queue.elementAt(0);
        queue.removeElementAt(0);
      }
      Spew("bg: doing one...");
      if (cmd instanceof AddCommand) {
        AddCommand addcmd = (AddCommand) cmd;
        InternetHeaders headers = addcmd.headers;
        Folder folder = addcmd.folder;

        String ids[] = headers.getHeader("Message-ID");
        String id;
        if (ids == null || ids.length == 0) {
          // MD5-hash-hack?  Well, maybe.  Not right now, though.  ###
          continue;
        }
        id = ids[0];
        if (id.charAt(0) == '<' && id.endsWith(">")) {
          id = id.substring(1, id.length() - 1);
        }
        addassert(id, "parent", folder.getName());

        String subj[] = headers.getHeader("Subject");
        if (subj != null && subj.length != 0)
          addassert(id, "subject", subj[0]);

        hackAddressList(id, headers, "from");
        hackAddressList(id, headers, "to");
        hackAddressList(id, headers, "cc");
      }
      Spew("bg: ...did one.");
    }
  }
}

