/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 *
 * Created: Jamie Zawinski <jwz@netscape.com>,  3 Aug 1995.
 * Ported from C on 19 Aug 1997.
 */

package calypso.util;

import java.util.Date;
import java.util.TimeZone;
import calypso.util.ByteBuf;

/** Parses a date out of a string of bytes.  Call it like this:

    <P><UL>
        <TT>ByteBuf bytes = </TT>...<TT> ;</TT><BR>
        <TT>Date date = NetworkDate.parseDate(bytes);</TT>
    </UL>

    <P> Note that it operates on bytes, not chars, since network
    dates are always ASCII.

    <P> Why would you want to use this instead of
    <TT>java.text.DateFormat.parse()</TT>?  Because this algorithm has been
    tested in the field against real-world message headers for several
    years (the C code hadn't changed substantively since Netscape 2.0.)
    (There had been DST-related problems, but the tokenizer/parser was
    always sound.)

    @see Date
    @see Calendar
    @see java.text.DateFormat
 */

public class NetworkDate {

  private NetworkDate() { }

  static private final int UNKNOWN = 0;     // random unknown token

  static private final int SUN =    101;    // days of the week
  static private final int MON =    102;
  static private final int TUE =    103;
  static private final int WED =    104;
  static private final int THU =    105;
  static private final int FRI =    107;
  static private final int SAT =    108;

  static private final int JAN =    201;    // months
  static private final int FEB =    202;
  static private final int MAR =    203;
  static private final int APR =    204;
  static private final int MAY =    205;
  static private final int JUN =    206;
  static private final int JUL =    207;
  static private final int AUG =    208;
  static private final int SEP =    209;
  static private final int OCT =    210;
  static private final int NOV =    211;
  static private final int DEC =    212;

  static private final int PST =    301;    // a smattering of timezones
  static private final int PDT =    302;
  static private final int MST =    303;
  static private final int MDT =    304;
  static private final int CST =    305;
  static private final int CDT =    306;
  static private final int EST =    307;
  static private final int EDT =    308;
  static private final int AST =    309;
  static private final int NST =    310;
  static private final int GMT =    311;
  static private final int BST =    312;
  static private final int MET =    313;
  static private final int EET =    314;
  static private final int JST =    315;

  /** This parses a time/date string into a Time object.
   If it can't be parsed, -1 is returned.

   <P>Many formats are handled, including:

   <P><UL>
   <LI>  <TT> 14 Apr 89 03:20:12                </TT>
   <LI>  <TT> 14 Apr 89 03:20 GMT               </TT>
   <LI>  <TT> Fri, 17 Mar 89 4:01:33            </TT>
   <LI>  <TT> Fri, 17 Mar 89 4:01 GMT           </TT>
   <LI>  <TT> Mon Jan 16 16:12 PDT 1989         </TT>
   <LI>  <TT> Mon Jan 16 16:12 +0130 1989       </TT>
   <LI>  <TT> 6 May 1992 16:41-JST (Wednesday)  </TT>
   <LI>  <TT> 22-AUG-1993 10:59:12.82           </TT>
   <LI>  <TT> 22-AUG-1993 10:59pm               </TT>
   <LI>  <TT> 22-AUG-1993 12:59am               </TT>
   <LI>  <TT> 22-AUG-1993 12:59 PM              </TT>
   <LI>  <TT> Friday, August 04, 1995 3:54 PM   </TT>
   <LI>  <TT> 06/21/95 04:24:34 PM              </TT>
   <LI>  <TT> 20/06/95 21:07                    </TT>
   <LI>  <TT> 95-06-08 19:32:48 EDT             </TT>
   </UL>

  <P> But note that <TT>6/5/95</TT> is ambiguous, since it's not obvious
  which is the day and which is the month.  (<TT>6/13/95</TT> is not
  ambiguous, however.)

  @param buf                The bytes to parse.  This assumes the input to be
                            a sequence of 8-bit ASCII characters.  High-bit
                            characters are handled; 16-bit Unicode characters
                            are not.

  @param default_to_gmt     If the input string doesn't contain a description
    of the timezone, then this argument determines whether the string is
    interpreted relative to the local time zone (false) or to GMT (true).
    The correct value to pass in for this argument depends on what standard
    specified the time string which you are parsing; for RFC822 dates, this
    argument should be true.

  @return                   Microseconds since Jan 1, 1900, GMT.
                            You can pass this to <TT>new&nbsp;Date(long)</TT>.
                            Returns -1 if the string is unparsable
                            (no other negative value will ever be returned;
                            dates before the Epoch are not handled.)
  */

