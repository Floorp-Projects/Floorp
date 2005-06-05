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
 *  ParseBerkeleyFolder.java
 *  Created: Terry Weissman <terry@netscape.com>,  8 Aug 1997.
 *  Shamelessly based on mbox.c which was
 *   Created: Jamie Zawinski <jwz@netscape.com>, 22 Jul 1997.
 */

/**
   This sectionizes a mailbox file into its component messages; it understands
   both the BSD and Solaris formats ("From " and "Content-Length".)  See also
   http://www.netscape.com/eng/mozilla/2.0/relnotes/demo/content-length.html
   for related ranting.
 */


package grendel.storage;

import grendel.storage.intertwingle.Twingle;

import calypso.util.Assert;
import calypso.util.ByteBuf;
import calypso.util.ByteLineBuffer;
import calypso.util.RandomAccessFileWithByteLines;

import javax.mail.internet.InternetHeaders;

import java.io.RandomAccessFile;
import java.io.InputStream;
import java.io.IOException;


class ParseBerkeleyFolder {

  static final boolean DEBUG = false;

  static final int CONTENT_LENGTH_SLACK = 100;

  Twingle twingle = Twingle.GetSingleton();


  /* Returns the file-byte-index of the end of the message, if it can be
     determined from the given Content-Length, and if it checks out.
     If the Content-Length is obvious nonsense, or if any read/seek errors
     occur, returns IT_SEEK_ERROR.

     The file-read-pointer is expected to be just past the headers of the
     message in question (poised to read the first byte of the body.)
     It will be left in that same place upon return.

     The Content-Length is checked for sanity (by examining the file at
     the indicated position) and is adjusted if it seems to be off by a
     small amount.  It can be off by up to N bytes in either direction;
     the correct value is assumed to be the closer of the two nearest
     message-delimiters within that range (either earlier or later.)
     */

  BerkeleyFolder fFolder;

  byte buf[] = new byte [CONTENT_LENGTH_SLACK * 2 + 1];

  protected int total_message_count = 0;
  protected int undeleted_message_count = 0;
  protected int unread_message_count = 0;
  protected long deleted_message_bytes = 0;

  int totalMessageCount() { return total_message_count; }
  int undeletedMessageCount() { return undeleted_message_count; }
  int unreadMessageCount() { return unread_message_count; }
  long deletedMessageBytes() { return deleted_message_bytes; }


  int find_content_end(RandomAccessFileWithByteLines in, int content_length,
                       int file_length)
    throws IOException {

      int start_pos = (int) in.getFilePointer();
      boolean eof = false;
      int read_start, read_end, target;
      int early_match, late_match;
      int bytes;
      int result = -1;

      Assert.Assertion(content_length >= 0 && start_pos >= 0);
      if (content_length < 0 || start_pos < 0) return -1;

      target = start_pos + content_length;
      read_start = target - ((buf.length - 1) / 2);

      /* Read a block centered on the position indicated by Content-Length
         (with `slack' bytes in either direction.) */

      int b = 0;
      int bytes_left = buf.length - 1;

      if (read_start < start_pos) {
        bytes_left -= (start_pos - read_start);
        read_start = start_pos;
      }

      if (read_start + bytes_left > file_length) {
        bytes_left = file_length - read_start;
        eof = true;
      }

      in.seek(read_start);
      in.readFully(buf, b, bytes_left);

// System.err.println(new String(buf, b, bytes_left));

      b += bytes_left;

      read_end = read_start + b;

      /* Find the first mailbox delimiter at or before the position indicated
         by Content-Length.
         */

      early_match = -1;
      int s = (target - read_start) + 6;
      if (s > (read_end - read_start))
        s = (read_end - read_start);
      s -= 6;
      for (; s > 0; s--) {
        if (buf[s] == '\n' &&
            buf[s+1] == 'F' &&
            buf[s+2] == 'r' &&
            buf[s+3] == 'o' &&
            buf[s+4] == 'm' &&
            buf[s+5] == ' ') {
          early_match = read_start + s;
        }
      }

      /* Find the first mailbox delimiter at or past the position indicated
         by Content-Length. */

      late_match = -1;
      if (read_end > target) {

        s = (target - read_start) - 6;
        if (s < 0) s = 0;
        for ( ;
             s < (read_end - read_start) - 6;
             s++) {
          if (buf[s] == '\n' &&
              buf[s+1] == 'F' &&
              buf[s+2] == 'r' &&
              buf[s+3] == 'o' &&
              buf[s+4] == 'm' &&
              buf[s+5] == ' ') {
            late_match = read_start + s;
            break;
          }
        }
      }

      if (late_match == -1 && eof) {
        /* EOF counts as a match. */
        late_match = file_length - 1;
      }

      if (early_match == late_match) {
        if (late_match < target) {
          late_match = -1;
        } else {
          early_match = -1;
        }
      } else if (early_match == -1 && late_match < target) {
        early_match = late_match;
        late_match = -1;
      }

      if (early_match >= 0 && late_match >= 0) {
        /* Found two -- pick the closest one. */
        if ((target - early_match) < (late_match - target)) {
          result = early_match;
        } else {
          result = late_match;
        }
      }
      else if (early_match >= 0) {
        result = early_match;
      } else {
        result = late_match;
      }

      if (DEBUG) {
        if (result == -1)
          System.err.println("content-length of " + content_length + " is nonsense (or off by more than " + CONTENT_LENGTH_SLACK + " bytes)");
        else if (result != target) {
          if (result == late_match)
            System.err.println("content-length of " + content_length +
                               " was " +
                               (late_match - target) +
                               " bytes too small (pos " +
                               result +
                               ")");
          else
            System.err.println("content-length of " + content_length +
                               " was " +
                               (target - early_match) +
                               " bytes too large (pos " +
                               result +
                               ")");
        } else {
          System.err.println("*** Hey; Content-Length worked perfectly!");
        }
      }

      in.seek(start_pos);

      return result;
  }


