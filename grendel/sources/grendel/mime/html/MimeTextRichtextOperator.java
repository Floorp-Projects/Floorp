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

import calypso.util.ByteBuf;
import grendel.mime.IMimeObject;
import java.io.PrintStream;

/** This class converts text/richtext to HTML.
    @see MimeHTMLOperator
 */

class MimeTextRichtextOperator extends MimeTextOperator {

  static boolean enriched = false;

  MimeTextRichtextOperator(IMimeObject object, PrintStream out) {
    super(object, out);
  }

  public void pushBytes(ByteBuf b) {
    throw new Error("unimplemented");
  }

  public void pushEOF() {
    throw new Error("unimplemented");
  }
}
