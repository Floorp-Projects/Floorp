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


/** This class implements the parser for the message/rfc822 and message/news
    MIME containers, which is to say, mail and news messages.  This is a
    container which holds exactly one child, the body: messages with multiple
    parts are those which have a multipart/ as their body.
    @see MimeMultipart
    @see MimeLeaf
 */

class MimeMessageRFC822 extends MimeContainer {

  ByteLineBuffer line_buffer;
  ByteBuf line_bytes;
  InternetHeaders child_headers;

  public MimeMessageRFC822(String content_type, InternetHeaders headers) {
    super(content_type, headers);
    line_buffer = new ByteLineBuffer();
    line_bytes = new ByteBuf(100);
    child_headers = new InternetHeaders();
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

    // make sure every message has a body, even if it's a null body.
    if (open_child == null) {
      line_bytes.setLength(0);
      line_bytes.append("\r\n");
      process_line(line_bytes);
    }

    line_bytes = null;
    super.pushEOF();
   }

  /** Called for each line of this message. */
  void process_line(ByteBuf line) {

    if (open_child == null) {                   // Still parsing the headers

      child_headers.addHeaderLine(line.toString());

      // If this is a blank line, create a child.
      byte b = line.byteAt(0);
      if (b == '\r' || b == '\n') {

        String hh[] = child_headers.getHeader("Content-Type");
        String child_type = null;

        if (hh != null && hh.length != 0) {
          child_type = hh[0];
          // #### strip to first token
          int i = child_type.indexOf(';');
          if (i > 0)
            child_type = child_type.substring(0, i);
          child_type = child_type.trim();
        }
        hh = null;
        openChild(child_type, child_headers);
        child_type = null;
        child_headers = null;
      }

    } else {                                    // Parsing the body
      open_child.pushBytes(line);
    }
  }
}