  void recordOne(int envelope_start, int message_start,
                 int headers_end, int message_end,
                 InternetHeaders headers)
    throws IOException {

    Assert.Assertion(envelope_start >= 0 &&
                     message_start > envelope_start &&
                     headers_end >= message_start &&
                     message_end >= headers_end);

    if (DEBUG) {
      System.out.println("Found one: start: " + message_start +
                         "; end: " + message_end);
      // headers.dump();
    }
    BerkeleyMessage m = new BerkeleyMessage(fFolder, headers);

    total_message_count++;
    if (m.isDeleted()) {
      deleted_message_bytes += (message_end - message_start);
    } else {
      undeleted_message_count++;
      if (!m.isRead())
        unread_message_count++;
    }

    m.setStorageFolderIndex(message_start);
    m.setSize(message_end - message_start);
    fFolder.noticeInitialMessage(m);
    if (twingle != null) {
      if (DEBUG) System.out.println("Calling twingle.add");
      twingle.add(headers, fFolder);
    }
  }


  void mapOverMessages(RandomAccessFile infile, BerkeleyFolder f)
    throws IOException {
    mapOverMessages(infile, f, 0);
  }

  protected synchronized void mapOverMessages(RandomAccessFile infile,
                                              BerkeleyFolder f,
                                              long starting_file_pos)
    throws IOException {
      fFolder = f;
      InternetHeaders hdrs = new InternetHeaders();

      int file_length = (int) infile.length();
      infile.seek(starting_file_pos);

      RandomAccessFileWithByteLines in =
        new RandomAccessFileWithByteLines(infile);

      ByteBuf line = ByteBuf.Alloc();

      InternetHeaders headers = new InternetHeaders();

      int last_end = 0;
      int start = -1;
      int headers_end = -1;
      boolean inheader = false;

      ByteBuf contentlengthname = new ByteBuf("Content-Length");
      ByteBuf lenstr = new ByteBuf();


      int last_position = 0;          // file byte index of start of prev line
      int this_position = 0;          // file byte index of start of this line
      int next_position = 0;          // file byte index of start of next line

      boolean last_blank_p = false;   // whether the prev line was blank
      boolean this_blank_p = false;   // whether this line is blank

      while (in.readLine(line)) {
        last_position = this_position;
        this_position = next_position;
        next_position = (int) in.getFilePointer();

        byte[] bytes = line.toBytes();  // doesn't cons

        last_blank_p = this_blank_p;
        this_blank_p = (line.length() == 0);

        if (line.length() >= 5 &&
            bytes[0] == 'F' &&
            bytes[1] == 'r' &&
            bytes[2] == 'o' &&
            bytes[3] == 'm' &&
            bytes[4] == ' ') {
          if (start >= 0) {

            // The last byte of the previous message is the byte before the
            // first character on the current "From " line; unless the
            // previous line was blank, in which case, the last byte of the
            // previous message is the byte before the first byte on that
            // line.  This seems to do the right thing most of the time, in
            // the face of missing or slightly-off Content-Length headers.
            //
            int end = (last_blank_p
                       ? last_position
                       : this_position);
            recordOne(last_end, start, headers_end, end, headers);
            last_end = end;
          }

          // The following message begins at the first byte of the next line
          // following this "From " line (that is, the byte after this line's
          // linebreak character(s).)
          //
          start = next_position;
          headers = new InternetHeaders();
          inheader = true;
          continue;
        }

        if (inheader) {
          if (line.length() == 0 ||
              line.byteAt(0) == '\n' || line.byteAt(0) == '\r') {
            inheader = false;
            headers_end = this_position;
            String hh[] = headers.getHeader("Content-Length");
            if (hh != null && hh.length != 0) {
              int contentLength;
              try {
                contentLength = Integer.parseInt(hh[0]);
              } catch (NumberFormatException e) {
                contentLength = -1;
              }
              if (contentLength > 0) {
                int loc = find_content_end(in, contentLength, file_length);
                if (loc > 0) {
                  this_position = loc;
                  next_position = loc;
                  this_blank_p = false;
                  in.seek(loc);
                }
              }
            }
          } else {
            headers.addHeaderLine(line.toString());
          }
        }
      }
      if (start >= 0 && !inheader) {
        if (headers_end <= start)
          headers_end = file_length;
        recordOne(last_end, start, headers_end, file_length, headers);
      }
  }


