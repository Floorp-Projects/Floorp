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
 * Created: Jamie Zawinski <jwz@netscape.com>, 21 Aug 1997.
 */

package grendel.mime.parser;

import grendel.mime.IMimeOperator;
import javax.mail.internet.InternetHeaders;

/** This class implements the parser for the multipart/digest MIME container,
    which differs from multipart/mixed only in that the default type (for
    parts with no type explicitly specified) is message/rfc822 instead of
    text/plain.
 */

class MimeMultipartDigest extends MimeMultipart {

  public MimeMultipartDigest(String content_type, InternetHeaders headers) {
    super(content_type, headers);
  }

  public String defaultSubpartType() { return "message/rfc822"; }
}
