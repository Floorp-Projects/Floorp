/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 *
 * Created: Jamie Zawinski <jwz@netscape.com>, 10 Sep 1997.
 */

package grendel.storage;

import calypso.util.Assert;
import calypso.util.ByteBuf;
import calypso.util.ByteLineBuffer;

import java.lang.Thread;
import java.lang.SecurityException;
import java.lang.InterruptedException;

import java.util.Vector;
import java.util.Enumeration;

import java.io.File;
import java.io.OutputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.BufferedOutputStream;
import java.io.IOException;
import java.io.FileNotFoundException;


/** This class represents a newsrc-format disk file, and the newsgroup
    descriptions within it.
  */
public class NewsRC {

  static final boolean DEBUG = true;

  protected File file = null;   // disk file; might not exist, but never null.

  /* This holds the date of the newsrc file <I>when it was parsed.</I>
     We need to save this (older) values so that we can tell when
     the summary file is out of date.  This is measured in the units
     of File.lastModified(). */
  protected long file_date = -1;

  /* This holds the size of the file <I>when it was parsed.</I>
     We need to save this (older) values so that we can tell when
     the summary file is out of date. */
  protected long file_size = -1;

  /* Any "options" lines that appeared at the head of the newsrc file.
     We don't use them, but we preserve them if they exist. */
  protected ByteBuf options_lines = null;

  protected Vector lines = new Vector();        // holds NewsRCLines

  protected boolean dirty = false;              // needs to be saved to disk?

  Thread updateThread = null;                   // background save thread.


  /** Reads and parses the file, and returns a NewsRC object describing it.
      If the file doesn't exist, the newsrc object will be empty.
    */
  NewsRC(File file) throws IOException {
    this.file = file;
    load();
  }

  private synchronized void load() throws IOException {
    try {
      FileInputStream in = null;
      try {
        in = new FileInputStream(file);
        parse_file(in);
        file_size = file.length();
        file_date = file.lastModified();
        dirty = false;

      } finally {
        if (in != null)
          in.close();
      }
    } catch (FileNotFoundException e) {
      // do nothing -- it's ok for the file to not exist.
    }
  }


  /** Returns the file associated with this newsrc object.
      Note that the file may not actually exist on disk yet.
    */
  public File file() {
    Assert.Assertion(file != null);
    return file;
  }

  protected void parse_file(FileInputStream in) throws IOException {

    ByteLineBuffer b = new ByteLineBuffer();
    ByteBuf inbuf = new ByteBuf(1024);
    ByteBuf outbuf = new ByteBuf(100);
    boolean eof = false;

    lines.removeAllElements();

    while (!eof) {
      inbuf.setLength(0);
      int r = inbuf.read(in, 1024);
      if (r == -1) {
        eof = true;
        b.pushEOF();
      } else {
        b.pushBytes(inbuf);
      }

      while (b.pullLine(outbuf)) {
        NewsRCLine line = makeNewsRCLine(outbuf);
        if (line == null) {
          if (options_lines == null)
            options_lines = new ByteBuf(outbuf.length());
          options_lines.append(outbuf);
        } else {
          lines.addElement(line);
        }
      }
    }
  }

  /** Call this to indicate that the file needs to be flushed to disk.
   */
  public synchronized void markDirty() {

    if (dirty) return;

    if (DEBUG)
      System.err.println("Marking newsrc file " + file + " dirty.");

    boolean was_dirty = dirty;
    dirty = true;
    verifyFileDate(was_dirty);

    if (!dirty) return;  // verifyFileDate() might have saved or reloaded.

    if (updateThread == null || !updateThread.isAlive()) {

      if (DEBUG)
        System.err.println("spawning newsrc background saver for " + file);

      updateThread = new NewsRCUpdateThread(this);
      updateThread.start();
    }
  }

  /** Parses the given line and creates a NewsRCLine object representing it.
      Returns null if the line is unparsable.
    */
  protected NewsRCLine makeNewsRCLine(ByteBuf line_buf) {
    byte line[] = line_buf.toBytes();
    int length = line_buf.length();
    boolean subscribed = false;
    int i = 0;
    while (i < length && line[i] <= (byte)' ') i++;    // skip whitespace

    int start = i;
    while (i < length &&                        // skip non-whitespace, but
           line[i] > (byte)' ' &&                     // not the two magic chars
           line[i] != (byte)':' &&
           line[i] != (byte)'!')
      i++;

    if (i == start) return null;

    String group_name = new String(line, start, i);

    while (i < length && line[i] <= (byte)' ') i++;    // skip whitespace

    if (i < length && line[i] == (byte)':')
      subscribed = true;
    else if (i < length && line[i] == (byte)'!')
      subscribed = false;
    else
      return null;

    i++;
    while (i < length && line[i] <= (byte)' ') i++;    // skip whitespace
    return new NewsRCLine(this, group_name, subscribed, line, i, length);
  }


  /* Returns an Enumeration of the NewsRCLines in this NewsRC. */
  public Enumeration elements() {
    return lines.elements();
  }

  /** Returns a NewsRCLine describing the named newsgroup.
      If there isn't one, creates one.
   */
  public NewsRCLine getNewsgroup(String group_name) {
    for (Enumeration e = lines.elements(); e.hasMoreElements(); ) {
      NewsRCLine n = (NewsRCLine) e.nextElement();
      if (group_name.equals(n.name()))
        return n;
    }

    NewsRCLine n = new NewsRCLine(this, group_name, true);
    lines.addElement(n);
    return n;
  }

