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

import calypso.util.ByteBuf;
import calypso.util.Assert;
import java.util.Vector;
import java.util.Enumeration;
import java.util.Hashtable;
import java.io.File;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.BufferedInputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.BufferedOutputStream;
import java.io.RandomAccessFile;
import java.io.IOException;

/** This class knows how to read and write a mail folder summary file.
    This is a different format than that used by any previous version of
    Mozilla; it's similar to the 2.0/3.0 format, and totally unlike the
    4.0/5.0 format.
  */

/* The format is as follows:

   MAGIC_NUMBER VERSION FOLDER_SIZE FOLDER_DATE PARSED_THRU NMSGS NVALIDMSGS
    NUNREADMSGS DELETED_BYTES STRING_TABLE ID_TABLE [ MSG_DESC ]*

   MAGIC_NUMBER   := '# Netscape folder cache\r\n'
   VERSION        := uint32 (file format version number; this is version 6)

   FOLDER_SIZE    := uint32 (the size of the folder itself, so that we can
                     easily detect an out-of-date summary file)
   FOLDER_DATE    := uint32 (time_t of the folder; same purpose as above)
   PARSED_THRU    := uint32 (the last position in the folder that we have
                             parsed data for.  Normally the same as
                             FOLDER_SIZE, but if we append messages to a file,
                             we can just update FOLDER_SIZE and FOLDER_SIZE and
                             use this field to tell us what part of the
                             folder needs to be parsed.)
   NMSGS          := uint32 (total number of messages in this folder, including
                             expunged messages)
   NVALIDMSGS     := uint32 (total number of non-expunged messages in this
                             folder)
   NUNREADMSGS    := uint32 (total number of unread messages in this folder)
   DELETED_BYTES  := uint32 (number of bytes of messages which have been
                     deleted from the summary, but not yet compacted out
                     of the folder file itself.  The messages in question
                     have the DELETED bit set in their flags.)

   STRING_TABLE   := ST_LENGTH [ ST_STRING ]+
   ST_LENGTH      := uint32
   ST_STRING      := null terminated string

   ID_TABLE       := ID_LENGTH [ MSG_ID ]+
   ID_LENGTH      := uint32
   MSG_ID         := uint64 (64-bit hash of a Message-ID string)

   MSG_DESC       := AUTHOR RECIPIENT SUBJECT DATE ID REFERENCES
                     FLAGS POS LENGTH
   AUTHOR         := SINDEX (index into STRING_TABLE)
   RECIPIENT      := SINDEX (index into STRING_TABLE)
   SUBJECT        := SINDEX (index into STRING_TABLE)
   DATE           := uint32 (time_t)
   FLAGS          := uint32 (bitfield of other attributes of the message;
                     this is in `X-Mozilla-Status' format.)
   POS            := uint32 (byte index into file)
   LENGTH         := uint32 (length in bytes from envelope to end of last line)
   ID             := IINDEX (index into ID_TABLE)
   REFERENCES     := RLENGTH [ ID ]*
   RLENGTH        := uint16

   SINDEX         := uint16 -or- uint32 (depending on whether ST_LENGTH is
                     able to be represented with 16 bits.)
   IINDEX         := uint16 -or- uint32 (same, but for ID_LENGTH.)
 */

class MailSummaryFileGrendel extends MailSummaryFile {

  MailSummaryFileGrendel(BerkeleyFolder folder) {
    super(folder);
  }

