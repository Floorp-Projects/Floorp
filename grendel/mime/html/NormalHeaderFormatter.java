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

package grendel.mime.html;

import calypso.util.ByteBuf;
import javax.mail.internet.InternetHeaders;

/** This class provides a method which knows how to convert an
    InternetHeaders object to a brief HTML representation, containing
    only the ``interesting'' headers.
  */

class NormalHeaderFormatter extends HeaderFormatter {

  // #### need a way to customize this list
  public String interesting_headers[] = {
    "Subject",
    "Resent-Comments",
    "Resent-Date",
    "Resent-Sender",              // Not shown if Resent-From is present.
    "Resent-From",
    "Resent-To",
    "Resent-Cc",
    "Date",
    "Sender",                     // Not shown if From is present.
    "From",
    "Reply-To",                   // Not shown if From has the same addr.
    "Organization",
    "To",
    "CC",
    "BCC",
    "Posted-To",
    "Newsgroups",                 // Not shown if Posted-To was the same.
    "Followup-To",
    "References",

    "X-Mailer",                   // jwz finds these useful for debugging...
    "X-Newsreader",
    "X-Posting-Software",
    "X-News-Posting-Software",
  };

  public void formatHeaders(InternetHeaders headers, StringBuffer output) {

    boolean did_from = false;
    boolean did_resent_from = false;
    boolean did_posted_to = false;

    startHeaderOutput(output);

    for (int i = 0; i < interesting_headers.length; i++) {
      String header = interesting_headers[i];

      // The Subject header gets written in bold.
      //
      if (header.equalsIgnoreCase("Subject"))
        writeSubjectHeader(header, headers, output);

      // Message-IDs get displayed clickably.
      //
      else if (header.equalsIgnoreCase("References") ||
               header.equalsIgnoreCase("Message-ID") ||
               header.equalsIgnoreCase("Resent-Message-ID") ||
               header.equalsIgnoreCase("Supersedes"))
        writeIDHeader(header, headers, output);

      // The From header supercedes the Sender header.
      //
      else if (header.equalsIgnoreCase("Sender") ||
               header.equalsIgnoreCase("From")) {
        if (!did_from) {
          did_from = true;
          if (!writeAddressHeader("From", headers, output))
            writeAddressHeader("Sender", headers, output);
        }
      }

      // Likewise, the Resent-From header supercedes the Resent-Sender header.
      //
      else if (header.equalsIgnoreCase("Resent-Sender") ||
               header.equalsIgnoreCase("Resent-From")) {
        if (!did_resent_from) {
          did_resent_from = true;
          if (!writeAddressHeader("Resent-From", headers, output))
            writeAddressHeader("Resent-Sender", headers, output);
        }
      }

      // Emit the Newsgroups header unless we've already emitted a Posted-To
      // header that had the same value.
      //
      else if (header.equalsIgnoreCase("Posted-To")) {
        did_posted_to = true;
        writeNewsgroupHeader(header, headers, output);
      }
      else if (header.equalsIgnoreCase("Newsgroups"))
        if (!did_posted_to)
          writeNewsgroupHeader(header, headers, output);
        else {
          did_posted_to = true;
          String newsgroups[] = headers.getHeader("Newsgroups");
          String posted_to[]  = headers.getHeader("Posted-To");
          if (posted_to == null ||  posted_to.length == 0 ||
              newsgroups == null || newsgroups.length == 0 ||
              !posted_to[0].equalsIgnoreCase(newsgroups[0]))
            writeNewsgroupHeader(header, headers, output);
        }

      // Emit the Reply-To header only if it differs from the From header.
      // (we just compare the `address' part.)
      //
      else if (header.equalsIgnoreCase("Reply-To")) {
        String reply_to[] = headers.getHeader("Reply-To");
        if (reply_to != null && reply_to.length != 0) {
          String froms[] = headers.getHeader("From");
          String from_addrs = (froms == null || froms.length == 0 ? null
                               : froms[0]);
          String repl_addrs = reply_to[0];

          // #### extract 822 addr-specs from from_addrs
          // #### extract 822 addr-specs from repl_addrs

          if (from_addrs == null || repl_addrs == null ||
              !from_addrs.equalsIgnoreCase(repl_addrs)) {
            writeAddressHeader("Reply-To", headers, output);
          }
        }
      }

      // Random other address headers.
      // These headers combine all occurences: that is, if there is more than
      // one CC field, all of them will be combined and presented as one.
      //
      else if (header.equalsIgnoreCase("Resent-To") ||
               header.equalsIgnoreCase("Resent-CC") ||
               header.equalsIgnoreCase("To") ||
               header.equalsIgnoreCase("CC") ||
               header.equalsIgnoreCase("BCC"))
        writeAddressHeader(header, headers, output);

      // Random other newsgroup headers.
      // These headers combine all occurences, as with address headers.
      //
      else if (header.equalsIgnoreCase("Newsgroups") ||
               header.equalsIgnoreCase("Followup-To") ||
               header.equalsIgnoreCase("Posted-To"))
        writeNewsgroupHeader(header, headers, output);

      // Everything else.
      // These headers don't combine: only the first instance of the header
      // will be shown, if there is more than one.
      //
      else
        writeRandomHeader(header, headers, output, false);
    }

    finishHeaderOutput(output);
  }


//  public static void main(String args[]) {
//    InternetHeaders h = new InternetHeaders();
//    h.addLine("From: Jamie Zawinski <jwz@netscape.com>");
//    h.addLine("organization: about:jwz");
//    h.addLine("Subject: this http://home/ neat URL");
//    h.addLine("Newsgroups: alt.foo,alt.bar");
//
//    StringBuffer o = new StringBuffer();
//    NormalHeaderFormatter f = new NormalHeaderFormatter();
//    f.formatHeaders(h, o);
//    getOut().println(o.toString());
//  }
}
