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
 * Created: Jamie Zawinski <jwz@netscape.com>,  2 Sep 1997.
 */

package grendel.mime.parser;

import javax.mail.internet.InternetHeaders;


/** This class implements the parser for the message/external-body MIME
    containers, which is essentially a glorified URL.  This behaves like
    a message, in that it contains interior headers; but the content-type
    on those headers refers to remote data, not to the type of a nested
    part.  So, we parse the interior headers, but always treat the body
    of message/external-body as text/plain.
    @see MimeLeaf
 */

class MimeMessageExternalBody extends MimeMessageRFC822 {

    public MimeMessageExternalBody(String content_type,
                                   InternetHeaders headers) {
      super(content_type, headers);
    }

  public MimeObject makeChild(String child_type,
                              InternetHeaders child_headers) {
    return new MimeLeaf("text/plain", child_headers);
  }
}
