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

package grendel.mime.html;

import grendel.mime.IMimeObject;
import grendel.mime.IMimeOperator;
import calypso.util.ByteBuf;
import javax.mail.internet.InternetHeaders;
import java.io.PrintStream;

/** This class converts the multipart/appledouble and multipart/header-set
    MIME type to HTML, by presenting one link for both parts, and then
    attempting to display the second part inline.
 */

class MimeMultipartAppleDoubleOperator extends MimeContainerOperator {

  boolean got_first_child = false;
  String part_id = null;

  MimeMultipartAppleDoubleOperator(IMimeObject object, PrintStream out) {
    super(object, out);
    part_id = object.partID();
  }

  public IMimeOperator createChild(IMimeObject child) {

    if (got_first_child)
      return super.createChild(child);
    else {
      got_first_child = true;
      return new MimeMultipartAppleDoubleRSRCOperator(child, getOut(),
                                                      headers, part_id);
    }
  }
}

class MimeMultipartAppleDoubleRSRCOperator extends MimeExternalObjectOperator {

  MimeMultipartAppleDoubleRSRCOperator(IMimeObject object,
                                       PrintStream out,
                                       InternetHeaders parent_headers,
                                       String parent_id) {
    super(object, out);

    String url = "about:jwz";       // ####
    String type = "Macintosh File"; // #### I18N
    formatAttachmentBox(parent_headers, type, parent_id, url, null, false);
  }

  void writeBoxEarly() { }
}
