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
 * Created: Jamie Zawinski <jwz@netscape.com>,  2 Oct 1997.
 */

package grendel.storage;

import calypso.util.Assert;
import calypso.util.ByteBuf;

import javax.mail.internet.InternetHeaders;
import javax.mail.Header;

import java.io.RandomAccessFile;
import java.io.IOException;
import java.io.EOFException;
import java.util.Enumeration;

import grendel.util.Constants;

class ParseBerkeleyFolderAndExpunge extends ParseBerkeleyFolder {

  private static final boolean DEBUG = true;

  protected RandomAccessFile in = null;
  protected RandomAccessFile out = null;
  protected byte bytes[] = null;
  protected ByteBuf bytebuf = null;
  protected byte blank_mozilla_status[] = null;
  protected byte[] linebreak = null;
  Enumeration msgs = null;

  synchronized void expungeDeletedMessages(BerkeleyFolder f,
                                           RandomAccessFile in,
                                           RandomAccessFile out,
                                           Enumeration old_msgs)
    throws IOException {

    Assert.Assertion(old_msgs != null);

    total_message_count = 0;            // recordOne() updates these.
    undeleted_message_count = 0;
    unread_message_count = 0;
    deleted_message_bytes = 0;

    this.in = in;
    this.out = out;
    bytes = new byte[1024 * 4];
    bytebuf = new ByteBuf();
    blank_mozilla_status = ("X-Mozilla-Status: 0000" +
                            Constants.LINEBREAK).getBytes();

    // Figure out what line terminator is used in this file by searching
    // forward for the first "\n", "\r", or "\r\n".
    {
      long pos = in.getFilePointer();
      int b;
      do {
        b = in.read();
      } while (b != -1 && b != '\r' && b != '\n');

      if (b == -1 || (b == '\r' && (in.read() == '\n')))
        linebreak = "\r\n".getBytes();
      else if (b == '\n')
        linebreak = "\n".getBytes();
      else
        linebreak = "\r".getBytes();
      in.seek(pos);
    }

    msgs = old_msgs;
    mapOverMessages(in, f);

    bytes = null;
    bytebuf = null;
    linebreak = null;
  }



