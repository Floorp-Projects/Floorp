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

import calypso.util.Assert;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;

import java.util.Enumeration;
import java.util.Vector;

/** This implements a RDF-ish database, where changes take very little time.
  It works by queueing up those changes, and having a low-priority
  background thread process that queue into real changes.  The pending changes
  also get reflected into a disk file, so that if we crash, we'll recover
  unfinished stuff when the app next starts.
  <p>
  Queries on the database will search the queue for pending changes that
  could affect that query, and will commit those changes immediately. */

public final class BGDB extends BaseDB implements Runnable {
  static final boolean DEBUG = false;
  static void Spew(String s) {
    if (DEBUG) System.err.println("BGDB: " + s);
  }


  class Command {
    public String command;
    public String name;
    public String slot;
    public String value;
    Command(String c, String n, String s, String v) {
      command = c;
      name = n;
      slot = s;
      value = v;
    }
    public String toString() {
      return command + ": '" + name + "','" + slot + "','" + value + "'";
    }

  };

  DB base;
  File log;
  RandomAccessFile logfid;
  Vector queue = new Vector();
  Thread thread;
  BGDB(DB d, File t) throws IOException {
    log = t;
    base = d;
    logfid = new RandomAccessFile(log, "rw");
    long length = logfid.length();
    while (logfid.getFilePointer() < length) {
      try {
        queue.addElement(new Command(logfid.readUTF(),
                                     logfid.readUTF(),
                                     logfid.readUTF(),
                                     logfid.readUTF()));
      } catch (IOException e) {
        break;
      }
    }
    thread = new Thread(this);
    thread.setDaemon(true);
    thread.setPriority(Thread.NORM_PRIORITY - 1);
    thread.start();
  }

  public synchronized void addassert(String name, String slot, String value)
    throws IOException
  {
    logfid.seek(logfid.length());
    logfid.writeUTF("assert");
    logfid.writeUTF(name);
    logfid.writeUTF(slot);
    logfid.writeUTF(value);
    queue.addElement(new Command("assert", name, slot, value));
    notifyAll();                // Inform bg thread it has work to do.
  }

  public synchronized void unassert(String name, String slot, String value)
    throws IOException
  {
    logfid.seek(logfid.length());
    logfid.writeUTF("unassert");
    logfid.writeUTF(name);
    logfid.writeUTF(slot);
    logfid.writeUTF(value);
    queue.addElement(new Command("unassert", name, slot, value));
    notifyAll();                // Inform bg thread it has work to do.
  }

  public synchronized String findFirst(String name, String slot,
                                       boolean reverse) throws IOException
  {
    flushChanges(name, slot, reverse);
    return base.findFirst(name, slot, reverse);
  }

  public synchronized Enumeration findAll(String name, String slot,
                                          boolean reverse)
    throws IOException
  {
    flushChanges(name, slot, reverse);
    return base.findAll(name, slot, reverse);
  }

  private void flushChanges(String name, String slot, boolean reverse)
    throws IOException
  {
    for (int i=0 ; i<queue.size() ; i++) {
      Command c = (Command) queue.elementAt(i);
      while (slot.equals(c.slot) && name.equals(reverse ? c.value : c.name)) {
        flushCommand(c);
        queue.removeElementAt(i);
        if (i >= queue.size()) break;
        c = (Command) queue.elementAt(i);
      }
    }
  }

  private void truncateLog() {
    try {
      logfid.close();
    } catch (IOException e) {
      // ### What to do...
      System.out.println("logfid.close() failed in BGDB.truncateLog: " + e);
    }
    logfid = null;
    log.delete();
    try {
      logfid = new RandomAccessFile(log, "rw");
    } catch (IOException e) {
      // ### What to do...
      System.out.println("opening logfid failed in BGDB.truncateLog: " + e);
    }
  }

  public synchronized void flushChanges() throws IOException {
    while (queue.size() > 0) {
      flushCommand((Command) queue.elementAt(0));
      queue.removeElementAt(0);
    }
    truncateLog();
  }

  private void flushCommand(Command c) throws IOException {
    if (DEBUG) {
      String who;
      Spew(((Thread.currentThread() == thread) ? "bg" : "fg") + " executing " +
           c);
    }
    if (c.command.equals("assert")) {
      //XXXrlk: doing asserts later.
//      base.assert(c.name, c.slot, c.value);
    } else if (c.command.equals("unassert")) {
      base.unassert(c.name, c.slot, c.value);
    } else {
      Assert.NotReached("Unknown command type in BGDB.flushCommand");
    }
  }


  public void run() {
    Spew("bg Startup.");
    for (;;) {
      synchronized(this) {
        if (queue.size() > 0) {
          try {
            flushCommand((Command) queue.elementAt(0));
          } catch (IOException e) {
            // ### What to do...
            System.out.println("flushCommand failed in BGDB.run: " + e);
          }
          queue.removeElementAt(0);
        } else {
          truncateLog();
          try {
            Spew("bg waiting...");
            wait();
            Spew("bg awake.");
          } catch (InterruptedException e) {
            return;             // ### Is this right???
          }
        }
      }
    }
  }
}
