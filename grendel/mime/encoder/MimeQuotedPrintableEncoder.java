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
import calypso.util.Assert;

/** Implements a plaintext -> Quoted-Printable encoder.
 */
public class MimeQuotedPrintableEncoder extends MimeEncoder {

  boolean had_whitespace = false;
  int output_column = 0;
  byte last_code = NUL;

  static private final byte NUL = 0;   // don't encode
  static private final byte ESC = 1;   // always encode
  static private final byte SPC = 2;   // horizontal whitespace
  static private final byte BOL = 3;   // encode if at beginning of line
  static private final byte CR  = 4;   // carriage return
  static private final byte LF  = 5;   // linefeed

  static private final byte map[] = {
    ESC, ESC, ESC, ESC, ESC, ESC, ESC, ESC,      //   000-007
    ESC, SPC,  LF, ESC, ESC,  CR, ESC, ESC,      //   010-017
    ESC, ESC, ESC, ESC, ESC, ESC, ESC, ESC,      //   020-027
    ESC, ESC, ESC, ESC, ESC, ESC, ESC, ESC,      //   030-037
    SPC, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   040-047   !"#$%&'
    NUL, NUL, NUL, NUL, NUL, NUL, BOL, NUL,      //   050-057  ()*+,-./
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   060-067  01234567
    NUL, NUL, NUL, NUL, NUL, ESC, NUL, NUL,      //   070-077  89:;<=>?

    NUL, NUL, NUL, NUL, NUL, NUL, BOL, NUL,      //   100-107  @ABCDEFG
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   110-117  HIJKLMNO
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   120-127  PQRSTUVW
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   130-137  XYZ[\]^_
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   140-147  `abcdefg
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   150-157  hijklmno
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   160-167  pqrstuvw
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, ESC,      //   170-177  xyz{|}~

    ESC, ESC, ESC, ESC, ESC, ESC, ESC, ESC,      //   200-207
    ESC, ESC, ESC, ESC, ESC, ESC, ESC, ESC,      //   210-217
    ESC, ESC, ESC, ESC, ESC, ESC, ESC, ESC,      //   220-227
    ESC, ESC, ESC, ESC, ESC, ESC, ESC, ESC,      //   230-237
    ESC, ESC, ESC, ESC, ESC, ESC, ESC, ESC,      //   240-247
    ESC, ESC, ESC, ESC, ESC, ESC, ESC, ESC,      //   250-257
    ESC, ESC, ESC, ESC, ESC, ESC, ESC, ESC,      //   260-267
    ESC, ESC, ESC, ESC, ESC, ESC, ESC, ESC,      //   270-277

    ESC, ESC, ESC, ESC, ESC, ESC, ESC, ESC,      //   300-307
    ESC, ESC, ESC, ESC, ESC, ESC, ESC, ESC,      //   310-317
    ESC, ESC, ESC, ESC, ESC, ESC, ESC, ESC,      //   320-327
    ESC, ESC, ESC, ESC, ESC, ESC, ESC, ESC,      //   330-337
    ESC, ESC, ESC, ESC, ESC, ESC, ESC, ESC,      //   340-347
    ESC, ESC, ESC, ESC, ESC, ESC, ESC, ESC,      //   350-357
    ESC, ESC, ESC, ESC, ESC, ESC, ESC, ESC,      //   360-367
    ESC, ESC, ESC, ESC, ESC, ESC, ESC, ESC,      //   370-377
  };

  static private final byte hex[] = {
    (byte)'0', (byte)'1', (byte)'2', (byte)'3', (byte)'4', (byte)'5', (byte)'6', (byte)'7',
    (byte)'8', (byte)'9', (byte)'A', (byte)'B', (byte)'C', (byte)'D', (byte)'E', (byte)'F'
  };

  static private final byte         crlf[] =      "\r\n".getBytes();
  static private final byte      eq_crlf[] =     "=\r\n".getBytes();
  static private final byte eq_crlf_crlf[] = "=\r\n\r\n".getBytes();


  /** Given a sequence of input bytes, produces a sequence of output bytes
      using the quoted-printable encoding.  If there are bytes in `out'
      already, the new bytes are appended, so the caller should do
      `out.setLength(0)' first if that's desired.
   */
  public final void translate(ByteBuf in, ByteBuf out) {

    byte inb[] = in.toBytes();
    int in_length = in.length();

    for (int i = 0; i < in_length; i++) {
      byte b = inb[i];
      byte c = map[b];

      if (c == NUL) {                           // non-special character
        out.append(b);
        output_column++;
        had_whitespace = false;

      } else if (c == SPC) {                    // whitespace
        out.append(b);
        output_column++;
        had_whitespace = true;

      } else if (c == LF && last_code == CR) {
        // We already processed this linebreak;
        // ignore the LF in the CRLF pair.

      } else if (c == CR || c == LF) {          // start of linebreak

        if (had_whitespace) {
          // Whitespace cannot be allowed to occur at the end of the line.
          // So we encode " \n" as " =\n\n", that is, the whitespace, a
          // soft line break, and then a hard line break.
          //
          out.append(eq_crlf_crlf);
          had_whitespace = false;
        } else {
          out.append(crlf);
        }
        output_column = 0;

      } else if (c == BOL && output_column != 0) {      // beginning-of-line
        out.append(b);                                  // special, while not
        output_column++;                                // at beginning-of-line
        had_whitespace = false;

      } else {                                  // special character
        out.append((byte) '=');
        out.append((byte) hex[0xF & (b >> 4)]);
        out.append((byte) hex[0xF & b]);
        output_column += 3;
        had_whitespace = false;
      }

      last_code = c;

      Assert.Assertion(output_column <= 76);

      if (output_column >= 73) {
        out.append(eq_crlf);
        output_column = 0;
        had_whitespace = false;
      }
    }
  }

  /** Tell the quoted-printable encoder that no more input data will be
      forthcoming.  This may result in output, as a result of flushing the
      internal buffer.  This object must not be used again after calling eof().
      If there are bytes in `out' already, the new bytes are appended,
      so the caller should do `out.setLength(0)' first if that's desired.
   */
  public final void eof(ByteBuf out) {
    if (output_column != 0) {
      out.append(eq_crlf);
      output_column = 0;
      had_whitespace = false;
    }
  }
}