  public static long parseLong(ByteBuf buf, boolean default_to_gmt) {
    return parseLong(buf.toBytes(), 0, buf.length(), default_to_gmt);
  }

  /** The same, but takes a byte array and a region within it, instead
      of a ByteBuf. */
  public static long parseLong(byte string[], int start, int end,
                               boolean default_to_gmt) {

    int dotw  = UNKNOWN;
    int month = UNKNOWN;
    int zone  = UNKNOWN;

    int zone_offset = -1;
    int date = -1;
    int year = -1;
    int hour = -1;
    int min  = -1;
    int sec  = -1;

    int i = start;
    while (i < end) {

      byte c = string[i];
      byte c2 = ((i+1 < end) ? string[i+1] : 0);
      byte c3 = ((i+2 < end) ? string[i+2] : 0);

      switch (c) {
      case (byte)'a': case (byte)'A':
        if (month == UNKNOWN &&
            (c2 == 'p' || c2 == 'P') &&
            (c3 == 'r' || c3 == 'R'))
          month = APR;
        else if (zone == UNKNOWN &&
                 (c2 == 's' || c2 == 's') &&
                 (c3 == 't' || c3 == 'T'))
          zone = AST;
        else if (month == UNKNOWN &&
                 (c2 == 'u' || c2 == 'U') &&
                 (c3 == 'g' || c3 == 'G'))
          month = AUG;
        break;
      case (byte)'b': case (byte)'B':
        if (zone == UNKNOWN &&
            (c2 == 's' || c2 == 'S') &&
            (c3 == 't' || c3 == 'T'))
          zone = BST;
        break;
      case (byte)'c': case (byte)'C':
        if (zone == UNKNOWN &&
            (c2 == 'd' || c2 == 'D') &&
            (c3 == 't' || c3 == 'T'))
          zone = CDT;
        else if (zone == UNKNOWN &&
                 (c2 == 's' || c2 == 'S') &&
                 (c3 == 't' || c3 == 'T'))
          zone = CST;
        break;
      case (byte)'d': case (byte)'D':
        if (month == UNKNOWN &&
            (c2 == 'e' || c2 == 'E') &&
            (c3 == 'c' || c3 == 'C'))
          month = DEC;
        break;
      case (byte)'e': case (byte)'E':
        if (zone == UNKNOWN &&
            (c2 == 'd' || c2 == 'D') &&
            (c3 == 't' || c3 == 'T'))
          zone = EDT;
        else if (zone == UNKNOWN &&
                 (c2 == 'e' || c2 == 'E') &&
                 (c3 == 't' || c3 == 'T'))
          zone = EET;
        else if (zone == UNKNOWN &&
                 (c2 == 's' || c2 == 'S') &&
                 (c3 == 't' || c3 == 'T'))
          zone = EST;
        break;
      case (byte)'f': case (byte)'F':
        if (month == UNKNOWN &&
            (c2 == 'e' || c2 == 'E') &&
            (c3 == 'b' || c3 == 'B'))
          month = FEB;
        else if (dotw == UNKNOWN &&
                 (c2 == 'r' || c2 == 'R') &&
                 (c3 == 'i' || c3 == 'I'))
          dotw = FRI;
        break;
      case (byte)'g': case (byte)'G':
        if (zone == UNKNOWN &&
            (c2 == 'm' || c2 == 'M') &&
            (c3 == 't' || c3 == 'T'))
          zone = GMT;
        break;
      case (byte)'j': case (byte)'J':
        if (month == UNKNOWN &&
            (c2 == 'a' || c2 == 'A') &&
            (c3 == 'n' || c3 == 'N'))
          month = JAN;
        else if (zone == UNKNOWN &&
                 (c2 == 's' || c2 == 'S') &&
                 (c3 == 't' || c3 == 'T'))
          zone = JST;
        else if (month == UNKNOWN &&
                 (c2 == 'u' || c2 == 'U') &&
                 (c3 == 'l' || c3 == 'L'))
          month = JUL;
        else if (month == UNKNOWN &&
                 (c2 == 'u' || c2 == 'U') &&
                 (c3 == 'n' || c3 == 'N'))
          month = JUN;
        break;
      case (byte)'m': case (byte)'M':
        if (month == UNKNOWN &&
            (c2 == 'a' || c2 == 'A') &&
            (c3 == 'r' || c3 == 'R'))
          month = MAR;
        else if (month == UNKNOWN &&
                 (c2 == 'a' || c2 == 'A') &&
                 (c3 == 'y' || c3 == 'Y'))
          month = MAY;
        else if (zone == UNKNOWN &&
                 (c2 == 'd' || c2 == 'D') &&
                 (c3 == 't' || c3 == 'T'))
          zone = MDT;
        else if (zone == UNKNOWN &&
                 (c2 == 'e' || c2 == 'E') &&
                 (c3 == 't' || c3 == 'T'))
          zone = MET;
        else if (dotw == UNKNOWN &&
                 (c2 == 'o' || c2 == 'O') &&
                 (c3 == 'n' || c3 == 'N'))
          dotw = MON;
        else if (zone == UNKNOWN &&
                 (c2 == 's' || c2 == 'S') &&
                 (c3 == 't' || c3 == 'T'))
          zone = MST;
        break;
      case (byte)'n': case (byte)'N':
        if (month == UNKNOWN &&
            (c2 == 'o' || c2 == 'O') &&
            (c3 == 'v' || c3 == 'V'))
          month = NOV;
        else if (zone == UNKNOWN &&
                 (c2 == 's' || c2 == 'S') &&
                 (c3 == 't' || c3 == 'T'))
          zone = NST;
        break;
      case (byte)'o': case (byte)'O':
        if (month == UNKNOWN &&
            (c2 == 'c' || c2 == 'C') &&
            (c3 == 't' || c3 == 'T'))
          month = OCT;
        break;
      case (byte)'p': case (byte)'P':
        if (zone == UNKNOWN &&
            (c2 == 'd' || c2 == 'D') &&
            (c3 == 't' || c3 == 'T'))
          zone = PDT;
        else if (zone == UNKNOWN &&
                 (c2 == 's' || c2 == 'S') &&
                 (c3 == 't' || c3 == 'T'))
          zone = PST;
        break;
      case (byte)'s': case (byte)'S':
        if (dotw == UNKNOWN &&
            (c2 == 'a' || c2 == 'A') &&
            (c3 == 't' || c3 == 'T'))
          dotw = SAT;
        else if (month == UNKNOWN &&
                 (c2 == 'e' || c2 == 'E') &&
                 (c3 == 'p' || c3 == 'P'))
          month = SEP;
        else if (dotw == UNKNOWN &&
                 (c2 == 'u' || c2 == 'U') &&
                 (c3 == 'n' || c3 == 'N'))
          dotw = SUN;
        break;
      case (byte)'t': case (byte)'T':
        if (dotw == UNKNOWN &&
            (c2 == 'h' || c2 == 'H') &&
            (c3 == 'u' || c3 == 'U'))
          dotw = THU;
        else if (dotw == UNKNOWN &&
                 (c2 == 'u' || c2 == 'U') &&
                 (c3 == 'e' || c3 == 'E'))
          dotw = TUE;
        break;
      case (byte)'u': case (byte)'U':
        if (zone == UNKNOWN &&
            (c2 == 't' || c2 == 'T') &&
            !(c3 >= 'A' && c3 <= 'Z') &&
            !(c3 >= 'a' && c3 <= 'z'))
          // UT is the same as GMT but UTx is not.
          zone = GMT;
        break;
      case (byte)'w': case (byte)'W':
        if (dotw == UNKNOWN &&
            (c2 == 'e' || c2 == 'E') &&
            (c3 == 'd' || c3 == 'D'))
          dotw = WED;
        break;



        // parsing timezone offsets

      case (byte)'+': case (byte)'-': {

        if (zone_offset >= 0) {
          // already got one...
          i++;
          break;
        }

        if (zone != UNKNOWN && zone != GMT) {
          // GMT+0300 is legal, but PST+0300 is not.
          i++;
          break;
        }

        int sign = ((c == '+') ? 1 : -1);
        i++; /* move over sign */

        int token_start = i;
        int token_end;

        for (token_end = token_start; token_end < end; token_end++) {
          c = string[token_end];
          if (c < '0' || c > '9')
            break;
        }

        if ((token_end - token_start) == 4)
          // GMT+0000: offset in HHMM
          zone_offset = (((((string[token_start]-'0') * 10) +
                           (string[token_start+1]-'0'))
                          * 60) +
                         (((string[token_start+2]-'0') * 10) +
                          (string[token_start+3]-'0')));
        else if ((token_end - token_start) == 2)
          // GMT+00: offset in hours
          zone_offset = (((string[token_start]-'0') * 10) +
                         (string[token_start+1]-'0')) * 60;
        else if ((token_end - token_start) == 1)
          // GMT+0: offset in hours
          zone_offset = (string[token_start]-'0') * 60;
        else
          /* three digits, or more than 4: unrecognised. */
          break;

        zone_offset *= sign;
        zone = GMT;
        break;
      }


      // parsing numeric tokens

      case (byte)'0': case (byte)'1': case (byte)'2': case (byte)'3': 
      case (byte)'4': case (byte)'5': case (byte)'6': case (byte)'7': 
      case (byte)'8': case (byte)'9': {

        int tmp_hour = -1;
        int tmp_min = -1;
        int tmp_sec = -1;

        int token_start = i;
        int token_end;

        // move token_end to the first non-digit, or end of string.
        for (token_end = token_start; token_end < end; token_end++) {
          c = string[token_end];
          if (c < '0' || c > '9')
            break;
        }

        if (token_end < end && c == ':') {
          // If there's a colon, this is an hour/minute specification.
          // If we've already set hour or min, ignore this one.
          if (hour > 0 && min > 0)
            break;

          // We have seen "[0-9]+:", so this is probably HH:MM[:SS]

          if ((token_end - token_start) == 2)
            // two digits then a colon -- it is the hour.
            tmp_hour = ((string[token_start]-'0') * 10 +
                        (string[token_start+1]-'0'));
          else if ((token_end - token_start) == 1)
            // one digit then a colon -- it is the hour.
            tmp_hour = (string[token_start]-'0');
          else
            break;

          // Got the hour; move over the colon, and parse the minutes.

          // move token_start just past the colon.
          token_start = token_end+1;
          if (token_start >= end)
            break;

          // move token_end to the first non-digit, or end of string.
          for(token_end = token_start; token_end < end; token_end++) {
            c = string[token_end];
            if (c < '0' || c > '9')
              break;
          }

          if ((token_end - token_start) == 2)
            // two digits followed by a colon or EOS -- it is the minutes.
            tmp_min = ((string[token_start]-'0') * 10 +
                       (string[token_start+1]-'0'));
          else if ((token_end - token_start) == 1)
            // one digit followed by a colon or EOS -- it is the minutes.
            tmp_min = (string[token_start]-'0');
          else
            break;

          // Got the hour and minutes; move over the colon, and try to
          // parse the seconds.

          token_start = token_end;
          if (token_end < end && string[token_end] == ':') {
            // move token_start just past the colon.
            token_start++;

            // move token_end to the first non-digit, or end of string.
            for(token_end = token_start; token_end < end; token_end++) {
              c = string[token_end];
              if (c < '0' || c > '9')
                break;
            }

            if ((token_end - token_start) == 2)
              // two digits followed by EOT -- it is the seconds.
              tmp_sec = ((string[token_start]-'0') * 10 +
                         (string[token_start+1]-'0'));
            else if ((token_end - token_start) == 1)
              // one digit followed by EOT -- it is the seconds.
              tmp_sec = (string[token_start]-'0');
          }

          // If we made it here, we've parsed hour and min,
          // and possibly sec, so it worked as a unit.

          // skip over whitespace and see if there's an AM or PM
          // directly following the time.
          if (token_end < end && tmp_hour <= 12) {

            for(; token_start < end; token_start++) {
              c = string[token_start];
              if (c < '0' || c > '9')
                break;
            }

            while (token_start < end &&
                   string[token_start] <= ' ')
              token_start++;

            if (token_start+1 < end) {
              if ((string[token_start] == 'p' ||
                   string[token_start] == 'P') &&
                  (string[token_start+1] == 'm' ||
                   string[token_start+1] == 'M'))
                // 10:05pm == 22:05, and 12:05pm == 12:05
                tmp_hour = (tmp_hour == 12 ? 12 : tmp_hour + 12);
              else if (tmp_hour == 12 &&
                       (string[token_start] == 'a' ||
                        string[token_start] == 'A') &&
                       (string[token_start+1] == 'm' ||
                        string[token_start+1] == 'M'))
                // 12:05am == 00:05
                tmp_hour = 0;
            }
          }

          // if we made it all the way to here, we can keep these values.
          // (We might have bugged out after parsing hour and before
          // parsing min, and it would be bad to take one and not the
          // other.)
          hour = tmp_hour;
          min = tmp_min;
          sec = tmp_sec;
          i = token_end-1;
          break;
        }

        else if (token_end+1 < end &&
                 (c == '/' || c == '-') &&
                 (string[token_end+1] >= '0' &&
                  string[token_end+1] <= '9')) {
          // Perhaps this is 6/16/95, 16/6/95, 6-16-95, or 16-6-95
          // or even 95-06-05...
          //   #### But it doesn't handle 1995-06-22.
          //
          int n1, n2, n3;

          if (month != UNKNOWN)
            // if we saw a month name, this can't be.
            break;

          token_start = i;

          // get the first one or two digits

          n1 = (string[token_start] - '0');
          token_start++;
          if (token_start+1 < end &&
              string[token_start] >= '0' &&
              string[token_start] <= '9') {
            n1 = (n1*10) + (string[token_start] - '0');
            token_start++;
          }

          // demand that the next char be / or -.

          if (token_start >= end-1 ||
              (string[token_start] != '/' &&
               string[token_start] != '-'))
            break;
          token_start++;

          // demand that the next char be a digit.

          if (string[token_start] < '0' ||
              string[token_start] > '9')
            break;

          // get the second group of one or two digits

          n2 = (string[token_start] - '0');
          token_start++;
          if (token_start+1 < end &&
              string[token_start] >= '0' &&
              string[token_start] <= '9') {
            n2 = (n2*10) + (string[token_start] - '0');
            token_start++;
          }

          // demand that the next char be / or -.

          if (token_start >= end-1 ||
              (string[token_start] != '/' &&
               string[token_start] != '-'))
            break;
          token_start++;

          // demand that the next char be a digit.

          if (string[token_start] < '0' ||
              string[token_start] > '9')
            break;

          // get the third group of one, two, or four digits

          n3 = (string[token_start] - '0');
          token_start++;

          if (token_start+1 <= end &&
              string[token_start] >= '0' &&
              string[token_start] <= '9') {
            // digit two
            n3 = (n3*10) + (string[token_start] - '0');
            token_start++;

            // digit three
            if (token_start+1 < end &&
                string[token_start] >= '0' &&
                string[token_start] <= '9') {

              // if we have digit three but not digit four, give up on it:
              // there are no three-digit-numbers in this context.
              if (token_start+2 > end ||
                  (string[token_start+1] < '0' &&
                   string[token_start+1] > '9'))
                break;

              n3 = (n3*10) + (string[token_start++] - '0');
              n3 = (n3*10) + (string[token_start++] - '0');
            }
          }

          // if the digits are followed by an alphanumeric, give up on it.
          if (token_start < end &&
              ((string[token_start] >= '0' &&
                string[token_start] <= '9') ||
               (string[token_start] >= 'a' &&
                string[token_start] <= 'z') ||
               (string[token_start] >= 'A' &&
                string[token_start] <= 'Z')))
            break;


          // At this point, we've parsed three 1-2 digit numbers, with / or -
          // between them.  Now decide what the hell they are (DD/MM/YY or
          // MM/DD/YY or YY/MM/DD.)

          if (n1 > 70) {    // must be YY/MM/DD
            if (n2 > 12) break;
            if (n3 > 31) break;
            year = n1;
            if (year < 1900) year += 1900;
            month = (n2 + JAN - 1);
            date = n3;
            i = token_start;
            break;
          }

          if (n3 < 70 ||                // before epoch - can't represent it.
              (n1 > 12 && n2 > 12)) {   // illegal
            i = token_start;
            break;
          }

          if (n3 < 1900) n3 += 1900;

          if (n1 > 12) {        // must be DD/MM/YY
            date = n1;
            month = (n2 + JAN - 1);
            year = n3;
          } else {          // assume MM/DD/YY
            // #### In the ambiguous case, should we consult the locale to
            // find out the local default?
            month = (n1 + JAN - 1);
            date = n2;
            year = n3;
          }

          i = token_start;
        }

        else if (token_end+1 < end &&
                 (string[token_end] >= 'A' ||
                  string[token_end] <= 'Z') &&
                 (string[token_end+1] >= 'a' &&
                  string[token_end+1] <= 'z')) {
          // Digits followed by non-punctuation - what's that?
        }

        else if ((token_end - token_start) == 4) {
          // four digits in a row must be a year.
          if (year < 0)
            year = ((string[i]-'0')   * 1000 +
                    (string[i+1]-'0') * 100 +
                    (string[i+2]-'0') * 10 +
                    (string[i+3]-'0'));
        }

        else if ((token_end - token_start) == 2) {
          // two digits in a row might be a date, or a year

          int n = ((string[i]-'0') * 10 +
                   (string[i+1]-'0'));
          // If we don't have a date (day of the month) and we see a number
          // less than 32, then assume that is the date.
          //
          // Otherwise, if we have a date and not a year, assume this is the
          // year.  If it is less than 70, then assume it refers to the 21st
          // century.  If it is two digits (>= 70), assume it refers to the
          // 20th century.  Otherwise, assume it refers to an unambiguous year.
          //
          // The world will surely end soon.
          //
          if (date < 0 && n < 32)
            date = n;
          else if (year < 0) {
            if (n < 70)
              year = 2000 + n;
            else if (n < 100)
              year = 1900 + n;
            else
              year = n;
          }
        }

        else if ((token_end - token_start) == 1) {
          // one digit all alone -- it must be a date.
          if (date < 0)
            date = string[i]-'0';
        }

        else {
          // else, three or more than four digits - what's that?
          break;
        }
      }
      } // closes switch

      // Skip to the end of this token, whether we parsed it or not.
      // Tokens are delimited by whitespace, or ,;-/
      // But explicitly not :+-.

      for (; i < end; i++) {
        c = string[i];
        if (c <= ' ' || c == ',' || c == ';' || c == '-' || c == '+' ||
            c == '/' || c == '(' || c == ')' || c == '[' || c == ']')
          break;
      }

      boolean done = false;
      while (!done) {
        done = true;
        // skip over uninteresting chars before the next token.
        for (; i < end; i++) {
          c = string[i];
          if (! (c <= ' ' || c == ',' || c == ';' || c == '/' ||
                 c == '(' || c == ')' || c == '[' || c == ']'))
            break;
        }

        // "-" is ignored at the beginning of a token if we have not yet
        // parsed a year, or if the character after the dash is not a digit.
        if (i < end &&
            c == '-' &&
            (year < 0 ||
             string[i+1] < '0' ||
             string[i+1] > '9')) {
          i++;
          done = false;
        }
      }
    }

    // done parsing string -- turn symbolic zones into numeric offsets.

    if (zone != UNKNOWN && zone_offset == -1) {
      switch (zone) {
      case PST: zone_offset = -8 * 60; break;
      case PDT: zone_offset = -7 * 60; break;
      case MST: zone_offset = -7 * 60; break;
      case MDT: zone_offset = -6 * 60; break;
      case CST: zone_offset = -6 * 60; break;
      case CDT: zone_offset = -5 * 60; break;
      case EST: zone_offset = -5 * 60; break;
      case EDT: zone_offset = -4 * 60; break;
      case AST: zone_offset = -4 * 60; break;
      case NST: zone_offset = -3 * 60 - 30; break;
      case GMT: zone_offset =  0 * 60; break;
      case BST: zone_offset =  1 * 60; break;
      case MET: zone_offset =  1 * 60; break;
      case EET: zone_offset =  2 * 60; break;
      case JST: zone_offset =  9 * 60; break;
      }
    }

    // If we didn't find a year, month, or day-of-the-month, we can't
    // possibly parse this, so just give up.  (In fact, in the Unix C version
    // of this code, I'd seen mktime() do something random -- it always
    // returned "Tue Feb  5 06:28:16 2036", which is no doubt a
    // numerologically significant date...
    //
    if (month == UNKNOWN || date == -1 || year == -1)
      return -1;

    if (zone_offset == -1) {
      if (default_to_gmt)
        zone_offset = 0;
      else
        zone_offset = localZoneOffset();
    }

    return UTC((year - 1900),
               (month - JAN),
               date,
               (hour == -1 ? 0 : hour),
               (min  == -1 ? 0 : min) - zone_offset,
               (sec  == -1 ? 0 : sec));
  }

