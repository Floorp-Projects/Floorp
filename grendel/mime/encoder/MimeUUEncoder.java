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
 */

package grendel.mime.encoder;

import calypso.util.ByteBuf;

/** Implements a plaintext -> uuencode encoder.
 */
public final class MimeUUEncoder extends MimeEncoder {

  private byte name[] = null;              // name for the "begin" line
  private int buf = 0;                     // a 24-bit quantity
  private int buf_bytes = 0;               // how many octets are set in it
  private byte line[] = new byte[63];      // output buffer
  private int line_length = 1;             // output buffer fill pointer

  static private final byte begin_644[] = "begin 644 ".getBytes();
  static private final byte crlf[]      = "\r\n".getBytes();
  static private final byte end_crlf[]  = "end\r\n".getBytes();

  static private final byte map[] = {
    (byte)'`', (byte)'!', (byte)'"', (byte)'#', (byte)'$', (byte)'%', (byte)'&', (byte)'\'',    // 0-7
    (byte)'(', (byte)')', (byte)'*', (byte)'+', (byte)',', (byte)'-', (byte)'.', (byte)'/',     // 8-15
    (byte)'0', (byte)'1', (byte)'2', (byte)'3', (byte)'4', (byte)'5', (byte)'6', (byte)'7',     // 16-23
    (byte)'8', (byte)'9', (byte)':', (byte)';', (byte)'<', (byte)'=', (byte)'>', (byte)'?',     // 24-31
    (byte)'@', (byte)'A', (byte)'B', (byte)'C', (byte)'D', (byte)'E', (byte)'F', (byte)'G',     // 32-39
    (byte)'H', (byte)'I', (byte)'J', (byte)'K', (byte)'L', (byte)'M', (byte)'N', (byte)'O',     // 40-47
    (byte)'P', (byte)'Q', (byte)'R', (byte)'S', (byte)'T', (byte)'U', (byte)'V', (byte)'W',     // 48-55
    (byte)'X', (byte)'Y', (byte)'Z', (byte)'[', (byte)'\\',(byte)']', (byte)'^', (byte)'_',     // 56-63
  };


  public MimeUUEncoder(String file_name) {
    this.name = file_name.getBytes();
  }


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

  private final void flush_line(ByteBuf out, int offset) {
    line[0] = map[(line_length * 3 / 4) - offset];
    line[line_length]   = (byte)'\r';
    line[line_length+1] = (byte)'\n';
    out.append(line, 0, line_length + 2);
    line_length = 1;
  }

  /** Given a sequence of input bytes, produces a sequence of output bytes
      using uu encoding.  If there are bytes in `out' already, the new bytes
      are appended, so the caller should do `out.setLength(0)' first if that's
      desired.
   */
  public final void translate(ByteBuf in, ByteBuf out) {
    if (name != null) {
      out.append(begin_644);
      out.append(name);
      out.append(crlf);
      name = null;
    }

    byte inb[] = in.toBytes();
    int in_length = in.length();

    for (int i = 0; i < in_length; i++) {
      if (buf_bytes == 0)
        buf = (buf & 0x00FFFF) | (inb[i] << 16);
      else if (buf_bytes == 1)
        buf = (buf & 0xFF00FF) | (inb[i] << 8);
      else
        buf = (buf & 0xFFFF00) | (inb[i]);

      if ((++buf_bytes) == 3) {
        encode_token();
        if (line_length >= 60)
          flush_line(out, 0);
      }
    }
  }

  /** Tell the uu encoder that no more input data will be forthcoming.
      This may result in output, as a result of flushing the internal buffer.
      This object must not be used again after calling eof().  If there are
      bytes in `out' already, the new bytes are appended, so the caller should
      do `out.setLength(0)' first if that's desired.
   */
  public final void eof(ByteBuf out) {
    if (buf_bytes != 0) {
      int off = 3 - buf_bytes;
      encode_token();
      flush_line(out, off);
    }
    flush_line(out, 0);
    name = null;
    line = null;
    out.append(end_crlf);
  }
}