  /** Parses a Grendel (version 6) mail summary file, and adds the described
      messages to the associated Folder object.
   */
  synchronized long readSummaryFile(InputStream sum) throws IOException {

    // At the point at which this method is called, MAGIC_NUMBER and VERSION
    // have already been read.

    long folder_size   = readUnsignedInteger(sum);
    long folder_date   = readUnsignedInteger(sum);
    long old_thru      = readUnsignedInteger(sum);  // old parsed_thru
    long nmsgs         = readUnsignedInteger(sum);
    long nvalidmsgs    = readUnsignedInteger(sum);
    long nunreadmsgs   = readUnsignedInteger(sum);
    long deletedbytes  = readUnsignedInteger(sum);

    boolean pessimistic = false;

    if (!checkFolderDate(folder_date)) {
      System.err.println("warning: folder " + folder.getName() +
                         " is newer than summary file" +
//                         " (" + this.file_date + ", " +
//                         folder_date*1000L + ")" +
                         "; attempting salvage.");
      pessimistic = true;

    } else if (!checkFolderSize(folder_size)) {
      System.err.println("warning: folder " + folder.getName() + " is not " +
                         folder_size + " bytes, as summary file expected; " +
                         "\n\t attempting salvage.");
      pessimistic = true;
    }


    long string_table_length = readUnsignedInteger(sum);
    ByteBuf string_table[] = (pessimistic
                              ? null
                              : new ByteBuf[(int) string_table_length]);

    // Read N null-terminated strings.
    for (int i = 0; i < string_table_length; i++) {
      ByteBuf bb = new ByteBuf();
      for (int j = 0;; j++) {
        int b = sum.read();
        if (b == 0) break;
        if (b == -1) throw new IOException();
        bb.append((byte) b);
      }
      if (!pessimistic)
        string_table[i] = bb;
    }

    long id_table_length = readUnsignedInteger(sum);
    MessageID id_table[] = new MessageID[(int) id_table_length];

    // Read N 64-bit numbers (hashes of the Message-IDs.)
    for (int i = 0; i < id_table_length; i++) {
      long b0 = readUnsignedInteger(sum);
      long b1 = readUnsignedInteger(sum);
      id_table[i] = new MessageID((b0 << 32) | b1);
    }

    Vector msgs = (pessimistic
                   ? null
                   : new Vector((int) nmsgs));

    if (pessimistic)
      salvage = new Hashtable((int) nmsgs);

    boolean success = false;
    try {       // if we fail for some reason, set up the `salvage' table

      // Read N message descriptors.
      for (int i = 0; i < nmsgs; i++) {

        long author, recipient, subject, id;
        if (string_table_length <= 0xFFFF) {
          author    = readUnsignedShort(sum);
          recipient = readUnsignedShort(sum);
          subject   = readUnsignedShort(sum);
        } else {
          author    = readUnsignedInteger(sum);
          recipient = readUnsignedInteger(sum);
          subject   = readUnsignedInteger(sum);
        }
        long date   = readUnsignedInteger(sum);
        long flags  = readUnsignedInteger(sum);
        long pos    = readUnsignedInteger(sum);
        long length = readUnsignedInteger(sum);
        if (id_table_length <= 0xFFFF) {
          id        = readUnsignedShort(sum);
        } else {
          id        = readUnsignedInteger(sum);
        }

        int rlength = readUnsignedShort(sum);
        MessageID refs[] = null;

        if (rlength > 0) {
          refs = new MessageID[rlength];
          if (id_table_length <= 0xFFFF) {
            for (int j = 0; j < rlength; j++)
              refs[j] = id_table[readUnsignedShort(sum)];
          } else {
            for (int j = 0; j < rlength; j++)
              refs[j] = id_table[(int) readUnsignedInteger(sum)];
          }
        }

        if (pessimistic) {
          refs = null;
          salvage.put(id_table[(int) id], new Long(flags));
        } else {
          flags = BerkeleyMessage.mozillaFlagsToInternalFlags(flags);
          BerkeleyMessage m =
            new BerkeleyMessage(folder,
                                date * 1000L,
                                flags,
                                string_table[(int) author],
                                string_table[(int) recipient],
                                string_table[(int) subject],
                                id_table[(int) id],
                                refs);
          refs = null;
          m.setStorageFolderIndex((int) pos);
          m.setSize((int) length);
          msgs.addElement(m);
        }
      }

      success = true;

    } finally {
      string_table = null;
      id_table = null;

      if (!success &&
          msgs != null) {
        // We got some kind of error while reading the message data.
        // The `msgs' vector contains the messages we were able to parse,
        // but since we got an error, we're not going to actually add them
        // to the folder.  But, let's pull the flags out of the messages
        // and populate `salvage' with it (see doc of salvageMessages()
        // for rationale.)
        //
        // Note that if we realized very early that this file was out of
        // date, salvage will have already been populated, and `msgs'
        // will be null.  This branch is taken only if the file at first
        // appears to be up to date, but later turns out to be corrupted
        // or truncated.
        //
        if (salvage == null)
          salvage = new Hashtable((int) nmsgs);
        for (Enumeration e = msgs.elements(); e.hasMoreElements();) {
          BerkeleyMessage m = (BerkeleyMessage) e.nextElement();
          salvage.put(m.getMessageID(),
                      new Long(m.internalFlagsToMozillaFlags(m.flags)));
        }
      }
    }

    if (pessimistic)
      // Ok, we got what we needed.  Tell the caller to go and reparse
      // the folder.  It will then call salvageMessages() which will
      // pull stuff out of the hash table we just populated.
      return 0;

    this.total_message_count = (int) nmsgs;
    this.undeleted_message_count = (int) nvalidmsgs;
    this.unread_message_count = (int) nunreadmsgs;
    this.deleted_message_bytes = deletedbytes;

    // Successfully parsed it all; feed the messages into the folder.
    for (Enumeration e = msgs.elements(); e.hasMoreElements();)
      folder.noticeInitialMessage((BerkeleyMessage) e.nextElement());
    msgs = null;

    // Return the byte at which parsing in the folder should begin
    // (the last byte covered by this summary file.)
    return old_thru;
  }


