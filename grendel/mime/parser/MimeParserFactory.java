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

package grendel.mime.parser;

import java.lang.NullPointerException;
import grendel.mime.IMimeOperator;
import grendel.mime.IMimeParser;

public class MimeParserFactory {
  public static IMimeParser Make(String toplevel_content_type) {

    if (toplevel_content_type == null)
      throw new NullPointerException("content-type must not be null");

    else if (toplevel_content_type.equalsIgnoreCase("message/rfc822") ||
             toplevel_content_type.equalsIgnoreCase("message/news"))
      return new MimeMessageRFC822(toplevel_content_type, null);

    else
      throw new Error("currently, only message/rfc822 and message/news are" +
                      " valid top-level content types.");
  }
}
