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

package grendel.mime.html;

import java.lang.NullPointerException;
import grendel.mime.IMimeOperator;
import grendel.mime.IMimeObject;
import java.io.PrintStream;

public class MimeHTMLOperatorFactory {
  public static IMimeOperator Make(IMimeObject root_object) {
    return Make(root_object, System.out);
  }

  public static IMimeOperator Make(IMimeObject root_object, PrintStream out)
  {
    String ct = root_object.contentType();
    if (ct != null &&
        ct.equalsIgnoreCase("message/rfc822") ||
        ct.equalsIgnoreCase("message/news"))
      return new MimeMessageOperator(root_object, out);
    else
      throw new Error("currently, only message/rfc822 and message/news are" +
                      " valid top-level content types.");
  }
}
