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
 * Created: Jamie Zawinski <jwz@netscape.com>, 26 Aug 1997.
 */

package grendel.mime.html;

import grendel.mime.IMimeObject;
import calypso.util.ByteBuf;
import javax.mail.internet.InternetHeaders;
import java.io.PrintStream;

/** This class converts unknown MIME types to HTML representing
    an "attachment".
    @see MimeHTMLOperator
 */

class MimeExternalObjectOperator extends MimeLeafOperator {

  String part_id;

  MimeExternalObjectOperator(IMimeObject object, PrintStream out) {
    super(object, out);
    part_id = object.partID();
    writeBoxEarly();
  }

  void writeBoxEarly() {
    String url = "about:jwz";                   // ####
    formatAttachmentBox(headers, content_type, part_id, url, null, false);
  }

  void formatAttachmentBox(InternetHeaders headers, String type,
                           String link_name, String link_url, String body,
                           boolean all_headers_p) {

    String icon = "internal-gopher-binary";     // ####

    getOut().print("<CENTER>" +
                     "<TABLE CELLPADDING=8 CELLSPACING=1 BORDER=1>" +
                     "<TR><TD NOWRAP>");

//#### Temporarily removed because the current renderer doesn't get it --Edwin
//    if (icon != null) {
//      getOut().print("<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0>" +
//                       "<TR><TD NOWRAP VALIGN=CENTER>");
//      if (link_url != null)
//        getOut().print("<A HREF=\"" + link_url + "\">");
//      getOut().print("<IMG SRC=\"" + icon +
//                       "\" BORDER=0 ALIGN=MIDDLE ALT=\"\">");
//      if (link_url != null) getOut().print("</A>");
//      getOut().print("</TD><TD VALIGN=CENTER>");
//    }
//#### End of temporarily removed block --Edwin

    if (link_url != null) getOut().print("<A HREF=\"" + link_url + "\">");
    getOut().print(link_name);
    if (link_url != null) getOut().print("</A>");

//#### Temporarily removed because the current renderer doesn't get it --Edwin
//    if (icon != null)
//      getOut().print("</TD></TR></TABLE>");
//#### End of temporarily removed block --Edwin
    getOut().print("</TD><TD>");

    if (all_headers_p) {

      FullHeaderFormatter formatter = new FullHeaderFormatter();
      StringBuffer output = new StringBuffer(200);
      formatter.formatHeaders(headers, output);
      getOut().print(output.toString());

    } else {
      String cde[] = headers.getHeader("Content-Description");
      String desc = (cde == null || cde.length == 0 ? null : cde[0]);
      String name = null;

      ByteBuf buf = new ByteBuf(50);

      String cdi[] = headers.getHeader("Content-Disposition");
      String disp = (cdi == null || cdi.length == 0 ? null : cdi[0]);
// ####
//      if (disp != null)
//        name = headers.getParameter(buf, "filename");
//
//      if (name == null)
//        if (headers.getHeaderValue("Content-Type", buf, false, false))
//          name = headers.getParameter(buf, "name");

      if (name == null) {
        String n[] = headers.getHeader("Content-Name");
        if (n != null && n.length != 0)
          name = n[0];
      }

//#### Temporarily removed because the current renderer doesn't get it --Edwin
//      getOut().print("<TABLE CELLPADDING=0 CELLSPACING=0 BORDER=0>");
//
//      if (name != null)
//        getOut().print("<TR><TH VALIGN=BASELINE ALIGN=RIGHT NOWRAP>" +
//                         "Name" +  // #### l10n
//                         ": </TH><TD>" +
//                         name +
//                         "</TD></TR>");
//
//      if (type != null)
//        getOut().print("<TR><TH VALIGN=BASELINE ALIGN=RIGHT NOWRAP>" +
//                         "Type" +  // #### l10n
//                         ": </TH><TD>" +
//                         type +
//                         "</TD></TR>");
//
//      if (desc != null)
//        getOut().print("<TR><TH VALIGN=BASELINE ALIGN=RIGHT NOWRAP>" +
//                         "Description" +  // #### l10n
//                         ": </TH><TD>" +
//                         desc +
//                         "</TD></TR>");
//
//      getOut().print("</TABLE>");
//#### End of temporarily removed block --Edwin

//#### Temporarily inserted to get rendering working --Edwin
      if (name != null)
        getOut().print("<B>Name: </B>" + name + "<BR>");

      if (type != null)
        getOut().print("<B>Type: </B>" + type + "<BR>");

      if (desc != null)
        getOut().print("<B>Description: </B>" + desc + "<BR>");
//#### End of temporarily inserted block --Edwin

    }

    if (body != null) {
      getOut().print("<P><PRE>");
      // #### quote HTML
      getOut().print(body);
      getOut().print("</PRE>");
    }

    getOut().print("</TD></TR></TABLE></CENTER>");
  }


  public void pushBytes(ByteBuf b) {
    // Nothing to do with the bytes -- we only use the headers.
  }

  public void pushEOF() {
  }
}