  /** Assumes the salvage table contains Long values which are flags
      in X-Mozilla-Status form. */
  protected void salvageMessage(BerkeleyMessage m, Object salvage_object) {
    long flags = ((Long)salvage_object).longValue();
    m.flags |= m.mozillaFlagsToInternalFlags(flags);
  }


  /** Read only enough of the file to fill in the values of
      total_message_count, undeleted_message_count, unread_message_count,
      and deleted_message_bytes. */
  synchronized protected void getMessageCounts() {
    InputStream in = null;
    try {
      File sum = folder.summaryFileName();
      in = new BufferedInputStream(new FileInputStream(sum));

      // header
      for (int i = 0;
           i < MailSummaryFileFactory.SUMMARY_MAGIC_NUMBER.length;
           i++) {
        if (in.read() != MailSummaryFileFactory.SUMMARY_MAGIC_NUMBER[i])
          return;
      }

      // version number
      if (readUnsignedInteger(in) != MailSummaryFileFactory.VERSION_GRENDEL)
        return;

      readUnsignedInteger(in);  // FOLDER_SIZE
      readUnsignedInteger(in);  // FOLDER_DATE
      readUnsignedInteger(in);  // PARSED_THRU

      total_message_count     = (int) readUnsignedInteger(in);  // NMSGS
      undeleted_message_count = (int) readUnsignedInteger(in);  // NVALIDMSGS
      unread_message_count    = (int) readUnsignedInteger(in);  // NUNREADMSGS
      deleted_message_bytes   = readUnsignedInteger(in);  // DELETED_BYTES

    } catch (IOException e) {
      // ignore it.
    } finally {
      if (in != null)
        try {
          in.close();
        } catch (IOException e) {
        }
    }
  }



  /** Returns true, since this class implements writeSummaryFile(). */
  boolean writable() { return true; }

  /** Write a summary file for the associated folder, in the Grendel format
      (version 6.)
    */
  void writeSummaryFile() throws IOException {
    File sum = folder.summaryFileName();
    OutputStream out = null;
    boolean success = false;
    try {
      out = new BufferedOutputStream(new FileOutputStream(sum));
      writeSummaryData(out);
      success = true;
    } finally {
      // always close the file.
      // if there was an error, delete the partial file we just wrote.
      if (out != null) {
        out.close();
        if (!success)
          sum.delete();
      }
    }
  }

