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
 *
 * Contributors: Edwin Woudt <edwin@woudt.nl>
 */

package grendel.mime.html;


/** This class knows how to convert plain-text to HTML.
 */

public class TextHTMLConverter {

  /** Given a StringBuffer of text, alters that text in place to be displayable
      as HTML: the <, >, and & characters are converted to entities.

      @arg urls_too        If this argument is true, then any text in the
                           buffer that looks like a URL will have a link
                           wrapped around it pointing at that URL.

      @arg citations_too   If this argument is true, then if the line begins
                           with Usenet-style citation marks, it will be
                           wrapped in <CITE> </CITE> tags.

                           For either `urls_too' or `citations_too' to work,
                           the buffer must contain exactly one line of text.
                           If both of these arguments are false, the buffer
                           may fall on any boundary.
   */
  public static void quoteForHTML(StringBuffer text, boolean urls_too,
                                  boolean citations_too) {

    int in_length = text.length();

    for (int i = 0; i < in_length; i++) {
      char c = text.charAt(i);
      if (c == '<') {
        text.setCharAt(i, '&');
        text.insert(i+1, "lt;");
        in_length += 3;
        i += 3;

      } else if (c == '&') {
        text.setCharAt(i, '&');
        text.insert(i+1, "amp;");
        in_length += 4;
        i += 4;

      } else if (urls_too) {
        if (c > ' ' &&
            (i == 0 ||
             !Character.isLetterOrDigit((char) text.charAt(i-1))) &&
            isURLProtocol(text, i, in_length)) {
          int end;

          // found a URL protocol.  Now find the end of the URL.

          for (end = i; end < in_length; end++) {
            // These characters always mark the end of the URL.
            if (text.charAt(end) <= ' ' ||
                text.charAt(end) == '<' || text.charAt(end) == '>' ||
                text.charAt(end) == '`' || text.charAt(end) == ')' ||
                text.charAt(end) == '\'' || text.charAt(end) == '"' ||
                text.charAt(end) == ']' || text.charAt(end) == '}')
              break;
          }

          // Check for certain punctuation characters on the end, and strip
          // them off.
          while (end > i &&
                 (text.charAt(end-1) == '.' || text.charAt(end-1) == ',' ||
                  text.charAt(end-1) == '!' || text.charAt(end-1) == ';' ||
                  text.charAt(end-1) == '-' || text.charAt(end-1) == '?' ||
                  text.charAt(end-1) == '#'))
            end--;

          // if the url is less than 7 characters then we screwed up and got a
          // "news:" url or something which is worthless to us.  Exclude the A
          // tag in this case.
          //
          // Also exclude any URL that ends in a colon; those tend to be
          // internal and magic and uninteresting.
          //
          if ((end-i) > 7 &&
              text.charAt(end-1) != ':') {
            // extract the URL
            int url_length = end-i;
            char url[] = new char[url_length];
            text.getChars(i, end, url, 0);

            text.insert(i, "<A HREF=\"");
            i += 9;
            end += 9;
            in_length += 9;

            i = end-1;
            text.insert(i+1, "\">");
            i += 2;
            in_length += 2;

            text.insert(i+1, url);
            in_length += url_length;

            i++;
            end = i + url_length;
            while (i < end) {

              c = text.charAt(i);
              if (c == '<') {
                text.setCharAt(i, '&');
                text.insert(i+1, "lt;");
                in_length += 3;
                end += 3;
                i += 3;

              } else if (c == '&') {
                text.setCharAt(i, '&');
                text.insert(i+1, "amp;");
                in_length += 4;
                end += 4;
                i += 4;
              }

              i++;
            }

            text.insert(i, "</A>");
            i += 4;
            in_length += 4;

          } else {
            // move to the end of this URL for our next trip through the loop.
            i = end-1;
          }
        }
      }
    }

    if (citations_too) {

      // Decide whether this line is a quotation, and should be italicized.
      // This implements the following case-sensitive regular expression:
      //
      //        ^[ \t]*[A-Z]*[]>]
      //
      // Which matches these lines:
      //
      //        > blah blah blah
      //             > blah blah blah
      //        LOSER> blah blah blah
      //        LOSER] blah blah blah
      //
      int i;

      // skip over whitespace
      for (i = 0; i < in_length; i++)
        if (text.charAt(i) > ' ') break;

      // skip over ASCII uppercase letters
      for (; i < in_length; i++)
        if (text.charAt(i) < 'A' || text.charAt(i) > 'Z') break;

      if (i < in_length &&
          (text.charAt(i) == '>' || text.charAt(i) == ']') &&
          !sendmailFuckage(text, i, in_length)) {
        text.insert(i, "<CITE>");
        in_length += 6;
        text.insert(in_length, "</CITE><br>");
      }
    }
  }

