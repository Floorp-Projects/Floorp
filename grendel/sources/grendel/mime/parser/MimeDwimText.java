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
 * Created: Jamie Zawinski <jwz@netscape.com>, 21 Aug 1997.
 */

package grendel.mime.parser;

import grendel.mime.IMimeOperator;
import calypso.util.ByteBuf;
import calypso.util.ByteLineBuffer;
import javax.mail.internet.InternetHeaders;

/** This class implements a parser for untyped message contents, that is, it
    is the class used for the body of a message/rfc822 object which had
    <I>no</I> Content-Type header, as opposed to an unrecognized content-type.
    Such a message, technically, does not contain MIME data (it follows only
    RFC 822, not RFC 1521.)

    <P> This is a container class, and the reason for that is that it loosely
    parses the body of the message looking for ``sub-parts'' and then creates
    appropriate containers for them.

    <P> More specifically, it looks for uuencoded and BinHexed data.  It may
    do more than that some day.  (DWIM stands for ``Do What I Mean.'')

    @see MimeMultipart
    @see MimeXSunAttachment
 */

class MimeDwimText extends MimeContainer {

  ByteLineBuffer line_buffer;
  ByteBuf line_bytes;

  // enumeration of sub-part types
  static private final int TEXT   = 100;
  static private final int UUE    = 101;
  static private final int BINHEX = 102;

  int type;

  String subpart_name;
  String subpart_type;
  String subpart_encoding;

  public MimeDwimText(String content_type, InternetHeaders headers) {
    super(content_type, headers);
    line_buffer = new ByteLineBuffer();
    line_bytes = new ByteBuf(100);
    type = TEXT;
  }

  /** Buffers the bytes into lines, and calls process_line() on each line. */
  public void pushBytes(ByteBuf bytes) {
    line_buffer.pushBytes(bytes);
    while (line_buffer.pullLine(line_bytes))
      process_line(line_bytes);
  }

  /** Flushes the line buffer, and (maybe) calls process_line() one last time.
   */
  public void pushEOF() {
    line_buffer.pushEOF();
    while (line_buffer.pullLine(line_bytes))
      process_line(line_bytes);
    line_buffer = null;
    line_bytes = null;
    super.pushEOF();
  }

  /** Called for each line of this part. */
  void process_line(ByteBuf line_buf) {

    int length = line_buf.length();
    byte line[] = line_buf.toBytes();
    boolean just_started = false;

    if (length < 3)
      ;
    else if (line[0] == 'b' && uue_begin_line_p(line, length)) {
      // Close the old part and open a new one.
      just_started = true;
      // subpart_name was kludgily set by uue_begin_line_p...  I suck.
      subpart_type = name_to_content_type(subpart_name);
      subpart_encoding = "uuencode";
      open_untyped_child();

    } else if (line[0] == '(' && line[1] == 'T' &&
               binhex_begin_line_p(line, length)) {
      // Close the old part and open a new one.
      just_started = true;
      subpart_type = "application/mac-binhex40";
      subpart_encoding = null;
      subpart_name = null;
      open_untyped_child();
    }

    // Open a text/plain sub-part if there is no sub-part open.
    if (open_child == null) {
      subpart_type = "text/plain";
      subpart_encoding = null;
      subpart_name = null;
      open_untyped_child();
    }

    // Hand this line to the currently-open sub-part.
    open_child.pushBytes(line_buf);

    // Close this sub-part if this line demands it.
    if (length >= 3 &&
        ((type == UUE && line[0] == 'e' && uue_end_line_p(line, length)) ||
         (type == BINHEX && binhex_end_line_p(line, length))))
      closeChild();
  }

