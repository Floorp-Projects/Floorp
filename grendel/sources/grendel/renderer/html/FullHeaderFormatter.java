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
 * Created: Jamie Zawinski <jwz@netscape.com>, 31 Aug 1997.
 */

package grendel.renderer.html;

import calypso.util.ByteBuf;
import javax.mail.internet.InternetHeaders;
import javax.mail.Header;
import java.util.Enumeration;

public class FullHeaderFormatter extends HeaderFormatter {

public void formatHeaders(InternetHeaders headers, StringBuilder output) {

    startHeaderOutput(output);

    for (Enumeration e = headers.getAllHeaders(); e.hasMoreElements();) {
      Header hh = (Header) e.nextElement();
      String h = hh.getName();
      String v = hh.getValue();

      if (h.equalsIgnoreCase("Subject"))
        writeSubjectHeader(h, v, output);

      else if (h.equalsIgnoreCase("References") ||
               h.equalsIgnoreCase("Message-ID") ||
               h.equalsIgnoreCase("Resent-Message-ID") ||
               h.equalsIgnoreCase("Supersedes"))
        writeIDHeader(h, v, output);

      else if (h.equalsIgnoreCase("Sender") ||
               h.equalsIgnoreCase("From") ||
               h.equalsIgnoreCase("Resent-Sender") ||
               h.equalsIgnoreCase("Resent-From") ||
               h.equalsIgnoreCase("Reply-To") ||
               h.equalsIgnoreCase("Resent-To") ||
               h.equalsIgnoreCase("Resent-CC") ||
               h.equalsIgnoreCase("To") ||
               h.equalsIgnoreCase("CC") ||
               h.equalsIgnoreCase("BCC"))

        writeAddressHeader(h, v, output);

      else if (h.equalsIgnoreCase("Newsgroups") ||
               h.equalsIgnoreCase("Followup-To") ||
               h.equalsIgnoreCase("Posted-To"))
        writeNewsgroupHeader(h, v, output);

      else
        writeRandomHeader(h, v, output);
    }
    finishHeaderOutput(output);
  }
}
