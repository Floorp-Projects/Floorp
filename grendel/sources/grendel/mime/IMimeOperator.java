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

package grendel.mime;

import calypso.util.ByteBuf;
import java.util.Enumeration;

/** This is the interface of objects which operate on a parsed tree of
    MIME objects.  When an IMimeParser is created to parse a stream, an
    implementor of IMimeOperator is created along with it, to operate
    on the MIME objects as they are parsed.  Each IMimeOperator has
    an IMimeObject associated with it, representing the node currently
    being parsed.  It may use that to interrogate information like
    content-types and other header values.

    @see grendel.mime.html.MimeHTMLOperatorFactory
    @see IMimeObject
    @see IMimeParser
 */

public interface IMimeOperator {

  /** If the MimeObject is a leaf node, this method is called with the bytes
      of the body.  It may be called any number of times, including 0, and the
      blocks will not necessarily fall on line boundaries.
   */
  void pushBytes(ByteBuf b);

  /** If the MimeObject is not a leaf node, this method will be called each
      time a new child object is seen.  It should create and return a
      corresponding MimeOperator (which may or may not be of the same type
      as `this'.)
   */
  IMimeOperator createChild(IMimeObject object);

  /** This method is called when the end of the MimeObject's body has been
      seen.  It is called exactly once, and is called both for leaf and
      non-leaf MIME objects.  Neither pushBytes() nor createChild() will
      be called after pushEOF().
   */
  void pushEOF();
}
