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

import grendel.mime.IMimeObject;
import grendel.mime.IMimeParser;
import grendel.mime.IMimeOperator;

import calypso.util.ByteBuf;

import javax.mail.internet.InternetHeaders;

import java.io.InputStream;
import java.util.Enumeration;



/** This is the base class for objects implementing parsers for MIME parts.
    It implements the IMimeObject interface.

    <P> The class hierarchy is:
    <P>
    <UL>
        <LI> MimeObject (abstract)
        <UL>
            <LI> MimeContainer (abstract)
            <UL>
                <LI> MimeMultipart
                <UL>
                    <LI> MimeMultipartDigest
                    <LI> MimeXSunAttachment
                </UL>
                <LI> MimeMessageRFC822
                <UL>
                    <LI> MimeMessageExternalBody
                </UL>
                <LI> MimeDwimText
            </UL>
            <LI> MimeLeaf (abstract)
        </UL>
    </UL>
 */

abstract class MimeObject implements IMimeObject, IMimeParser {

  String content_type;          // definitive content-type (may be null)
  InternetHeaders headers;      // headers describing this object
  String id;                    // the part-number string, like "1.2.3"
  IMimeOperator operator;       // the operator to feed information to

  /** Creates a parser for a MIME object.

      @arg content_type         The MIME content type of this object.
                                If this is null, it will be extracted
                                from the headers argument.

      @arg headers              The headers describing this MIME object.
                                This arg may be null for the outermost
                                container (but in that case, the
                                content_type arg must be provided.)
   */
  public MimeObject(String content_type, InternetHeaders headers) {

    if (content_type == null && headers != null) {
      String hh[] = headers.getHeader("Content-Type");
      if (hh != null && hh.length != 0) {
        content_type = hh[0];
        // #### strip to first token
        int i = content_type.indexOf(';');
        if (i > 0)
          content_type = content_type.substring(0, i);
        content_type = content_type.trim();
      }
    }

    // Note: content_type may still be null.  That's ok.
    this.content_type = content_type;
    this.headers = headers;
  }

  public void setOperator(IMimeOperator op) {
    if (operator != null) throw new Error("operator already set");
    operator = op;
  }

  public IMimeObject getObject() { return this; }

  public abstract void pushBytes(ByteBuf buffer);

  public void pushEOF() {
    operator.pushEOF();
    operator = null;
  }

//  public void parseStream(InputStream in) {
//    ByteBuf b = new ByteBuf(10240);     /* Is there some way to ask the
//                                           stream what it's preferred buffer
//                                           size is? */
//    while (in.read(b) >= 0)
//      pushBytes(b);
//    b.setLength(0);
//    b = null;
//  }

  // For IMimeObject
  public String contentType() { return content_type; }
  public InternetHeaders headers() { return headers; }
  public String partID() { return id; }
  public abstract Enumeration children();
}
