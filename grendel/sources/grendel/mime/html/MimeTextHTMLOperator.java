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
 * Created: Jamie Zawinski <jwz@netscape.com>, 27 Aug 1997.
 */

package grendel.mime.html;

import grendel.mime.IMimeObject;
import calypso.util.ByteBuf;
import java.io.PrintStream;

/** This class converts text/html MIME parts to HTML.
    @see MimeHTMLOperator
 */

class MimeTextHTMLOperator extends MimeTextOperator {

  MimeTextHTMLOperator(IMimeObject object, PrintStream out) {
    super(object, out);
  }

  public void pushBytes(ByteBuf b) {
    buffer.setLength(0);
    decodeBytesToUnicode(b, buffer);
    getOut().print(buffer.toString());
  }

  public void pushEOF() {

    getOut().print("</td></td></td></td></td></td></td></td></td></td>" +
                     "</tr></tr></tr></tr></tr></tr></tr></tr></tr></tr>" +
                     "</table></table></table></table></table></table>" +
                     "</b></b></b></b></b></b></b></b></b></b></b></b></b>" +
                     "</ul></ul></ul></ul></ul></ul></ul></ul></ul></ul>");

    buffer = null;
  }
}