  public void write(OutputStream stream) throws IOException {

    synchronized (stream) {

      if (options_lines != null) {
        stream.write(options_lines.toBytes(), 0,
                     options_lines.length());
      }
      ByteBuf b = new ByteBuf();

      final byte[] CR = { (byte)'\r' };
      final byte[] LF = { (byte)'\n' };
      final byte[] CRLF = { (byte)'\r', (byte)'\n' };
      byte linebreak[] = (Constants.ISUNIX ? LF : Constants.ISMAC ? CR : CRLF);

      for (Enumeration e = lines.elements(); e.hasMoreElements(); ) {
        NewsRCLine n = (NewsRCLine) e.nextElement();
        b.setLength(0);
        n.write(b);
        b.append(linebreak);
        stream.write(b.toBytes(), 0, b.length());
      }
    }
  }


  /** Returns the name of a non-existent file that can be used as a temporary
      file while writing this newsrc.  This name is OS-dependent.
      <P>
      On all systems, the summary file resides in the same directory
      as the folder file, and has the same "base" name.
      <P>
      On Unix, the summary file for <TT>.newsrc-HOSTNAME</TT> would be
      <TT>.newsrc-HOSTNAME.ns_tmp</TT>.
      <P>
      On Windows and Mac, the summary file for <TT>news-HOSTNAME.rc</TT>
      would be <TT>news-HOSTNAME.TMP</TT>.
    */
  protected File tempFileName() {
    File file = file();
    String dir = file.getParent();
    String name = file.getName();

    if (Constants.ISUNIX) {
      name = name + ".ns_tmp";
    } else {
      int i = name.lastIndexOf(".");
      if (i > 0)
        name = name.substring(0, i);
      name = name + ".tmp";
    }

    return new File(dir, name);
  }


  /** Writes the file associated with this NewsRC object, if there have
      been any changes to it.
   */
  public void save() throws IOException {
    if (!dirty)
      return;
    synchronized (this) {
      if (!dirty)
        return;
      verifyFileDate(true);
      save_internal();
    }
  }

  protected void save_internal() throws IOException {

    if (!dirty)
      return;
    synchronized (this) {
      if (!dirty)                 // double check
        return;

      File file = file();
      File tmp = tempFileName();
      OutputStream out = null;

      try {                       // make sure the temp file gets deleted.

        try {                     // make sure the stream gets closed.
          out = new FileOutputStream(tmp);
          out = new BufferedOutputStream(out);

          if (DEBUG) {
            System.err.println("Writing tmp newsrc file " + tmp);
          }

          write(out);
        } finally {               // make sure the stream gets closed.
          if (out != null) {
            out.close();
            out = null;
          }
        }

        tmp.renameTo(file);

        file_size = file.length();
        file_date = file.lastModified();

        dirty = false;

        if (DEBUG) {
          System.err.println("Renaming tmp newsrc file " + tmp + " to " +
                             file);
        }

        tmp = null;

      } finally {                 // make sure the temp file gets deleted.
        if (tmp != null)
          try {
            tmp.delete();
          } catch (SecurityException e) {
          }
      }
    }
  }


  /** Compare the current date/size of the disk file with the date/size at
      the time we read it, and if the disk version has changed, do something
      sensible.
   */
  protected synchronized void verifyFileDate(boolean was_dirty) {
    long cur_size = -1;
    long cur_date = -1;

    if (file.exists()) {
      cur_size = file.length();
      cur_date = file.lastModified();
    }

    if (cur_date != file_date || cur_size != file_size) {

      if (file_date != -1 && cur_date == -1) {
        // file used to exist, and has been deleted...
        System.err.println("WARNING: " + file +
                           " was deleted out from under us!  Saving now...");
        dirty = true;

        try {
          save_internal();
        } catch (IOException e) {
          System.err.println("ERROR!  Saving " + file + ": " + e);
        }

      } else {
        // either: the file didn't exist and now it does;
        // or it already existed, but has changed since we read it.

        // #### dialog box or something
        synchronized(System.err) {
          System.err.println("WARNING: " + file + " has changed!");
          if (was_dirty)
            System.err.println("         Discarding our changes and " +
                               "reloading the disk version...");
          else
            System.err.println("         Reloading the disk version...");
        }

        try {
          load();
        } catch (IOException e) {
          System.err.println("ERROR!  Reloading " + file + ": " + e);
        }

        // #### Perhaps here we should generate some kind of listener event,
        //      to let the folder list know that the unread message counts
        //      may have changed?
      }
    }
  }


  public static void main(String argv[]) {
    try {
      NewsRC rc = new NewsRC(new File(argv[0]));
      if (rc.options_lines != null)
        System.out.print(rc.options_lines);
      for (Enumeration e = rc.lines.elements(); e.hasMoreElements(); )
        System.out.println(e.nextElement());
    } catch (IOException e) {
      System.err.println(e);
    }
  }
}



/** This class implements a thread which runs in the background and saves a
    modified newsrc file.  It dies once the file has been updated; it will be
    re-spawned if that newsrc file becomes dirty again.

    <P> There can be one of these threads per NewsRC object.
  */
class NewsRCUpdateThread extends Thread {

  static int delay = 30; // seconds
  NewsRC rc = null;

  NewsRCUpdateThread(NewsRC rc) {
    this.rc = rc;
    setDaemon(true);
    setName(getClass().getName() + " " + rc.file());
  }

  public void run() {

    try {
      sleep(delay * 1000);
    } catch (InterruptedException e) {
      return;   // is this the right way to do this?
    }

    if (NewsRC.DEBUG)
      System.err.println("background thread saving newsrc file " + rc.file());

    try {
      synchronized (rc) {
        rc.updateThread = null;
        rc.save();
      }
    } catch (IOException e) {
      // nothing to be done?
      System.err.println("IOException while saving newsrc file " + rc.file());
    }
  }
}
