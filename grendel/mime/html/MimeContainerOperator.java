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

import grendel.mime.IMimeOperator;
import grendel.mime.IMimeObject;
import calypso.util.ByteBuf;
import java.io.PrintStream;

/** This class converts multipart/ types to HTML: mainly this involves
    emitting HRs between the children.
 */

class MimeContainerOperator extends MimeHTMLOperator {

  boolean any_kids = false;

  MimeContainerOperator(IMimeObject object, PrintStream out) {
    super(object, out);
  }

  public IMimeOperator createChild(IMimeObject child) {

    if (any_kids)
      // Push out an HR between each child.
      getOut().print("<HR WIDTH=\"90%\" SIZE=4>");
    any_kids = true;

    return super.createChild(child);
  }

  public void pushBytes(ByteBuf b) {
    throw new Error("containers don't get bytes!");
  }

  public void pushEOF() {
  }
}
