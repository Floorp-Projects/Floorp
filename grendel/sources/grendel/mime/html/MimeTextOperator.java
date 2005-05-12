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

import calypso.util.ByteBuf;
import grendel.mime.IMimeObject;
import java.io.PrintStream;

/** This class converts text/plain (or unknown text/ types) to HTML.
    @see MimeHTMLOperator
 */

class MimeTextOperator extends MimeLeafOperator {

  StringBuffer buffer = null;

  MimeTextOperator(IMimeObject object, PrintStream out) {
    super(object, out);
    buffer = new StringBuffer(200);
//    getOut().print("<PRE VARIABLE>");
  }

  void decodeBytesToUnicode(ByteBuf in, StringBuffer out) {
    // #### need to look at charset and do deep magic
    for (int i = 0; i < in.length(); i++) {
      out.append((char) in.byteAt(i));
    }
  }

  public void pushBytes(ByteBuf b) {
    buffer.setLength(0);
    decodeBytesToUnicode(b, buffer);
    TextHTMLConverter.quoteForHTML(buffer, true, true);
    String strData = buffer.toString();
    strData = strData.replace(" ", "&nbsp;");
    strData = strData.replace("\n", "<br>");
    getOut().print(strData);
  }

  public void pushEOF() {
    buffer = null;
//    getOut().print("</PRE>");
  }
}
