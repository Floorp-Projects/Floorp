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
 * Created: Jamie Zawinski <jwz@netscape.com>, 29 Aug 1997.
 */

package grendel.mime;

import java.io.RandomAccessFile;
import java.io.IOException;

import calypso.util.ByteBuf;
import grendel.mime.parser.MimeParserFactory;
import grendel.mime.html.MimeHTMLOperatorFactory;


/* This silly little class implements an operator which draws every MIME
   part as a labelled box, to give an overview of the MIME structure (and
   incidentally to show that all of the operators are closed properly,
   since the table won't show up if they're not.)
 */
class MimeDebugOperator implements IMimeOperator {
  String type;
  public MimeDebugOperator(IMimeObject obj) {
    type = obj.contentType();
    System.out.print("<P><TABLE BORDER CELLPADDING=8 CELLSPACING=4" +
                     (obj.headers() == null ? "" : " WIDTH=100%") +
                     ">");
    System.out.print("<TR><TH BGCOLOR=BLACK><FONT COLOR=#FFFFFF>" +
                     type +
                     (obj.partID() == null ? "" :
                      "<BR>" + obj.partID()) +
                     "</FONT></TH></TR>");
    System.out.print("<TR><TD ALIGN=CENTER VALIGN=CENTER BGCOLOR=#FFFFFF>\n");
  }
  public void pushBytes(ByteBuf b) { }
  public IMimeOperator createChild(IMimeObject object) {
    return new MimeDebugOperator(object);
  }
  public void pushEOF() {
    System.out.println("</TD></TR></TABLE><P><!-- " + this.type + " -->");
  }
}



class TestMime {
  public static void main(String args[]) {

    for (int i = 0; i < args.length; i++) {
      try {
        System.err.println("Starting...");
        long start = System.currentTimeMillis();
        RandomAccessFile file = new RandomAccessFile(args[i], "r");

        String toplevel_type = "message/rfc822";

        IMimeParser parser = MimeParserFactory.Make(toplevel_type);
        IMimeOperator op;

        if (true)
          op = MimeHTMLOperatorFactory.Make(parser.getObject());
        else
          op = new MimeDebugOperator(parser.getObject());

        parser.setOperator(op);

        byte[] bytes = new byte[1024];
        ByteBuf buf = new ByteBuf(bytes.length);
        int bread;
        do {
          bread = file.read(bytes, 0, bytes.length);
          if (bread > 0) {
            buf.setLength(0);
            buf.append(bytes, 0, bread);        // this sucks!
            parser.pushBytes(buf);
          }
        } while (bread >= 0);
        bytes = null;
        buf = null;
        parser.pushEOF();

        long end = System.currentTimeMillis();
        System.err.println("Took " + (end - start) + " milliseconds.");

      } catch (IOException e) {
        System.err.println("Got an IO exception: " + e.getMessage());
        e.printStackTrace();
        return;
      }
    }
  }
}
