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
 * Created: Jamie Zawinski <jwz@netscape.com>, 28 Aug 1997.
 */

package grendel.mime.encoder;

import calypso.util.ByteBuf;

/** Implements a Quoted-Printable -> plaintext decoder.
 */
public class MimeQuotedPrintableDecoder extends MimeEncoder {

  private byte token[] = new byte[3];      // input and output buffer
  private int token_length = 0;            // input buffer length

  static private final byte NUL = 127;     // must be greater than 16
  static private final byte ESC = 126;     // must be greater than 16
  static private final byte CR  = 125;     // must be greater than 16
  static private final byte LF  = 124;     // must be greater than 16

  static private final byte map[] = {
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   000-007
    NUL, NUL,  LF, NUL, NUL,  CR, NUL, NUL,      //   010-017
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   020-027
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   030-037
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   040-047   !"#$%&'
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   050-057  ()*+,-./
      0,   1,   2,   3,   4,   5,   6,   7,      //   060-067  01234567
      8,   9, NUL, NUL, NUL, ESC, NUL, NUL,      //   070-077  89:;<=>?

    NUL,  10,  11,  12,  13,  14,  15, NUL,      //   100-107  @ABCDEFG
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   110-117  HIJKLMNO
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   120-127  PQRSTUVW
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   130-137  XYZ[\]^_
    NUL,  10,  11,  12,  13,  14,  15, NUL,      //   140-147  `abcdefg
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   150-157  hijklmno
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   160-167  pqrstuvw
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   170-177  xyz{|}~

    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   200-207
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   210-217
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   220-227
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   230-237
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   240-247
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   250-257
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   260-267
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   270-277

    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   300-307
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   310-317
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   320-327
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   330-337
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   340-347
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   350-357
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   360-367
    NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,      //   370-377
  };


  /** Given a sequence of input bytes using the quoted-printable encoding,
      produces a sequence of unencoded output bytes.  Note that some (small)
      amount of buffering may be necessary, if the input byte stream didn't
      fall on an appropriate boundary.  If there are bytes in `out' already,
      the new bytes are appended, so the caller should do `out.setLength(0)'
      first if that's desired.
   */
  public final void translate(ByteBuf in, ByteBuf out) {

    /* To Do, maybe someday: when decoding quoted-printable, we are required
       to strip trailing whitespace from lines -- since when encoding in QP,
       one is required to quote such trailing whitespace, any trailing
       whitespace which remains must have been introduced by a stupid gateway.
       We don't do this right now, as it would require arbitrary lookahead
       instead of just a fixed distance of three bytes.
     */

    byte inb[] = in.toBytes();
    int in_length = in.length();

    int i = 0;

    while (i < in_length) {

      // First, copy as many consecutive bytes as we can.
      if (token_length == 0) {
        int j = i;
        while (i < in_length && map[inb[i]] != ESC)
          i++;
        if (i != j)
          out.append(inb, j, i-j);

        if (i >= in_length)
          break;
      }

      // at this point, either token_length > 0, or we've seen ESC

      byte b = inb[i];
      byte c = map[b];
      i++;

      token[token_length++] = b;

      if (token_length != 3)
        continue;

      token_length = 0;

      // assert (map[token[0]] == ESC)

      byte n1 = map[token[1]];
      byte n2 = map[token[2]];

      if (n1 == CR || n1 == LF) {               // "=\r", "=\n", or "=\r\n"
        if (n1 == LF || n2 != LF) {
          // lone CR or LF -- push out or put back following character.
          if (n2 == ESC) {
            token[0] = token[2];
            token_length = 1;
          } else {
            out.append(token[2]);
          }
        }

      } else if (n1 <= 15 || n2 <= 15) {        // "=xy" where x and y are hex
        out.append((byte) ((n1 << 4) | n2));

      } else {                                  // something unparsable.
        out.append(token);                      // write it out unchanged.
      }

    }
  }


  /** Tell the quoted-printable decoder that no more input data will be
      forthcoming.  This may result in output, as a result of flushing the
      internal buffer.  This object must not be used again after calling eof().
      If there are bytes in `out' already, the new bytes are appended,
      so the caller should do `out.setLength(0)' first if that's desired.
   */
  public final void eof(ByteBuf out) {
    // ignore the last incomplete token, if any -- it means "no trailing CRLF".
    token_length = 0;
    token = null;
  }
}
