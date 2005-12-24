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
import java.util.Enumeration;
import javax.mail.internet.InternetHeaders;

public class BriefHeaderFormatter extends HeaderFormatter {
    public String interesting_headers[] = {
        "Subject",
                "Date",
                "Sender",                     // Not shown if From is present.
                "From",
                "Reply-To",                   // Not shown if From has the same addr.
    };
    
    public void formatHeaders(InternetHeaders headers, StringBuilder output) {
        startHeaderOutput(output);
        
        // The Subject header gets written in bold.
        writeSubjectHeader("Subject", headers, output);
        
        // The From header supercedes the Sender header.
        if (!writeAddressHeader("From", headers, output)) {
            writeAddressHeader("Sender", headers, output);
        }
        // The Date header get's reformated
        writeRandomHeader("Date", headers, output, false);
        
        finishHeaderOutput(output);
    }
    
    /** Called when beginning to output a header block.  This opens the table. */
    void startHeaderOutput(StringBuilder output) {
        super.startHeaderOutput(output);
        output.append("<TR>");
    }
    
    /** Called when done filling a header block.  This closes the table. */
    void finishHeaderOutput(StringBuilder output) {
        output.append("</TR>");
        super.finishHeaderOutput(output);
    }
    
    boolean writeRandomHeader(String header, StringBuilder value, StringBuilder output) {
        output.append("<TH VALIGN=BASELINE ALIGN=RIGHT NOWRAP><FONT FACE=" + HEADER_FONT_NAME + ">");
        output.append(localizeHeaderName(header));
        output.append(": </FONT></TH><TD><FONT FACE=" + HEADER_FONT_NAME + ">");
        quoteHTML(value);
        output.append(value);
        output.append("</FONT></TD>");
        return true;
    }
    
}