  /* subroutine of writeSummaryFile, broken out for clarity. */
  protected void writeSummaryData(OutputStream out)
    throws IOException, SecurityException {

    Vector msgs = folder.fMessages;
    int nmsgs = msgs.size();
    int nvalidmsgs = 0;
    int nunreadmsgs = 0;
    int deletedbytes = 0;

    // Walk over the messages to count up the unread, deleted, and bytes.
    for (int i = 0; i < nmsgs; i++) {
      BerkeleyMessage m = (BerkeleyMessage) msgs.elementAt(i);
      if (m.isDeleted()) {
        deletedbytes += m.fLength;
      } else {
        nvalidmsgs++;
        if (!m.isRead())
          nunreadmsgs++;
      }
    }

    // These *should* be in sync, but if they're not, correct them.
/*
    Assert.Assertion(nmsgs == this.total_message_count);
    Assert.Assertion(nvalidmsgs == this.undeleted_message_count);
    Assert.Assertion(nunreadmsgs == this.unread_message_count);
    Assert.Assertion(deletedbytes == this.deleted_message_bytes);
*/
    this.total_message_count = nmsgs;
    this.undeleted_message_count = nvalidmsgs;
    this.unread_message_count = nunreadmsgs;
    this.deleted_message_bytes = deletedbytes;


    ByteStringTable string_table = folder.getStringTable();
    MessageIDTable id_table = folder.getMessageIDTable();

    int string_table_length = string_table.size();
    int id_table_length = id_table.size();

    out.write(MailSummaryFileFactory.SUMMARY_MAGIC_NUMBER);
    writeUnsignedInteger(out, MailSummaryFileFactory.VERSION_GRENDEL);

    writeUnsignedInteger(out, this.file_size);
    writeUnsignedInteger(out, (this.file_date / 1000L));
    writeUnsignedInteger(out, this.file_size);   // parsed_thru
    writeUnsignedInteger(out, nmsgs);
    writeUnsignedInteger(out, nvalidmsgs);
    writeUnsignedInteger(out, nunreadmsgs);
    writeUnsignedInteger(out, deletedbytes);

    writeUnsignedInteger(out, string_table_length + 1);
    out.write(0);  // first elt in table is a null string, for simplicity
    for (int i = 0; i < string_table_length; i++) {
      ByteString s = (ByteString) string_table.getObject(i);
      out.write(s.toBytes());
      out.write(0);
    }

    writeUnsignedInteger(out, id_table_length);
    for (int i = 0; i < id_table_length; i++) {
      MessageID id = (MessageID) id_table.getObject(i);
      long h = id.hash;
      writeUnsignedInteger(out, h >> 32);
      writeUnsignedInteger(out, h & 0xFFFFFFFF);
    }

    for (int i = 0; i < nmsgs; i++) {

      BerkeleyMessage m = (BerkeleyMessage) msgs.elementAt(i);
      int author_name = m.author_name + 1;
      int recipient_name = m.recipient_name + 1;
      int subject = m.subject + 1;

      if (string_table_length <= 0xFFFF) {
        writeUnsignedShort(out, author_name);
        writeUnsignedShort(out, recipient_name);
        writeUnsignedShort(out, subject);
      } else {
        writeUnsignedInteger(out, author_name);
        writeUnsignedInteger(out, recipient_name);
        writeUnsignedInteger(out, subject);
      }

      writeUnsignedInteger(out, (m.sentDate / 1000L));
      writeUnsignedInteger(out, m.internalFlagsToMozillaFlags(m.flags));
      writeUnsignedInteger(out, m.getStorageFolderIndex());
      writeUnsignedInteger(out, m.getSize());

      if (id_table_length <= 0xFFFF) {
        writeUnsignedShort(out, m.message_id);
      } else {
        writeUnsignedInteger(out, m.message_id);
      }

      int refs[] = m.references;
      if (refs == null) {
        writeUnsignedShort(out, 0);
      } else {
        int rlength = refs.length;
        writeUnsignedShort(out, rlength);
        if (id_table_length <= 0xFFFF) {
          for (int j = 0; j < rlength; j++)
            writeUnsignedShort(out, refs[j]);
        } else {
          for (int j = 0; j < rlength; j++)
            writeUnsignedInteger(out, refs[j]);
        }
      }
    }
  }

  /** Called when the folder's disk file has been appended to by <I>this</I>
      program.  (As opposed to, an unexpected, unknown change by some other
      program.)  This overwrites certain fields in a Grendel-format summary
      file.  Those fields are:
      <UL>
        <LI> FOLDER_SIZE
        <LI> FOLDER_DATE
        <LI> NMSGS
        <LI> NVALIDMSGS
        <LI> NUNREADMSGS
        <LI> DELETED_BYTES
      </UL>
    */
  synchronized void updateSummaryFile() throws IOException {
    super.updateSummaryFile();

    RandomAccessFile io = null;
    try {
      io = new RandomAccessFile(folder.summaryFileName(), "rw");
      boolean ok = true;

      // First, ensure the file is in the format we expect it to be in.
      //
      for (int i = 0;
           i < MailSummaryFileFactory.SUMMARY_MAGIC_NUMBER.length;
           i++)
        if (io.read() != MailSummaryFileFactory.SUMMARY_MAGIC_NUMBER[i]) {
          ok = false;
          break;
        }

      if (ok && (readUnsignedInteger(io) !=
                 MailSummaryFileFactory.VERSION_GRENDEL))
        ok = false;

      if (ok) {
        // We are now positioned just before the FOLDER_SIZE and FOLDER_DATE
        // slots.  Overwrite them with the current size/date of the file.
        // (Note that super.updateSummaryFile() called setFolderDateAndSize().)
        writeUnsignedInteger(io, this.file_size);
        writeUnsignedInteger(io, (this.file_date / 1000L));

        // Skip over the PARSED_THRU token
        io.seek(io.getFilePointer() + 4);

        // Now overwrite the NMSGS, NVALIDMSGS and NUNREADMSGS tokens.
        writeUnsignedInteger(io, this.total_message_count);
        writeUnsignedInteger(io, this.undeleted_message_count);
        writeUnsignedInteger(io, this.unread_message_count);
        writeUnsignedInteger(io, this.deleted_message_bytes);
      }

    } finally {
      if (io != null)
        io.close();
    }
  }
}
