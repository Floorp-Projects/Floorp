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
import calypso.util.ByteBuf;
import javax.mail.internet.InternetHeaders;
import java.util.StringTokenizer;


/** This class represents the X-Sun-Attachment type, which is the Content-Type
    assigned by that pile of garbage called MailTool.  This is not a MIME type
    per se, but it's very similar to multipart/mixed, so it's easy to parse.
    Lots of people use MailTool, so we're stuck with having to parse this
    idiotic, obsolete format.
    @see MimeMultipart
    @see MimeUntypedText
 */

/*  The format is this:

    = Content-Type is X-Sun-Attachment
    = parts are separated by lines of exactly ten dashes
    = just after the dashes comes a block of headers, including:

      X-Sun-Data-Type: (manditory)
            Values are Text, Postscript, Scribe, SGML, TeX, Troff, DVI,
            and Message.

      X-Sun-Encoding-Info: (optional)
            Ordered, comma-separated values, including Compress and Uuencode.

      X-Sun-Data-Name: (optional)
            File name, maybe.

      X-Sun-Data-Description: (optional)
            Longer text.

      X-Sun-Content-Lines: (manditory, unless Length is present)
            Number of lines in the body, not counting headers and the blank
            line that follows them.

      X-Sun-Content-Length: (manditory, unless Lines is present)
            Bytes, presumably using Unix line terminators.
 */

class MimeXSunAttachment extends MimeMultipart {

  public MimeXSunAttachment(String content_type, InternetHeaders headers) {
    super(content_type, headers);
  }

  protected void computeBoundary() {
    // handled in checkBoundary();
  }

  protected int checkBoundary(ByteBuf line_buf) {
    if (line_buf.length() <= 10)
      return BOUNDARY_NONE;
    else {
      byte line[] = line_buf.toBytes();
      if (line[0] == '-' && line[1] == '-' && line[2] == '-' &&
          line[3] == '-' && line[4] == '-' && line[5] == '-' &&
          line[6] == '-' && line[7] == '-' && line[8] == '-' &&
          line[9] == '-' &&
          (line[10] == '\r' || line[10] == '\n'))
        return BOUNDARY_SEPARATOR;
      else
        return BOUNDARY_NONE;
    }
  }

  protected void createMultipartChild() {
    child_headers = translateSunHeaders(child_headers);
    super.createMultipartChild();
  }

  private String getHeader(InternetHeaders ih, String name) {
    String h[] = ih.getHeader(name);
    if (h == null || h.length == 0)
      return null;
    else
      return h[0];
  }