  /** The same, but assumes GMT is the default timezone. */
  public static long parseLong(ByteBuf string) {
    return parseLong(string, true);
  }

  /** Like parseLong(), but returns a new Date object instead. */
  public static Date parseDate(ByteBuf string, boolean default_to_gmt) {
    long date = parseLong(string, default_to_gmt);
    if (date == -1) return null;
    return new Date(date);
  }

  /** The same, but assumes GMT is the default timezone. */
  public static Date parseDate(ByteBuf string) {
    return parseDate(string, true);
  }


  /** Composes the given date into a number suitable for passing to
      <TT>new&nbsp;Date(n)</TT>.  This is the number of milliseconds
      past the Epoch.
   */
  public static long UTC(int year, int month, int date,
                         int hour, int min, int sec) {
    long day = (date
                + monthOffset[month]
                + ((((year & 3) != 0)
                    || ((year % 100 == 0) && ((year + 300) % 400 != 0))
                    || (month < 2))
                   ? -1 : 0)  /* convert day-of-month to 0 based range,
                               * except following February in a leap year,
                               * in which case we skip the conversion to
                               * account for the extra day in February */
                + ((year - 70) * 365L)      // days per year
                + ((year - 69) / 4)         // plus leap days
                - ((year - 1) / 100)        // no leap on century years
                + ((year + 299) / 400));    // except %400 years
    return (1000 *
            ((sec + (60 * (min + (60 * hour))))
             + (60 * 60 * 24) * day));
    }

