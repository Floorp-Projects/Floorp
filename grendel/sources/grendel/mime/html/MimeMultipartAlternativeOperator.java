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
import java.io.PrintStream;

/** This class converts the multipart/alternative  MIME type to HTML,
    by buffering the sub-parts and extracting the most appropriate one.
 */

class MimeMultipartAlternativeOperator extends MimeHTMLOperator {

  boolean done = false;
  IMimeObject saved_child = null;
  IMimeOperator current_child = null;
  ByteBuf saved_output_buffer = null;
  ByteBuf current_output_buffer = null;

  MimeMultipartAlternativeOperator(IMimeObject object, PrintStream out) {
    super(object, out);
  }

  private void closeOpenChild() {
/*
    if (current_child != null) {
      // we've just buffered up one direct child of this
      // multipart/alternative.  Examine it and see if it's
      // one we can display inline.  If it is, save it
      // (replacing the last one we saved, if any.)
      if (displayableInline(current_child.object)) {
        saved_child = current_child;
        saved_output_buffer = current_output_buffer;
      }
    }
*/
  }

  public IMimeOperator createChild(IMimeObject obj) {
    closeOpenChild();
    // make a new stream for this part, which writes into a
    // newly-allocated byte buffer.  (There may be two such
    // in existance at a time, the one we're filling, and the
    // last decent one we've seen.)
/*
    current_output_buffer = new CharArrayWriter();
    current_child =
      super.createChild(child,
                        new MimeOutputBuffer(current_output_buffer));
*/
    return current_child;
  }

  public void pushEOF() {
    // we've seen the end of this mult/alternative, and thus of
    // its last child.  Now is the time to write the output:
    // the buffer of the last "good" child we've seen, or if
    // there isn't one, of the last child we've seen period.
/*
    closeOpenChild();
    if (saved_output_buffer)
      writeOutput(saved_output_buffer);
    else if (current_output_buffer)
      writeOutput(current_output_buffer);
*/
  }

  public void pushBytes(ByteBuf b) {
    throw new Error("containers don't get bytes!");
  }
}
