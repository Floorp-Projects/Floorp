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
 * Created: Jamie Zawinski <jwz@netscape.com>, 26 Aug 1997.
 */

package grendel.mime.html;

import grendel.mime.IMimeOperator;
import grendel.mime.IMimeObject;
import calypso.util.ByteBuf;
import javax.mail.internet.InternetHeaders;
import java.io.PrintStream;

/** This is the base class of objects which convert a MIME object into HTML.

    @see MimeOperator
 */

abstract class MimeHTMLOperator implements IMimeOperator {

  String content_type;
  InternetHeaders headers;
  private PrintStream out;

  MimeHTMLOperator(IMimeObject object, PrintStream o) {
    content_type = object.contentType();
    headers = object.headers();
    out = o;
  }

  final PrintStream getOut() {
    return out;
  }

  public IMimeOperator createChild(IMimeObject child_object) {
    return createChild(child_object, out);
  }

  public IMimeOperator createChild(IMimeObject child_object,
                                   PrintStream child_out) {

    String child_type = child_object.contentType();
    MimeHTMLOperator result;

    if (child_type == null)  // the untyped-text container
      result = new MimeContainerOperator(child_object, child_out);

    else if (child_type.equalsIgnoreCase("text/html"))
      result = new MimeTextHTMLOperator(child_object, child_out);

//    else if (child_type.equalsIgnoreCase("text/enriched"))
//      result = new MimeTextEnrichedOperator(child_object, child_out);

//    else if (child_type.equalsIgnoreCase("text/richtext"))
//      result = new MimeTextRichtextOperator(child_object, child_out);

//    else if (child_type.equalsIgnoreCase("text/x-vcard"))
//      result = new MimeTextVCardOperator(child_object, child_out);

    else if (child_type.regionMatches(true, 0, "text/", 0, 5))
      result = new MimeTextOperator(child_object, child_out);

    // #### ... more here ...

    else if (child_type.equalsIgnoreCase("message/rfc822") ||
             child_type.equalsIgnoreCase("message/news"))
      result = new MimeMessageOperator(child_object, child_out);

//    else if (child_type.equalsIgnoreCase("multipart/alternative"))
//      result = new MimeMultipartAlternativeOperator(child_object, child_out);

//    else if (child_type.equalsIgnoreCase("multipart/related"))
//      result = new MimeMultipartRelatedOperator(child_object, child_out);

//    else if (child_type.equalsIgnoreCase("multipart/signed"))
//      result = new MimeMultipartSignedOperator(child_object, child_out);

    else if (child_type.equalsIgnoreCase("multipart/appledouble") ||
             child_type.equalsIgnoreCase("multipart/header-set"))
      result = new MimeMultipartAppleDoubleOperator(child_object, child_out);

    else if (child_type.regionMatches(true, 0, "multipart/", 0, 10) ||
             child_type.equalsIgnoreCase("x-sun-attachment"))
      result = new MimeContainerOperator(child_object, child_out);

    else if (child_type.equalsIgnoreCase("message/external-body"))
      result = new MimeExternalBodyOperator(child_object, child_out);

    else
      result = new MimeExternalObjectOperator(child_object, child_out);

    return result;
  }

}
