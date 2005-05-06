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
import calypso.util.NullJavaEnumeration;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;

import java.util.Enumeration;
import java.util.Hashtable;
import java.util.NoSuchElementException;

/** This is a completely wretched thing that implements a RDF-ish database.
  It works by generating zillions of tiny files.  We should never ever really
  use this; it's just for playing.  If we actually ever do ship this, I will
  be forced to spend the rest of my life hunting down every copy on every
  machine in the world and eradicating it. */


final class HackDB extends BaseDB {
  private File top;
  private File tmpfile;
  private Hashtable slotdirs = new Hashtable();
  private Hashtable reverseslotdirs = new Hashtable();

  HackDB(File f) throws IOException {
    top = f;
    ensureDirectory(top);
    tmpfile = new File(top, "--tmp--");
  }


  private void ensureDirectory(File f) throws IOException {
    if (!f.exists()) f.mkdirs();
    if (!f.isDirectory()) {
      throw new IOException("Must be a directory: " + f);
    }
  }

  private File findSlotFile(String name, boolean reverse) throws IOException {
    Hashtable table = (reverse ? reverseslotdirs : slotdirs);
    File result = (File) table.get(name);
    if (result == null) {
      result = new File(top, reverse ? "reverse-" + name : name);
      ensureDirectory(result);
      table.put(name, result);
    }
    return result;
  }

  private void putone(File dir, String name, String value) throws IOException {
    File n = new File(dir, name);
    RandomAccessFile file = new RandomAccessFile(n, "rw");
    String line;
    long length = file.length();
    while (file.getFilePointer() < length) {
      line = file.readUTF();
      if (line.equals(value)) {
        file.close();
        return;
      }
    }
    file.writeUTF(value);
    file.close();
  }

  private void nukeone(File dir, String name, String value)
    throws IOException
  {
    File n = new File(dir, name);
    if (!n.exists()) return;
    RandomAccessFile file = new RandomAccessFile(n, "r");
    tmpfile.delete();
    RandomAccessFile out = new RandomAccessFile(tmpfile, "rw");
    long length = file.length();
    boolean found = false;
    int count = 0;
    while (file.getFilePointer() < length) {
      String line = file.readUTF();
      if (line.equals(value)) {
        found = true;
      } else {
        out.writeUTF(line);
        count++;
      }
    }
    file.close();
    out.close();
    if (found) {
      n.delete();
      if (count > 0) {
        tmpfile.renameTo(n);
      } else {
        tmpfile.delete();
      }
    } else {
      tmpfile.delete();
    }
  }


  public synchronized void addassert(String name, String slot, String value)
    throws IOException
  {
    File s = findSlotFile(slot, false);
    File r = findSlotFile(slot, true);
    putone(s, name, value);
    putone(r, value, name);
  }


  public synchronized void unassert(String name, String slot, String value)
    throws IOException
  {
    File s = findSlotFile(slot, false);
    File r = findSlotFile(slot, true);
    nukeone(s, name, value);
    nukeone(r, value, name);
  }

  public synchronized String findFirst(String name, String slot,
                                       boolean reverse) throws IOException {
    File f = new File(findSlotFile(slot, reverse), name);
    if (!f.exists()) return null;
    RandomAccessFile fid = new RandomAccessFile(f, "r");
    String line = null;
    if (fid.length() > 0) {
      line = fid.readUTF();
    }
    fid.close();
    return line;
  }



  public Enumeration findAll(String name, String slot, boolean reverse)
    throws IOException
  {
    File f = new File(findSlotFile(slot, reverse), name);
    if (!f.exists()) return NullJavaEnumeration.kInstance;
    final RandomAccessFile thefid = new RandomAccessFile(f, "r");
    return new Enumeration() {
      RandomAccessFile fid = thefid;
      String next = null;
      public boolean hasMoreElements() {
        if (next != null) return true;
        if (fid == null) return false;
        try {
          next = fid.readUTF();
        } catch (IOException e) {
          next = null;
        }
        if (next == null) {
          try {
            fid.close();
          } catch (IOException e) {
          }
          fid = null;
          return false;
        }
        return true;
      }
      public Object nextElement() throws NoSuchElementException {
        if (!hasMoreElements()) throw new NoSuchElementException();
        String result = next;
        next = null;
        return result;
      }
    };
  }


};
