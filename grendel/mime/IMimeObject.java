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
 * Interface to extracting information from MIME objects.
 * Created: Jamie Zawinski <jwz@netscape.com>, 21 Aug 1997.
 */

package grendel.mime;

import javax.mail.internet.InternetHeaders;
import java.util.Enumeration;

/**
   This is an interface for extracting information from a MIME part,
   or a tree of MIME parts.
   @see IMimeParser
   @see IMimeOperator
 */

public interface IMimeObject {

  /** Returns the MIME content-type of this object.
      Note that under certain conditions, this may disagree with the
      content-type in the object's headers: the value returned by
      contentType() takes priority.

      Note also that the content-type may be null, in the specific
      case of the top-level body of a non-MIME message.
    */
  String contentType();

  /** Returns the headers which describe this object.
      This may be null in some cases, including the case of the
      outermost object.  These headers describe the object itself,
      not the contents of the object: so, for a message/rfc822 object,
      these would be the headers which contained a Content-Type of
      message/rfc822.  The interior headers, those describing the
      <I>body</I> of the object, are found in the child.
   */
  InternetHeaders headers();

  /** Returns the children of the object, or null if it has no children
      or is not a container at all.
   */
  Enumeration children();

  /** Returns the ID string of the MIME part.
      This is the parent-relative index string, like "1.2.3"
      as found in mailbox: and news: URLs.
      This will be null for the outermost part.
      The first child of the outer part is "1";
      the third child of that child is "1.3"; and so on.
    */
  String partID();

//  IMimeObject findChild(String partNumber);
//  String suggestedFileName();
//  ...various other utility routines...

}