  static private final short monthOffset[] = {
    0,     // 31   January
    31,    // 28   February
    59,    // 31   March
    90,    // 30   April
    120,   // 31   May
    151,   // 30   June
    181,   // 31   July
    212,   // 31   August
    243,   // 30   September
    273,   // 31   October
    304,   // 30   November
    334    // 31   December
           // 365
  };

  static private int localZoneOffset() {
    TimeZone z = TimeZone.getDefault();
    int off = z.getRawOffset() / (1000 * 60);
    if (z.inDaylightTime(new Date()))           // Consing!!
      off += 60;
    return off;
  }


/*
  private static void debug_print(int dotw, int month, int zone, int offset,
                                  int date, int year, int hour, int min,
                                  int sec) {
    System.out.println("dotw  = " + debug_token_to_string(dotw)
                       + " (" + dotw + ")");
    System.out.println("date  = " + date);
    System.out.println("month = " + debug_token_to_string(month)
                       + " (" + month + ")");
    System.out.println("year  = " + year);
    System.out.println("hour  = " + hour);
    System.out.println("min   = " + min);
    System.out.println("sec   = " + sec);
    System.out.println("zone  = " + debug_token_to_string(zone)
                       + " (" + zone + ")");
    System.out.println("off   = " + offset);
    System.out.println("");
  }

  private static String debug_token_to_string(int t) {
    switch (t) {
    case UNKNOWN: return "UNKNOWN"; case SUN: return "SUN";
    case MON: return "MON"; case TUE: return "TUE"; case WED: return "WED";
    case THU: return "THU"; case FRI: return "FRI"; case SAT: return "SAT";
    case JAN: return "JAN"; case FEB: return "FEB"; case MAR: return "MAR";
    case APR: return "APR"; case MAY: return "MAY"; case JUN: return "JUN";
    case JUL: return "JUL"; case AUG: return "AUG"; case SEP: return "SEP";
    case OCT: return "OCT"; case NOV: return "NOV"; case DEC: return "DEC";
    case PST: return "PST"; case PDT: return "PDT"; case MST: return "MST";
    case MDT: return "MDT"; case CST: return "CST"; case CDT: return "CDT";
    case EST: return "EST"; case EDT: return "EDT"; case AST: return "AST";
    case NST: return "NST"; case GMT: return "GMT"; case BST: return "BST";
    case MET: return "MET"; case EET: return "EET"; case JST: return "JST";
    default: return "???";
    }
  }
 */
}


  /*
    We only handle a few time zones, but in case someone is feeling ambitious,
    here are a bunch of other timezone names and numbers.

    It's generally frowned upon to put timezone names in date headers instead
    of GMT offsets, so the small set of zone names that we already handle is
    probably sufficient.

    ---------------------------------------------------------------------
    I lifted this list of time zone abbreviations out of a UNIX computers
    setup file (specifically, from an AT&T StarServer running UNIX System V
    Release 4, in the /usr/lib/local/TZ directory).

    The list is by no means complete or comprehensive, as much of it comes
    out of scripts designed to adjust the time on computers when Daylight
    Savings Time (DST) rolls around. Also, I would consider it at least a
    little suspect. First, because it was compiled by Americans, and you know
    how us Americans are with geography :). Second, the data looks to be a
    little old, circa 1991 (note the reference to the "Soviet Union").

    The first column is an approximate name for the time zone described, the
    second column gives the time relative to GMT, the third column takes a
    stab at listing the country that the time zone is in, and the final
    column gives one or more abbreviations that apply to that time zone (note
    that abbreviations that end with "DST" or with "S" as the middle letter
    indicate Daylight Savings Time is in effect).

    I've also tried to roughly divide the listings into geographical
    groupings.

    Hope this helps,

    Raymond McCauley
    Texas A&M University
    scooter@tamu.edu

    <List follows...>
    =================

    Europe

    Great Britain 0:00 GB-Eire GMT, BST
    Western European nations +0:00 W-Eur WET, WET DST
    Iceland +0:00 - WET
    Middle European nations +1:00 M-Eur MET, MET DST
    Poland +1:00 W-Eur MET, MET DST
    Eastern European nations +2:00 E-Eur EET, EET DST
    Turkey +3:00 Turkey EET, EET DST
    Warsaw Pact/Soviet Union +3:00 M-Eur ????

    North America

    Canada/Newfoundland -3:30 Canada NST, NDT
    Canada/Atlantic -4:00 Canada AST, ADT
    Canada/Eastern -5:00 Canada EST, EDT
    Canada/Central -6:00 Canada CST, CDT
    Canada/East-Saskatchewan -6:00 Canada CST
    Canada/Mountain -7:00 Canada MST, MDT
    Canada/Pacific -8:00 Canada PST, PDT
    Canada/Yukon -9:00 Canada YST, YDT

    US/Eastern -5:00 US EST, EDT
    US/Central -6:00 US CST, CDT
    US/Mountain -7:00 US MST, MDT
    US/Pacific -8:00 US PST, PDT
    US/Yukon -9:00 US YST, YDT
    US/Hawaii -10:00 US HST, PST, PDT, PPET

    Mexico/BajaNorte -8:00 Mexico PST, PDT
    Mexico/BajaSur -7:00 Mexico MST
    Mexico/General -6:00 Mexico CST

    South America

    Brazil/East -3:00 Brazil EST, EDT
    Brazil/West -4:00 Brazil WST, WDT
    Brazil/Acre -5:00 Brazil AST, ADT
    Brazil/DeNoronha -2:00 Brazil FST, FDT

    Chile/Continental -4:00 Chile CST, CDT
    Chile/EasterIsland -6:00 Chile EST, EDT


    Asia

    People's Repub. of China +8:00 PRC CST, CDT
    (Yes, they really have only one time zone.)

    Republic of Korea +9:00 ROK KST, KDT

    Japan +9:00 Japan JST
    Singapore +8:00 Singapore SST
    Hongkong +8:00 U.K. HKT
    ROC +8:00 - CST

    Middle East

    Israel +3:00 Israel IST, IDT
    (This was the only listing I found)

    Australia

    Australia/Tasmania +10:00 Oz EST
    Australia/Queensland +10:00 Oz EST
    Australia/North +9:30 Oz CST
    Australia/West +8:00 Oz WST
    Australia/South +9:30 Oz CST


    Hour    TZN     DZN     Zone    Example
    0       GMT             Greenwich Mean Time             GMT0
    0       UTC             Universal Coordinated Time      UTC0
    2       FST     FDT     Fernando De Noronha Std         FST2FDT
    3       BST             Brazil Standard Time            BST3
    3       EST     EDT     Eastern Standard (Brazil)       EST3EDT
    3       GST             Greenland Standard Time         GST3
    3:30    NST     NDT     Newfoundland Standard Time      NST3:30NDT
    4       AST     ADT     Atlantic Standard Time          AST4ADT
    4       WST     WDT     Western Standard (Brazil)       WST4WDT
    5       EST     EDT     Eastern Standard Time           EST5EDT
    5       CST     CDT     Chile Standard Time             CST5CDT

    Hour    TZN     DZN     Zone    Example
    5       AST     ADT     Acre Standard Time              AST5ADT
    5       CST     CDT     Cuba Standard Time              CST5CDT
    6       CST     CDT     Central Standard Time           CST6CDT
    6       EST     EDT     Easter Island Standard          EST6EDT
    7       MST     MDT     Mountain Standard Time          MST7MDT
    8       PST     PDT     Pacific Standard Time           PST8PDT
    9       AKS     AKD     Alaska Standard Time            AKS9AKD
    9       YST     YDT     Yukon Standard Time             YST9YST
    10      HST     HDT     Hawaii Standard Time            HST10HDT
    11      SST             Somoa Standard Time             SST11
    -12     NZS     NZD     New Zealand Standard Time       NZS-12NZD
    -10     GST             Guam Standard Time              GST-10
    -10     EAS     EAD     Eastern Australian Standard     EAS-10EAD
    -9:30   CAS     CAD     Central Australian Standard     CAS-9:30CAD
    -9      JST             Japan Standard Time             JST-9
    -9      KST     KDT     Korean Standard Time            KST-9KDT
    -8      CCT             China Coast Time                CCT-8
    -8      HKT             Hong Kong Time                  HKT-8
    -8      SST             Singapore Standard Time         SST-8
    -8      WAS     WAD     Western Australian Standard     WAS-8WAD
    -7:30   JT              Java Standard Time              JST-7:30
    -7      NST             North Sumatra Time              NST-7
    -5:30   IST             Indian Standard Time            IST-5:30
    -3:30   IST     IDT     Iran Standard Time              IST-3:30IDT
    -3      MSK     MSD     Moscow Winter Time              MSK-3MSD
    -2      EET             Eastern Europe Time             EET-2
    -2      IST     IDT     Israel Standard Time            IST-2IDT
    -1      MEZ     MES     Middle European Time            MEZ-1MES
    -1      SWT     SST     Swedish Winter Time             SWT-1SST
    -1      FWT     FST     French Winter Time              FWT-1FST
    -1      CET     CES     Central European Time           CET-1CES
    -1 WAT West African Time WAT-1
  */