  /** Translates x-sun-attachment sub-headers to legal MIME headers. */
  InternetHeaders translateSunHeaders(InternetHeaders in) {
    InternetHeaders out = new InternetHeaders();
    String dt = getHeader(in, "X-Sun-Data-Type");
    String cs = getHeader(in, "X-Sun-Charset");
    String ei = getHeader(in, "X-Sun-Encoding-Info");
    String dn = getHeader(in, "X-Sun-Data-Name");
    String dd = getHeader(in, "X-Sun-Data-Description");

    if (dn != null && dd != null && dn.equalsIgnoreCase(dd))
      dd = null;  // you losers.

    if (dt != null && dd != null && dt.equalsIgnoreCase(dd))
      dd = null;  // you complete losers.

    if (dt != null && dn != null && dt.equalsIgnoreCase(dn))
      dn = null;  // you complete and utter losers.

    String mime_type = null;
    String mime_enc = null;


    /* These are the magic types used by MailTool that I can determine.
       The only actual written spec I've found only listed the first few.
       The rest were found by inspection (both of real-world messages,
       and by running `strings' on the MailTool binary, and on the file
       /usr/openwin/lib/cetables/cetables (the "Class Engine", Sun's
       equivalent to .mailcap and mime.types.)
     */
    String sun_type_table[][] = {
      { "default",              "text/plain" },
      { "default-doc",          "text/plain" },
      { "text",                 "text/plain" },
      { "scribe",               "text/plain" },
      { "sgml",                 "text/plain" },
      { "tex",                  "text/plain" },
      { "troff",                "text/plain" },
      { "c-file",               "text/plain" },
      { "h-file",               "text/plain" },
      { "readme-file",          "text/plain" },
      { "shell-script",         "text/plain" },
      { "cshell-script",        "text/plain" },
      { "makefile",             "text/plain" },
      { "hidden-docs",          "text/plain" },
      { "message",              "message/rfc822" },
      { "mail-message",         "message/rfc822" },
      { "mail-file",            "text/plain" },
      { "gif-file",             "image/gif" },
      { "jpeg-file",            "image/jpg" },
      { "ppm-file",             "image/ppm" },
      { "pgm-file",             "image/x-portable-graymap" },
      { "pbm-file",             "image/x-portable-bitmap" },
      { "xpm-file",             "image/x-xpixmap" },
      { "ilbm-file",            "image/ilbm" },
      { "tiff-file",            "image/tiff" },
      { "photocd-file",         "image/x-photo-cd" },
      { "sun-raster",           "image/x-sun-raster" },
      { "audio-file",           "audio/basic" },
      { "postscript",           "application/postscript" },
      { "postscript-file",      "application/postscript" },
      { "framemaker-document",  "application/x-framemaker" },
      { "sundraw-document",     "application/x-sun-draw" },
      { "sunpaint-document",    "application/x-sun-paint" },
      { "sunwrite-document",    "application/x-sun-write" },
      { "islanddraw-document",  "application/x-island-draw" },
      { "islandpaint-document", "application/x-island-paint" },
      { "islandwrite-document", "application/x-island-write" },
      { "sun-executable",       "application/octet-stream" },
      { "default-app",          "application/octet-stream" },
      { "compress",             "application/x-compress" },
      { "default-compress",     "application/x-compress" },
      { "gzip",                 "application/x-gzip" },
      { "uuencode",             "application/x-uuencode" },
    };
    for (int i = 0; i < sun_type_table.length; i++) {
      if (dt.equalsIgnoreCase(sun_type_table[i][0])) {
        mime_type = sun_type_table[i][1];
        break;
      }
    }


    /* Translate x-sun-attachment sub-part encodings to legal MIME encodings.
       However, if the X-Sun-Encoding-Info field contains more than one
       encoding (that is, contains a comma) then assign it the encoding of
       the *rightmost* element in the list; and change its Content-Type to
       application/octet-stream.  Examples:

             Sun Type:                    Translates To:
        ==================            ====================
        type:     TEXT                type:     text/plain
        encoding: COMPRESS            encoding: x-compress

        type:     POSTSCRIPT          type:     application/x-compress
        encoding: COMPRESS,UUENCODE   encoding: x-uuencode

        type:     TEXT                type:     application/octet-stream
        encoding: UNKNOWN,UUENCODE    encoding: x-uuencode
       </PRE>
    */
    if (ei != null) {
      StringTokenizer st = new StringTokenizer(ei, " ,", false);
      String last_enc = null;
      String butlast_enc = null;

      while (st.hasMoreTokens()) {
        String t = st.nextToken();
        if (t.equalsIgnoreCase("adpcm-compress")) {
          // this "adpcm-compress" pseudo-encoding is some random junk that
          // MailTool adds to the encoding description of .AU files: we can
          // ignore it if it is the leftmost element of the encoding field.
          // (It looks like it's created via `audioconvert -f g721'.  Why?
          // Who knows.)  Ignore it.
        } else {
          butlast_enc = last_enc;
          last_enc = t;
        }
      }

      if (butlast_enc != null) {
        if (butlast_enc.equalsIgnoreCase("uuencode"))
          mime_type = "application/x-uuencode";
        else if (butlast_enc.equalsIgnoreCase("gzip"))
          mime_type = "application/x-gzip";
        else if (butlast_enc.equalsIgnoreCase("compress") ||
                 butlast_enc.equalsIgnoreCase("default-compress"))
          mime_type = "application/x-compress";
        else
          mime_type = "application/octet-stream";
      }

      if (last_enc != null) {
        if (last_enc.equalsIgnoreCase("uuencode")) {
          mime_enc = "x-uuencode";
        } else {
          mime_type = "application/octet-stream";
        }
      }
    }

    if (mime_type == null) {
      mime_type = "application/octet-stream";
      if (mime_enc == null && dt != null && dt.equalsIgnoreCase("uuencode"))
        mime_enc = "x-uuencode";
    }

    if (cs != null && mime_type.startsWith("text/"))
      mime_type += "; charset=\"" + cs + "\"";

    out.addHeaderLine("Content-Type: " + mime_type + "\r\n");

    if (mime_enc != null)
      out.addHeaderLine("Content-Transfer-Encoding: " + mime_enc + "\r\n");

    if (dn != null)
      out.addHeaderLine("Content-Disposition: inline; filename=\"" + dn +
                        "\"\r\n");

    if (dd != null)
      out.addHeaderLine("Content-Description: " + dd + "\r\n");

    return out;
  }
}
