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


/** This class provides a method which knows how to convert an
 * InternetHeaders object to an HTML representation.
 */

abstract class HeaderFormatter {
    abstract public void formatHeaders(InternetHeaders headers,
            StringBuilder output);
    
    /** The name of the default font to be used in the headers box at the top
     *of every Grendel email message */
    protected static final String HEADER_FONT_NAME = "Sans-Serif";
    
    /** Translates an RFC-mandated message header name (like "Subject")
     * into a localized string which should be presented to the user.
     */
    String localizeHeaderName(String header) {
        return header;
    }
    
    /** Called to translate plain-text to an HTML-presentable form. */
    void quoteHTML(StringBuilder string) {
        TextHTMLConverter.quoteForHTML(string, true, false);
    }
    
    /** Called when beginning to output a header block.  This opens the table. */
    void startHeaderOutput(StringBuilder output) {
        output.append("<TABLE CELLPADDING=2 CELLSPACING=0 BORDER=0>");
    }
    
    /** Called when done filling a header block.  This closes the table. */
    void finishHeaderOutput(StringBuilder output) {
        output.append("</TABLE>");
    }
    
    /*************************************************************************/
    
    boolean writeAddressHeader(String header, InternetHeaders headers,
            StringBuilder output) {
        String values[] = headers.getHeader(header);
        if (values == null || values.length == 0)
            return false;
        else {
            String value = "";
            for (int i = 0; i < values.length; i++) {
                if (i > 0) value += "\r\n\t";
                value += values[i];
            }
            return writeRandomHeader(header, value, output);
        }
    }
    
    boolean writeAddressHeader(String header, String value,
            StringBuilder output) {
        return writeAddressHeader(header, new StringBuilder(value), output);
    }
    
    boolean writeAddressHeader(String header, StringBuilder value,
            StringBuilder output) {
        // #### write me add a mailto url round the address
        return writeRandomHeader(header, value, output);
    }
    
    /*************************************************************************/
    
    boolean writeNewsgroupHeader(String header, InternetHeaders headers,
            StringBuilder output) {
        String values[] = headers.getHeader(header);
        if (values == null || values.length == 0)
            return false;
        else {
            String value = "";
            for (int i = 0; i < values.length; i++) {
                if (i > 0) value += "\r\n\t";
                value += values[i];
            }
            return writeNewsgroupHeader(header, value, output);
        }
    }
    
    boolean writeNewsgroupHeader(String header, String value,
            StringBuilder output) {
        return writeNewsgroupHeader(header, new StringBuilder(value), output);
    }
    
    boolean writeNewsgroupHeader(String header, StringBuilder value,
            StringBuilder output) {
        // #### write me
        return writeRandomHeader(header, value, output);
    }
    
    
    /*************************************************************************/
    
    boolean writeIDHeader(String header, InternetHeaders headers,
            StringBuilder output) {
        String values[] = headers.getHeader(header);
        if (values == null || values.length == 0)
            return false;
        else {
            String value = "";
            for (int i = 0; i < values.length; i++) {
                if (i > 0) value += "\r\n\t";
                value += values[i];
            }
            return writeIDHeader(header, value, output);
        }
    }
    
    boolean writeIDHeader(String header, String value, StringBuilder output) {
        return writeIDHeader(header, new StringBuilder(value), output);
    }
    
    boolean writeIDHeader(String header, StringBuilder value,
            StringBuilder output) {
        // #### write me
        return writeRandomHeader(header, value, value);
    }
    
    
    /*************************************************************************/
    
    boolean writeSubjectHeader(String header, InternetHeaders headers,
            StringBuilder output) {
        String values[] = headers.getHeader(header);
        if (values == null || values.length == 0)
            return false;
        else {
            String value = "";
            for (int i = 0; i < values.length; i++) {
                if (i > 0) value += "\r\n\t";
                value += values[i];
            }
            return writeSubjectHeader(header, value, output);
        }
    }
    
    boolean writeSubjectHeader(String header, String value,
            StringBuilder output) {
        return writeSubjectHeader(header, new StringBuilder(value), output);
    }
    
    boolean writeSubjectHeader(String header, StringBuilder value,
            StringBuilder output) {
        // #### write me
        return writeRandomHeader(header, value, output);
    }
    
    
    /*************************************************************************/
    
    boolean writeRandomHeader(String header, InternetHeaders headers,
            StringBuilder output,
            boolean all_p) {
        String values[] = headers.getHeader(header);
        if (values == null || values.length == 0)
            return false;
        else {
            String value = "";
            for (int i = 0; i < values.length; i++) {
                if (i > 0) value += "\r\n\t";
                value += values[i];
            }
            return writeRandomHeader(header, value, output);
        }
    }
    
    boolean writeRandomHeader(String header, String value, StringBuilder output) {
        return writeRandomHeader(header, new StringBuilder(value), output);
    }
    
    boolean writeRandomHeader(String header, String values[], StringBuilder output) {
        String value = "";
        for (int i = 0; i < values.length; i++) {
            if (i > 0) value += "\r\n\t";
            value += values[i];
        }
        return writeRandomHeader(header, value, output);
    }
    
    boolean writeRandomHeader(String header, StringBuilder value, StringBuilder output) {
        output.append("<TR><TH VALIGN=BASELINE ALIGN=RIGHT NOWRAP><FONT FACE=" + HEADER_FONT_NAME + ">");
        output.append(localizeHeaderName(header));
        output.append(": </FONT></TH><TD><FONT FACE=" + HEADER_FONT_NAME + ">");
        quoteHTML(value);
        output.append(value);
        output.append("</FONT></TD></TR>");
        return true;
    }
}
