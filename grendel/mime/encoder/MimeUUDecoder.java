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

/** Implements a uuencode -> plaintext decoder.
 */
public final class MimeUUDecoder extends MimeEncoder {

  // Maps ASCII characters to their corresponding uuencode 6-bit values.
  static private final byte map[] = {
     0,  0,  0,  0,  0,  0,  0,  0,      //   000-007
     0,  0,  0,  0,  0,  0,  0,  0,      //   010-017
     0,  0,  0,  0,  0,  0,  0,  0,      //   020-027
     0,  0,  0,  0,  0,  0,  0,  0,      //   030-037
     0,  1,  2,  3,  4,  5,  6,  7,      //   040-047   !"#$%&'
     8,  9, 10, 11, 12, 13, 14, 15,      //   050-057  ()*+,-./
    16, 17, 18, 19, 20, 21, 22, 23,      //   060-067  01234567
    24, 25, 26, 27, 28, 29, 30, 31,      //   070-077  89:;<=>?

    32, 33, 34, 35, 36, 37, 38, 39,      //   100-107  @ABCDEFG
    40, 41, 42, 43, 44, 45, 46, 47,      //   110-117  HIJKLMNO
    48, 49, 50, 51, 52, 53, 54, 55,      //   120-127  PQRSTUVW
    56, 57, 58, 59, 60, 61, 62, 63,      //   130-137  XYZ[\]^_
     0,  0,  0,  0,  0,  0,  0,  0,      //   140-147  `abcdefg
     0,  0,  0,  0,  0,  0,  0,  0,      //   150-157  hijklmno
     0,  0,  0,  0,  0,  0,  0,  0,      //   160-167  pqrstuvw
     0,  0,  0,  0,  0,  0,  0,  0,      //   170-177  xyz{|}~

     0,  0,  0,  0,  0,  0,  0,  0,      //   200-207
     0,  0,  0,  0,  0,  0,  0,  0,      //   210-217
     0,  0,  0,  0,  0,  0,  0,  0,      //   220-227
     0,  0,  0,  0,  0,  0,  0,  0,      //   230-237
     0,  0,  0,  0,  0,  0,  0,  0,      //   240-247
     0,  0,  0,  0,  0,  0,  0,  0,      //   250-257
     0,  0,  0,  0,  0,  0,  0,  0,      //   260-267
     0,  0,  0,  0,  0,  0,  0,  0,      //   270-277

     0,  0,  0,  0,  0,  0,  0,  0,      //   300-307
     0,  0,  0,  0,  0,  0,  0,  0,      //   310-317
     0,  0,  0,  0,  0,  0,  0,  0,      //   320-327
     0,  0,  0,  0,  0,  0,  0,  0,      //   330-337
     0,  0,  0,  0,  0,  0,  0,  0,      //   340-347
     0,  0,  0,  0,  0,  0,  0,  0,      //   350-357
     0,  0,  0,  0,  0,  0,  0,  0,      //   360-367
     0,  0,  0,  0,  0,  0,  0,  0,      //   370-377
  };

  static private final int BEGIN = 100;
  static private final int BODY  = 101;
  static private final int END   = 102;
  int state = BEGIN;


  ByteBuf line_buf = new ByteBuf(80);   // we buffer by lines while decoding
  ByteBuf out_buf  = new ByteBuf(80);   // line buffering output is faster


  private final void decode_line(ByteBuf out) {

    byte line[] = line_buf.toBytes();
    int length = line_buf.length();

    if (state == BODY &&
        length == 3 &&
        line[0] == 'e' &&
        line[1] == 'n' &&
        line[2] == 'd') {
      state = END;
      line_buf = null;

    } else if (state == BEGIN) {
      if (length >= 9 &&
          line[0] == 'b' &&
          line[1] == 'e' &&
          line[2] == 'g' &&
          line[3] == 'i' &&
          line[4] == 'n' &&
          line[5] == ' ') {
        state = BODY;
      }

    } else if (length > 1) {   // state == BODY

      // We map down `line', reading four bytes and writing three.

      out_buf.ensureCapacity(length);
      byte outb[] = out_buf.toBytes();
      int outi = 0;

      int i = map[line[0]];
      int lost = i - (((length - 1) * 3) / 4);

      // If lost is >0 get here, then the line is shorter than the length byte
      // at the beginning says it should be: it's internally inconsistent.  We
      // will parse from it what we can...
      //
      // This probably happened because some gateway stripped trailing
      // whitespace from the end of the line -- so pretend the line was padded
      // with spaces (which conveniently map to \000...)
      if (lost > 0)
        i -= lost;

      int in = 0;
      for (++in; i > 0; in += 4, i -= 3) {
        if (i >= 3) {
          // We read four; write three.
          outb[outi++] = (byte) (map[line[in]]   << 2 | map[line[in+1]] >> 4);
          outb[outi++] = (byte) (map[line[in+1]] << 4 | map[line[in+2]] >> 2);
          outb[outi++] = (byte) (map[line[in+2]] << 6 | map[line[in+3]]);

        } else {
          // Handle a line that isn't a multiple of 4 long.
          // (We read 1, 2, or 3, and will write 1 or 2.)
          outb[outi++]   = (byte)(map[line[in]]   << 2 | map[line[in+1]] >> 4);
          if (i == 2) {
            outb[outi++] = (byte)(map[line[in+1]] << 4 | map[line[in+2]] >> 2);
          }
        }
      }

      // If the line was truncated, pad the missing bytes with 0 (SPC). */
      while (lost > 0) {
        outb[outi++] = 0;
        lost--;
      }

      // Append to `out' a line at a time, so that we don't make one method
      // call per *byte*...  (It's more like one per 38 or something.)
      out.append(outb, 0, outi);
    }

    if (line_buf != null)
      line_buf.setLength(0);
  }


  /** Given a sequence of input bytes in the uuencode encoding, produces a
      sequence of unencoded output bytes.  Note that some (small) amount of
      buffering may be necessary, if the input byte stream didn't fall on an
      appropriate boundary.  If there are bytes in `out' already, the new
      bytes are appended, so the caller should do `out.setLength(0)' first
      if that's desired.
   */
  public final void translate(ByteBuf in, ByteBuf out) {
    if (line_buf == null)
      return;
    byte inb[] = in.toBytes();
    int in_length = in.length();
    for (int i = 0; i < in_length; i++) {
      if (inb[i] == '\r' || inb[i] == '\n') {
        if (line_buf.length() != 0) {
          decode_line(out);
          if (line_buf == null)
            return;
        }
      } else {
        line_buf.append(inb[i]);
      }
    }
  }

  /** Tell the uudecoder that no more input data will be forthcoming.  This
      may result in output, as a result of flushing the internal buffer.
      This object must not be used again after calling eof().  If there are
      bytes in `out' already, the new bytes are appended, so the caller should
      do `out.setLength(0)' first if that's desired.
   */
  public final void eof(ByteBuf out) {
    if (line_buf != null &&
        line_buf.length() != 0)
      decode_line(out);
    line_buf = null;
    out_buf = null;
  }
}