  /** Overwrite the X-Mozilla-Status header near the given file position.

      @param message_start   The position in the file of the start of a
                             message.  A header block is assumed to start at
                             this position.  The file is scanned forward for
                             an X-Mozilla-Status header in this block.
      @param message_length  The length of the message in bytes.
      @param mozilla_flags   The flags to overwrite into the existing
                             X-Mozilla-Status header, if any.

      @return                true if the header was overwritten, false
                             otherwise.
   */
  boolean flushMozillaStatus(RandomAccessFile io,
                             long message_start,
                             int message_length,
                             short mozilla_flags) throws IOException {

    long message_end = message_start + message_length;

    io.seek(message_start);
    ByteBuf buf = null;
    ByteBuf line = null;

    try {
      buf  = ByteBuf.Alloc();
      line = ByteBuf.Alloc();
      ByteLineBuffer linebuf = new ByteLineBuffer();

      boolean eof = false;

      // `pos' will point at the file position of the beginning of the
      // line we are processing.
      long pos = message_start;

      while (!eof) {
        buf.setLength(0);
        eof = (buf.read(io, 1024) < 0);
        if (eof) {
          linebuf.pushEOF();
        } else {
          linebuf.pushBytes(buf);
        }

        while (linebuf.pullLine(line)) {

          byte first = line.byteAt(0);
          if (first == '\r' || first == '\n')
            // end of headers -- and we never saw X-Mozilla-Status!
            return false;
          else if ((first == 'X' || first == 'x') &&
                   (line.regionMatches(true, 0, "X-Mozilla-Status:", 0, 17))) {
            // Woo hoo.  Make sure it has exactly the right number of bytes.
            int start = 17;
            while (line.byteAt(start) == ' ' || line.byteAt(start) == '\t')
              start++;

            for (int digits = 0; digits < 4; digits++) {
              byte b = line.byteAt(start + digits);
              if (! ((b >= '0' && b <= '9') ||
                     (b >= 'a' && b <= 'f') ||
                     (b >= 'A' && b <= 'F')))
                // if there aren't 4 consecutive hex digits, bug out.
                return false;
            }
            // If there isn't whitespace or a newline after the digits,
            // bug out.
            if (line.byteAt(start + 4) > ' ')
              return false;

            // Ok!  At this point, `start' points to the position in the
            // line of the first hex digit.  So the position in the file
            // is `pos+start'.  Seek there, and overwrite the bytes with
            // the new flag.

            String s = Integer.toHexString(((int) mozilla_flags) & 0xFFFF);
            if (s.length() == 3) s = "0" + s;
            else if (s.length() == 2) s = "00" + s;
            else if (s.length() == 1) s = "000" + s;
            io.seek(pos + start);
            io.write(s.getBytes());
            return true;  // all done!
          }

          pos += line.length();

          if (pos > message_end)
            // oops, reached end of message before end of headers?
            return false;
        }
      }
    } finally {
      if (buf != null)  ByteBuf.Recycle(buf);
      if (line != null) ByteBuf.Recycle(line);
    }

    // reached eof before end of message headers.
    return false;
  }

}
