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

/**
   This is an interface for streaming parsing of MIME data.
   @see grendel.mime.parser.MimeParserFactory
   @see IMimeOperator
   @see IMimeObject
 */

public interface IMimeParser {

  /** Set the IMimeOperator associated with this parser.
      Since the parsers and operators both need access to each other,
      one will create a parser, then create an operator for it,
      then point the parser at the operator using this method.
      It is an error to try and change the operator once it has
      already been set.
    */
  void setOperator(IMimeOperator op);

  /** Feed bytes to be parsed into the parser.
    */
  void pushBytes(ByteBuf buffer);

  /** Inform the parser that no more bytes will be forthcoming.
    */
  void pushEOF();

  /** Returns a corresponding object implementing the IMimeObject interface.
      This will very likely be the same object.
    */
  IMimeObject getObject();
}
