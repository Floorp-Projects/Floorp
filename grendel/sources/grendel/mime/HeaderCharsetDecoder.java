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
 * Created: Jamie Zawinski <jwz@netscape.com>,  7 Oct 1997.
 */

package grendel.mime;

import calypso.util.ByteBuf;

/** This class knows how to convert Unicode to and from the MIME encoding
    defined in RFC 2047.
  */
class HeaderCharsetDecoder {

  static HeaderCharsetDecoder decoder = null;

  private HeaderCharsetDecoder() { }  // only one instance

  static public HeaderCharsetDecoder Get() {
    if (decoder == null)
      synchronized(HeaderCharsetDecoder.class) {
        if (decoder == null)
          decoder = new HeaderCharsetDecoder();
      }
    return decoder;
  }


  /** Converts a ByteBuf of raw header bytes to a StringBuffer of Unicode
      characters, decoding the bytes as per the encoding rules of RFC 2047.
      This is used by getUnicodeHeader().

      @param in               The raw bytes to be decoded.
      @param out              The resultant Unicode characters.
      @param tokenizable_p    Whether the header from which these bytes came
                              was a tokenizable header (like "From") or an
                              unstructured-text header (like "Subject").
                              This information is needed because the decoding
                              rules are subtly different for the two types
                              of headers.
   */
  public void mimeToUnicode(ByteBuf in, StringBuffer out,
                            boolean tokenizable_p) {
    // #### write me
    byte inb[] = in.toBytes();
    int L = in.length();
    for (int i = 0; i < L; i++)
      out.append((char) inb[i]);
  }

  public void unicodeToMime(String in, ByteBuf out, boolean tokenizable_p) {
    // #### write me
    out.append(in.getBytes());
  }

}
