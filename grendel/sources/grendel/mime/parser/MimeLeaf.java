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
import grendel.mime.encoder.MimeEncoder;
import grendel.mime.encoder.MimeBase64Decoder;
import grendel.mime.encoder.MimeQuotedPrintableDecoder;
import grendel.mime.encoder.MimeUUDecoder;

import calypso.util.ByteBuf;
import javax.mail.internet.InternetHeaders;
import java.util.Enumeration;


/** This abstract class represents the parsers for all MIME objects which are
    not containers for other MIME objects.  The implication of this is that
    they are MIME types which can have Content-Transfer-Encodings applied to
    their data.
    @see MimeContainer
 */

class MimeLeaf extends MimeObject {

  MimeEncoder decoder = null;
  ByteBuf decoded = null;

  public MimeLeaf(String content_type, InternetHeaders headers) {
    super(content_type, headers);

    decoder = null;
    if (headers != null) {
      String enc[] = headers.getHeader("Content-Transfer-Encoding");
      String encoding = (enc == null || enc.length == 0 ? null : enc[0]);

      if (encoding == null)
        ;
      else if (encoding.equalsIgnoreCase("base64"))
        decoder = new MimeBase64Decoder();
      else if (encoding.equalsIgnoreCase("quoted-printable"))
        decoder = new MimeQuotedPrintableDecoder();
      else if (encoding.equalsIgnoreCase("uuencode") ||
               encoding.equalsIgnoreCase("uue") ||
               encoding.equalsIgnoreCase("x-uuencode") ||
               encoding.equalsIgnoreCase("x-uue"))
        decoder = new MimeUUDecoder();
    }

    if (decoder != null)
      decoded = new ByteBuf();
  }

  public Enumeration children() { return null; }

  public void pushBytes(ByteBuf buffer) {

    if (decoder == null)
      // If there's no decoder, push the bytes through raw.
      operator.pushBytes(buffer);
    else {
      // If there is a decoder, decode them, and pus the result through.
      decoded.setLength(0);
      decoder.translate(buffer, decoded);
      operator.pushBytes(decoded);
    }
  }

  public void pushEOF()  {

    if (decoder != null) {
      // If there is a decoder, tell it we're at EOF, and give it a chance
      // to spit out its last batch of buffered bytes.  Then feed them
      // through to the operator.
      decoded.setLength(0);
      decoder.eof(decoded);
      if (decoded.length() != 0)
        operator.pushBytes(decoded);
      decoder = null;
      decoded = null;
    }

    super.pushEOF();
  }
}