  void open_untyped_child() {
    InternetHeaders hdrs = new InternetHeaders();

    if (subpart_type == null)
      hdrs.addHeaderLine("Content-Type: application/octet-stream\r\n");
    else
      hdrs.addHeaderLine("Content-Type: " + subpart_type + "\r\n");

    if (subpart_encoding != null)
      hdrs.addHeaderLine("Content-Transfer-Encoding: " + subpart_encoding
                         + "\r\n");

    if (subpart_name != null)
      hdrs.addHeaderLine("Content-Disposition: inline; filename=\"" +
                    subpart_name + "\"\r\n");
    hdrs.addHeaderLine("\r\n");

    String null_str = null;     // weird ambiguous prototype thing...
    openChild(null_str, hdrs);
    hdrs = null;
  }

  boolean uue_begin_line_p(byte line[], int length) {

    if (length < 10 ||
        line[0] != 'b' ||
        line[1] != 'e' ||
        line[2] != 'g' ||
        line[3] != 'i' ||
        line[4] != 'n' ||
        line[5] != ' ')
      return false;

    // ...then three or four octal digits.
    int i = 6;
    if (line[i] < '0' || line[i] > '7') return false;
    i++;
    if (line[i] < '0' || line[i] > '7') return false;
    i++;
    if (line[i] < '0' || line[i] > '7') return false;
    i++;

    if (line[i] == ' ')
      i++;
    else {
      if (line[i] < '0' || line[i] > '7') return false;
      i++;
      if (line[i] != ' ') return false;
    }

    while (i < length && line[i] <= ' ')
      i++;

    while (length >= i && line[length-1] <= ' ')
      length--;

    // so sue me.
    subpart_name = new String(line, i, length-i);

    return true;
  }

  boolean uue_end_line_p(byte line[], int length) {
    // accept any line with whitespace at the beginning of the line;
    // or any line beginning with the characters "end".
    return (length > 1 &&
            (line[0] == ' ' ||
             line[0] == '\t' ||
             (length > 3 &&
              ((line[0] == 'e' || line[0] == 'E') &&
               (line[1] == 'n' || line[1] == 'N') &&
               (line[2] == 'd' || line[2] == 'D')))));
  }

  boolean binhex_begin_line_p(byte line[], int length) {
    String s = new String(line, 0, length);
    return s.startsWith("(This file must be converted with BinHex 4.0)");
  }

  boolean binhex_end_line_p(byte line[], int length) {
    if (length < 64) return false;
    if (line[length-1] == '\n') length--;
    if (line[length-1] == '\r') length--;
    return (length == 64);
  }

  String name_to_content_type(String filename) {
    // ####  need to get at the file-extension-to-mime-type mapping...

    filename = filename.toLowerCase();
    if (filename.endsWith("txt") || filename.endsWith("text"))
      return "text/plain";
    else if (filename.endsWith("htm") || filename.endsWith("html"))
      return "text/html";
    else if (filename.endsWith("gif"))
      return "image/gif";
    else if (filename.endsWith("jpg") || filename.endsWith("jpeg"))
      return "image/jpeg";
    else if (filename.endsWith("pjpg") || filename.endsWith("pjpeg"))
      return "image/pjpeg";
    else if (filename.endsWith("xbm"))
      return "image/x-xbitmap";
    else if (filename.endsWith("xpm"))
      return "image/x-xpixmap";
    else if (filename.endsWith("xwd"))
      return "image/x-xwindowdump";
    else if (filename.endsWith("bmp"))
      return "image/x-MS-bmp";
    else if (filename.endsWith("au"))
      return "audio/basic";
    else if (filename.endsWith("aif") || filename.endsWith("aiff") ||
             filename.endsWith("aifc"))
      return "audio/x-aiff";
    else if (filename.endsWith("ps"))
      return "application/postscript";
    else if (filename.endsWith("p7m"))
      return "application/x-pkcs7-mime";
    else if (filename.endsWith("p7c"))
      return "application/x-pkcs7-mime";
    else if (filename.endsWith("p7s"))
      return "application/x-pkcs7-signature";
    else
      return "application/octet-stream";
  }
}
