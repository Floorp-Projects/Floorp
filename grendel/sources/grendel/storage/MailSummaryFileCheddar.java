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
import java.util.Vector;
import java.util.Enumeration;
import java.util.Hashtable;
import java.io.File;
import java.io.InputStream;
import java.io.IOException;
import java.io.BufferedInputStream;
import java.io.FileInputStream;
import java.io.IOException;

/** This class knows how to read a Netscape 2.0 / 3.0 mail summary file.
    It does not know how to write them, because our new internal representation
    of messages doesn't keep around enough information to regenerate the file
    compatibly (doing so would consume a lot more memory.)
  */

/* The summary format, as documented in ns/lib/libmsg/mailsum.h:

   MAGIC_NUMBER VERSION FOLDER_SIZE FOLDER_DATE PARSED_THRU NMSGS NVALIDMSGS
    NUNREADMSGS DELETED_BYTES STRING_TABLE [ MSG_DESC ]*

   MAGIC_NUMBER   := '# Netscape folder cache\r\n'
   VERSION        := uint32 (file format version number; this is version 4)

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
                             deleted messages)
   NVALIDMSGS     := uint32 (total number of non-deleted messages in this
                             folder)
   NUNREADMSGS    := uint32 (total number of unread messages in this folder)
   DELETED_BYTES  := uint32 (number of bytes of messages which have been
                     deleted from the summary, but not yet compacted out
                     of the folder file itself.  The messages in question
                     have the DELETED bit set in their flags.)

   STRING_TABLE   := ST_LENGTH [ ST_STRING ]+
   ST_LENGTH      := uint16
   ST_STRING      := null terminated string

   MSG_DESC       := SENDER RECIPIENT SUBJECT DATE LINES ID REFERENCES
                     FLAGS POS LENGTH STATUSINDEX
   SENDER         := uint16 (index into STRING_TABLE)
   RECIPIENT      := uint16 (index into STRING_TABLE)
   SUBJECT        := uint16 (index into STRING_TABLE)
   DATE           := uint32 (time_t)
   FLAGS          := uint32 (bitfield of other attributes of the message;
                     this is in `X-Mozilla-Status' format.)
   POS            := uint32 (byte index into file)
   LENGTH         := uint32 (length in bytes from envelope to end of last line)
   STATUSOFFSET   := uint16 (Offset from beginning of message envelope for
                             X-Mozilla-Status header)
   LINES          := uint16 (length in lines; but I think this was unused...)
   ID             := uint16 (index into STRING_TABLE)
   REFERENCES     := RLENGTH [ ID ]*
   RLENGTH        := uint16
 */

class MailSummaryFileCheddar extends MailSummaryFile {

  MailSummaryFileCheddar(BerkeleyFolder folder) {
    super(folder);
  }

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
    int table_length   = readUnsignedShort(sum);

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

    ByteBuf table[] = new ByteBuf[table_length];
    // Read N null-terminated strings.
    for (int i = 0; i < table_length; i++) {
      ByteBuf bb = new ByteBuf();
      for (int j = 0;; j++) {
        int b = sum.read();
        if (b == 0) break;
        if (b == -1) throw new IOException();
        bb.append((byte) b);
      }

      table[i] = bb;
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
        int sender    = readUnsignedShort(sum);
        int recipient = readUnsignedShort(sum);
        int subject   = readUnsignedShort(sum);
        long date     = readUnsignedInteger(sum);
        long flags    = readUnsignedInteger(sum);
        long pos      = readUnsignedInteger(sum);
        long length   = readUnsignedInteger(sum);
        int statusoff = readUnsignedShort(sum);
        int lines     = readUnsignedShort(sum);
        int id        = readUnsignedShort(sum);
        int rlength   = readUnsignedShort(sum);

        ByteBuf refs[] = null;
        if (rlength > 0) {
          refs = new ByteBuf[rlength];
          for (int j = 0; j < rlength; j++) {
            refs[j] = table[readUnsignedShort(sum)];
          }
        }

        if (pessimistic) {
          refs = null;
          ByteBuf bid = table[id];
          MessageID mid = new MessageID(bid.toBytes(), 0, bid.length());
          salvage.put(mid, new Long(flags));
        } else {
          flags = BerkeleyMessage.mozillaFlagsToInternalFlags(flags);
          BerkeleyMessage m = new BerkeleyMessage(folder,
                                                  date * 1000L,
                                                  flags,
                                                  table[sender],
                                                  table[recipient],
                                                  table[subject],
                                                  table[id],
                                                  refs);
          refs = null;
          m.setSize((int) length);
          msgs.addElement(m);
        }
      }

      success = true;

    } finally {
      table = null;

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

}
