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
 * Created: Jamie Zawinski <jwz@netscape.com>, 28 Sep 1997.
 */

package grendel.storage;

import calypso.util.Assert;
import java.io.File;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.io.IOException;
import java.io.EOFException;
import java.util.Hashtable;
import java.util.Enumeration;

abstract class MailSummaryFile {

  /** The folder and associated disk file.  The file might not exist. */
  protected BerkeleyFolder folder;
  protected File file;  // note this is the *folder*, not the *summary*.

  /* This holds the date of the file <I>when it was parsed.</I>
     We need to save this (older) values so that we can tell when
     the summary file is out of date.  This is measured in the units
     of File.lastModified(). */
  protected long file_date;

  /* This holds the size of the file <I>when it was parsed.</I>
     We need to save this (older) values so that we can tell when
     the summary file is out of date. */
  protected long file_size;

  /* The total number of messages, non-deleted messages, and unread messages
     in the folder, assuming that file_date and file_size match reality. */
  protected int total_message_count = -1;
  protected int undeleted_message_count = -1;
  protected int unread_message_count = -1;
  protected long deleted_message_bytes = -1;

  /** For salvageMessages().  If the table is non-null, its keys will be
      MessageID objects, and the values will be opaque objects intended
      to be passed to salvageMessageSummary().  Subclasses are responsible
      for populating this table, if they are able.
    */
  protected Hashtable salvage = null;


  MailSummaryFile(BerkeleyFolder f) {
    folder = f;
    file = folder.getFile();
    file_size = file.length();
    file_date = file.lastModified();
  }

  /** Parses a summary file for the associated folder, and adds the described
      message objects to the folder.  The given stream is assumed to have
      been opened, and positioned past any version-identifying header info.
      Returns the byte position at which parsing should resume (that is, the
      last byte position which the summary file summarizes.)
   */
  abstract long readSummaryFile(InputStream sum) throws IOException;

  /** Whether this class knows how to write a summary file. */
  boolean writable() { return false; }

  /** Write a summary file for the associated folder.  If a subclass provides
      an implementation for this method, it should also override writable().
    */
  void writeSummaryFile() throws IOException {
    throw new Error("unimplemented");
  }


  /** Returns the date that the summary file expects to find on the
      folder's file.  This is measured in the units of File.lastModified().
      Set this expectation with setFolderDateAndSize().
    */
  long folderDate() {
    return file_date;
  }

  /** Returns the size in bytes that the summary file expects of the
      folder's file.  Set this expectation with setFolderDateAndSize().
    */
  long folderSize() {
    return file_size;
  }

  /** Returns the total number of messages in the folder.
      This includes deleted and unread messages.
    */
  int totalMessageCount() {
    if (total_message_count == -1)
      synchronized (this) {
        if (total_message_count == -1)  // double check
          getMessageCounts();
      }
    return total_message_count;
  }

  /** Returns the number of non-deleted messages in the folder.
      This includes unread messages.
    */
  int undeletedMessageCount() {
    if (undeleted_message_count == -1)
      synchronized (this) {
        if (undeleted_message_count == -1)  // double check
          getMessageCounts();
      }
    return undeleted_message_count;
  }

  /** Returns the number of unread messages in the folder.
      This does not include unread messages that are also deleted.
    */
  int unreadMessageCount() {
    if (unread_message_count == -1)
      synchronized (this) {
        if (unread_message_count == -1)  // double check
          getMessageCounts();
      }
    return unread_message_count;
  }


  /** Returns the number of bytes consumed by deleted but not expunged
      messages in the folder. */
  long deletedMessageBytes() {
    if (deleted_message_bytes == -1)
      synchronized (this) {
        if (deleted_message_bytes == -1)  // double check
          getMessageCounts();
      }
    return deleted_message_bytes;
  }

  /** Read only enough of the file to fill in the values of
      total_message_count, undeleted_message_count, unread_message_count,
      and deleted_message_bytes. */
  abstract protected void getMessageCounts();


  /** Set the date and size of the corresponding folder-file.
      This date/time pair should correspond to the file at the time at
      which it was <I>parsed</I>, not the current time.
      The next time writeSummaryFile() is called, the new summary
      file will encode this date and size expectation.
   */
  void setFolderDateAndSize(long file_date, long file_size) {
    this.file_date = file_date;
    this.file_size = file_size;
  }


  /** Set the message counts (total, undeleted, and unread) of the
      corresponding folder-file.  The next time writeSummaryFile() is
      called, the new summary file will include these numbers.

      @param total_count      The total number of messages in the folder.
                              This must match the number of elements returned
                              by getMessages() at the time that
                              writeSummaryFile() is called.

      @param undeleted_count  The total number of messages in the folder
                              that are not marked as deleted.  This also
                              must match reality when writeSummaryFile() is
                              called.

      @param unread_count     The total number of messages in the folder
                              that are not marked as either read or deleted.
                              Ditto writeSummaryFile().

      @param deleted_bytes    The total number of bytes consumed by messages
                              that are marked as deleted.
                              Ditto writeSummaryFile().
   */
  void setFolderMessageCounts(int total_count, int undeleted_count,
                              int unread_count, long deleted_bytes) {
    Assert.Assertion(total_count >= 0);
    Assert.Assertion(undeleted_count >= 0);
    Assert.Assertion(unread_count >= 0);
    Assert.Assertion(deleted_bytes >= 0);
    Assert.Assertion(undeleted_count <= total_count);
    Assert.Assertion(unread_count <= total_count);

    this.total_message_count = total_count;
    this.undeleted_message_count = undeleted_count;
    this.unread_message_count = unread_count;
    this.deleted_message_bytes = deleted_bytes;
  }