  void recordOne(int envelope_start, int message_start,
                 int headers_end, int message_end,
                 InternetHeaders headers)
    throws IOException {

//      System.err.println("   ** " +
//                         envelope_start + " " + message_start + " " +
//                         headers_end + " " + message_end);

    Assert.Assertion(envelope_start >= 0 &&
                     message_start > envelope_start &&
                     headers_end >= message_start &&
                     message_end >= headers_end);

    BerkeleyMessage old_msg = (BerkeleyMessage) msgs.nextElement();
    Assert.Assertion(old_msg != null);

    if (DEBUG) {
      // Make sure the sequence of messages in the file is in step with the
      // sequence of messages in the folder object.
      String idh = null;
      String hh[] = headers.getHeader("Message-ID");
      if (hh != null && hh.length != 0)
        idh = hh[0];

      if (idh != null) {
        MessageID id = new MessageID(idh);
        if (bytebuf.length() > 4 &&
            !id.equals(old_msg.getMessageID())) {
          System.err.println("ids differ!");
          System.err.println("   new id = " + id + " (" + idh + ")");
          System.err.println("   old id = " + old_msg.getMessageID());
        }
      }
    }

    if ((old_msg.flags & MessageBase.FLAG_DELETED) != 0)
      // This message is deleted; don't write it out.
      return;


    // Create a new message.  We can't reuse the old message, because the
    // caller is going to want to back out of any changes if an error occurs;
    // so it needs to hold on to a copy of the original data until we're done.
    //
    BerkeleyMessage new_msg = new BerkeleyMessage(fFolder, headers);


    // First, copy the envelope line from one file to the other.
    // Then remember the position in the output file of (what will be) the
    // beginning of the headers of this message.
    //
    long in_pos = in.getFilePointer();

    in.seek(envelope_start);
    copyBytes(message_start - envelope_start, false);
    new_msg.setStorageFolderIndex((int) out.getFilePointer());


    int total_msg_bytes_written = 0;    // doesn't include envelope


    // Now, write out the headers.  Rather than just copying the bytes of
    // the headers from one file to another, we reconstruct the output
    // headers from the parsed headers object.  We do this because it's
    // the easiest way to skip over (don't copy) certain headers, which
    // we want to recompute the values of.
    //
    // Note that this could possibly (but not probably) change the linebreaks
    // of the headers during their passage from one file to another.  Note
    // also that we *don't* change the linebreaks of the bodies, just the
    // headers.  Since all linebreaks in one file should match, this
    // shouldn't happen (or matter.)
    //
    for (Enumeration e = headers.getAllHeaders(); e.hasMoreElements();) {
      Header hh = (Header) e.nextElement();
      String h = hh.getName();
      String v = hh.getValue();

      // Don't copy these headers from input to output.
      if (h.equalsIgnoreCase("X-Mozilla-Status") ||
          h.equalsIgnoreCase("Content-Length") ||
          h.equalsIgnoreCase("X-UIDL"))
        continue;

      bytebuf.setLength(0);
      bytebuf.append(h);
      bytebuf.append(": ");
      bytebuf.append(v);
      bytebuf.append(linebreak);
      out.write(bytebuf.toBytes(), 0, bytebuf.length());
      total_msg_bytes_written += bytebuf.length();
    }

    // Now, write out the recomputed values of the X-Mozilla-Status,
    // Content-Length, and (maybe) X-UIDL headers.
    //
    bytebuf.setLength(0);

    if (false) {
      bytebuf.append("X-UIDL: ");
      bytebuf.append("####");
      bytebuf.append(linebreak);
    }

//    if (old_msg.flags != new_msg.flags) {
//      System.err.println("flags diff at " + message_start + ": " +
//                         old_msg.flags + " " + new_msg.flags);
//    }

    long xms = BerkeleyMessage.makeMozillaFlags(old_msg);
    bytebuf.append("X-Mozilla-Status: ");
    String s = Long.toHexString(xms);
    int i = s.length();
    if (i == 1) bytebuf.append("000");
    else if (i == 2) bytebuf.append("00");
    else if (i == 3) bytebuf.append("0");
    bytebuf.append(s);
    bytebuf.append(linebreak);

    bytebuf.append("Content-Length: ");
    int cl = (message_end - headers_end - linebreak.length);
    if (cl < 0) cl = 0;
    bytebuf.append(cl);
    bytebuf.append(linebreak);

    out.write(bytebuf.toBytes(), 0, bytebuf.length());
    total_msg_bytes_written += bytebuf.length();


    // We've written the headers.  Now copy the body bytes.
    //
    in.seek(headers_end);
    copyBytes(message_end - headers_end, true);
    total_msg_bytes_written += (message_end - headers_end);
    in.seek(in_pos);

    // Now that we've written the data, update the message with the true
    // byte-length (this will be the value of the Content-Length header,
    // plus the number of bytes we wrote for the headers.)
    //
    new_msg.setSize(total_msg_bytes_written);

    // Update the summary counts...
    Assert.Assertion(!new_msg.isDeleted());
    total_message_count++;
    undeleted_message_count++;
    if (!new_msg.isRead())
      unread_message_count++;

    // Now hand this new message object back to the folder, and we're done.
    //
    fFolder.noticeInitialMessage(new_msg);
  }


  /** Copy N bytes from `in' to `out'.
      If `blank_line' is true, and the data copied didn't end with a
      linebreak, insert a linebreak.
    */
  private void copyBytes(int n, boolean blank_line) throws IOException {
    int buf_length = bytes.length;
    int bytes_left = n;
    boolean broken = false;

    while (bytes_left > 0) {
      int to_read = bytes_left;
      if (to_read > buf_length)
        to_read = buf_length;
      int bytes_read = in.read(bytes, 0, to_read);
//      System.err.println("      read " + bytes_read +
//                         " (with " + bytes_left + " left)");
      if (bytes_read < 0)
        throw new EOFException();
      else
        bytes_left -= bytes_read;

      out.write(bytes, 0, bytes_read);

      if (blank_line)
        broken = (bytes[bytes_read-1] == '\r' ||
                  bytes[bytes_read-1] == '\n');
    }

    if (blank_line && !broken) {
//      System.err.println("inventing linebreak at " + in.getFilePointer());
      out.write(linebreak, 0, linebreak.length);
    }
  }
}