  private final static boolean isURLProtocol(StringBuffer buf,
                                             int start, int length) {
    switch(buf.charAt(start)) {
    case 'a': case 'A':
      return matchSubstring(buf, start, length, "about:");
    case 'f': case 'F':
      return (matchSubstring(buf, start, length, "ftp:") ||
              matchSubstring(buf, start, length, "file:"));
    case 'g': case 'G':
      return matchSubstring(buf, start, length, "gopher:");
    case 'h': case 'H':
      return (matchSubstring(buf, start, length, "http:") ||
              matchSubstring(buf, start, length, "https:"));
    case 'm': case 'M':
      return (matchSubstring(buf, start, length, "mailto:") ||
              matchSubstring(buf, start, length, "mailbox:"));
    case 'n': case 'N':
      return matchSubstring(buf, start, length, "news:");
    case 'r': case 'R':
      return matchSubstring(buf, start, length, "rlogin:");
    case 's': case 'S':
      return matchSubstring(buf, start, length, "snews:");
    case 't': case 'T':
      return (matchSubstring(buf, start, length, "telnet:") ||
              matchSubstring(buf, start, length, "tn3270:"));
    case 'w': case 'W':
      return matchSubstring(buf, start, length, "wais:");
    case 'u': case 'U':
      return matchSubstring(buf, start, length, "urn:");
    default:
      return false;
    }
  }

  private final static boolean matchSubstring(StringBuffer buf, int start,
                                              int length, String string) {
    int L = string.length();
    if (length - start <= L) return false;
    for (int i = 0; i < L; i++) {
      if (Character.toLowerCase(string.charAt(i)) !=
          Character.toLowerCase(buf.charAt(start+i)))
        return false;
    }
    return true;
  }

  private final static boolean sendmailFuckage(StringBuffer buf,
                                               int start, int length) {
    return ((length - start) > 5 &&
            buf.charAt(start  ) == '>' &&
            buf.charAt(start+1) == 'F' &&
            buf.charAt(start+2) == 'r' &&
            buf.charAt(start+3) == 'o' &&
            buf.charAt(start+4) == 'm' &&
            buf.charAt(start+5) == ' ');
  }

  static void test(String x) {
    System.out.println("Testing: " + x);
    StringBuffer buf = new StringBuffer(x);
    quoteForHTML(buf, true, false);
    System.out.println("         " + buf.toString());
  }

  public static void main(String args[]) {
    test("foobar");
    test("<foobar>");
    test("gabba gabba <hey>");
    test("this is not http: a url");
    test("this is not hxxp:adssfafsa a url");
    test("this is http:adssfafsa a url");
    test("this is mailto: a url");
    test("this is mailto:jwz a url");
    test("this is ABOUT:JWZ?LOSSAGE=SPECTACULAR a url");
    test("this is ABOUT:JWZ?LOSSAGE=SPECTACULAR&egregious=very. a url");
    test("http://somewhere/fbi.cgi?huzza=&lt;zorch&gt;");
    test("---http://somewhere/fbi.cgi?huzza=&lt;zorch&gt;...");
  }
}