  /** Called when the folder's disk file has been appended to by *this*
      program.  (As opposed to, an unexpected, unknown change by some other
      program.)  This calls setFolderDateAndSize() with the current date
      and size of the folder file (as it appears on disk right now.)

      <P> If the number of messages has changed, you should have called
      setFolderMessageCounts() before calling this.
    */
  void updateSummaryFile() throws IOException {
    setFolderDateAndSize(file.lastModified(), file.length());
    // by default, do nothing to the disk file.
  }

  /** Compare the date of the folder disk file to a date (presumably) read
      from the summary file, and return true of they are equal.
   */
  protected boolean checkFolderDate(long summary_date) {
    long folder_date = file.lastModified();

    // #### System dependency: to be compatible with the Cheddar file format,
    //      we need to be able to compare lastModified() with the time_t
    //      value in the file (which came from `stat.st_mtime'.)
    //
    // Note that, to be compatible with this, MailSummaryFileGrendel divides
    // the date by 1000 when saving it to the file.
    //
    summary_date *= 1000L;

    return (folder_date == summary_date);
  }

  /** Compare the size of the folder disk file to a size (presumably) read
      from the summary file, and return true of they are equal.
   */
  protected boolean checkFolderSize(long summary_size) {
    return (summary_size == file.length());
  }


  /** Call this if something has gone wrong when parsing the summary file,
      but <I>after</I> the folder file has been parsed.  This might cause
      some additional information to be scavenged from the summary file
      and stored into the messages.

      <P> The idea here is that an out-of-date summary file cannot be
      trusted as far as byte-offsets and such goes, but if the summary
      file had stored some property of a message (such as whether it was
      deleted) then we might as well believe that, lacking any other
      source of information.

      <P> Note that one folder can contain multiple messages with the same
      ID, so this process is not totally foolproof.  It is, however,
      better than nothing.
    */
  void salvageMessages() {
    if (salvage == null) return;
    Hashtable t = salvage;
    salvage = null;
    if (folder.fMessages != null) {
      for (Enumeration e = folder.fMessages.elements(); e.hasMoreElements();) {
        BerkeleyMessage m = (BerkeleyMessage) e.nextElement();
        MessageID id = (MessageID) m.getMessageID();
        if (id == null) continue;
        Object value = t.get(id);
        if (value != null)
          salvageMessage(m, value);
      }
    }
  }

  protected void salvageMessage(BerkeleyMessage m, Object salvage_object) {
    // default is to do nothing.  Subclasses should implement this if
    // they can.
  }


  /* Some random I/O utilities... */


  protected synchronized int readUnsignedShort(InputStream s)
    throws IOException {
    int b0 = s.read(); if (b0 == -1) throw new EOFException();
    int b1 = s.read(); if (b1 == -1) throw new EOFException();
    return ((b0 << 8) | b1);
  }

  protected synchronized long readUnsignedInteger(InputStream s)
    throws IOException {
    int b0 = s.read(); if (b0 == -1) throw new EOFException();
    int b1 = s.read(); if (b1 == -1) throw new EOFException();
    int b2 = s.read(); if (b2 == -1) throw new EOFException();
    int b3 = s.read(); if (b3 == -1) throw new EOFException();
    return ((((long)b0) << 24) | (b1 << 16) | (b2 << 8) | b3);
  }

  protected synchronized void writeUnsignedShort(OutputStream s, int i)
    throws IOException {
    s.write((i >> 8) & 0xFF);
    s.write(i & 0xFF);
  }

  protected synchronized void writeUnsignedInteger(OutputStream s, long i)
    throws IOException {
    s.write((int) (i >> 24) & 0xFF);
    s.write((int) (i >> 16) & 0xFF);
    s.write((int) (i >> 8) & 0xFF);
    s.write((int) (i & 0xFF));
  }

  /* Java sucks!  I had to duplicate these because, as far as I can tell,
     there is no class or interface which FileInputStream and RandomAccessFile
     have in common, nor that FileOutputStream and RandomAccessFile have in
     common.  What a total crock!
   */

  protected synchronized int readUnsignedShort(RandomAccessFile s)
    throws IOException {
    int b0 = s.read(); if (b0 == -1) throw new EOFException();
    int b1 = s.read(); if (b1 == -1) throw new EOFException();
    return ((b0 << 8) | b1);
  }

  protected synchronized long readUnsignedInteger(RandomAccessFile s)
    throws IOException {
    int b0 = s.read(); if (b0 == -1) throw new EOFException();
    int b1 = s.read(); if (b1 == -1) throw new EOFException();
    int b2 = s.read(); if (b2 == -1) throw new EOFException();
    int b3 = s.read(); if (b3 == -1) throw new EOFException();
    return ((((long)b0) << 24) | (b1 << 16) | (b2 << 8) | b3);
  }

  protected synchronized void writeUnsignedShort(RandomAccessFile s, int i)
    throws IOException {
    s.write((i >> 8) & 0xFF);
    s.write(i & 0xFF);
  }

  protected synchronized void writeUnsignedInteger(RandomAccessFile s, long i)
    throws IOException {
    s.write((int) (i >> 24) & 0xFF);
    s.write((int) (i >> 16) & 0xFF);
    s.write((int) (i >> 8) & 0xFF);
    s.write((int) (i & 0xFF));
  }

}
