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
 * Created: Jamie Zawinski <jwz@netscape.com>, 28 Aug 1997.
 *
 * Contributors: Edwin Woudt <edwin@woudt.nl>
 *               harning@cbs.dk
 */

package grendel.mime.encoder;

import calypso.util.ByteBuf;

/** Implements a Base64 -> plaintext decoder.
 */
public final class MimeBase64Encoder extends MimeEncoder {

  private int buf = 0;                     // a 24-bit quantity
  private int buf_bytes = 0;               // how many octets are set in it
  private byte line[] = new byte[74];      // output buffer
  private int line_length = 0;             // output buffer fill pointer

  static private final byte crlf[] = "\r\n".getBytes();

  static private final byte map[] = {
    (byte)'A', (byte)'B', (byte)'C', (byte)'D', (byte)'E', (byte)'F', (byte)'G', (byte)'H',     // 0-7
    (byte)'I', (byte)'J', (byte)'K', (byte)'L', (byte)'M', (byte)'N', (byte)'O', (byte)'P',     // 8-15
    (byte)'Q', (byte)'R', (byte)'S', (byte)'T', (byte)'U', (byte)'V', (byte)'W', (byte)'X',     // 16-23
    (byte)'Y', (byte)'Z', (byte)'a', (byte)'b', (byte)'c', (byte)'d', (byte)'e', (byte)'f',     // 24-31
    (byte)'g', (byte)'h', (byte)'i', (byte)'j', (byte)'k', (byte)'l', (byte)'m', (byte)'n',     // 32-39
    (byte)'o', (byte)'p', (byte)'q', (byte)'r', (byte)'s', (byte)'t', (byte)'u', (byte)'v',     // 40-47
    (byte)'w', (byte)'x', (byte)'y', (byte)'z', (byte)'0', (byte)'1', (byte)'2', (byte)'3',     // 48-55
    (byte)'4', (byte)'5', (byte)'6', (byte)'7', (byte)'8', (byte)'9', (byte)'+', (byte)'/',     // 56-63
  };

  private final void encode_token() {
    int i = line_length;
    line[i]   = map[0x3F & (buf >> 18)];   // sextet 1 (octet 1)
    line[i+1] = map[0x3F & (buf >> 12)];   // sextet 2 (octet 1 and 2)
    line[i+2] = map[0x3F & (buf >> 6)];    // sextet 3 (octet 2 and 3)
    line[i+3] = map[0x3F & buf];           // sextet 4 (octet 3)
    line_length += 4;
    buf = 0;
    buf_bytes = 0;
  }

  private final void encode_partial_token() {
    int i = line_length;
    line[i]   = map[0x3F & (buf >> 18)];   // sextet 1 (octet 1)
    line[i+1] = map[0x3F & (buf >> 12)];   // sextet 2 (octet 1 and 2)

    if (buf_bytes == 1)
      line[i+2] = (byte)'=';
    else
      line[i+2] = map[0x3F & (buf >> 6)];  // sextet 3 (octet 2 and 3)

    if (buf_bytes <= 2)
      line[i+3] = (byte)'=';
    else
      line[i+3] = map[0x3F & buf];         // sextet 4 (octet 3)
    line_length += 4;
    buf = 0;
    buf_bytes = 0;
  }

  private final void flush_line(ByteBuf out) {
    line[line_length]   = (byte)'\r';
    line[line_length+1] = (byte)'\n';
    out.append(line, 0, line_length + 2);
    line_length = 0;
  }

  /** Given a sequence of input bytes, produces a sequence of output bytes
      using the base64 encoding.  If there are bytes in `out' already, the
      new bytes are appended, so the caller should do `out.setLength(0)'
      first if that's desired.
   */
  public final void translate(ByteBuf in, ByteBuf out) {

    byte inb[] = in.toBytes();
    int in_length = in.length();

    for (int i = 0; i < in_length; i++) {
      if (buf_bytes == 0)
        buf = (buf & 0x00FFFF) | (inb[i] << 16);
      else if (buf_bytes == 1)
        buf = (buf & 0xFF00FF) | ((inb[i] << 8) & 0x00FF00);
      else
        buf = (buf & 0xFFFF00) | ((inb[i]) & 0x0000FF);

      if ((++buf_bytes) == 3) {
        encode_token();
        if (line_length >= 72)
          flush_line(out);
      }
    }
  }

  /** Tell the base64 encoder that no more input data will be forthcoming.
      This may result in output, as a result of flushing the internal buffer.
      This object must not be used again after calling eof().  If there are
      bytes in `out' already, the new bytes are appended, so the caller should
      do `out.setLength(0)' first if that's desired.
   */
  public final void eof(ByteBuf out) {
    if (buf_bytes != 0)
      encode_partial_token();
    flush_line(out);
    line = null;
  }
}
