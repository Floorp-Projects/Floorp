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
 * Created: Jamie Zawinski <jwz@netscape.com>, 26 Sep 1997.
 */

package grendel.util;

import calypso.util.ByteBuf;

/** This class generates globally-unique message IDs.
    These IDs conform to RFC 822, RFC 1036, and the
    so-called son-of-1036 draft.
  */
public class MessageIDGenerator {

  private MessageIDGenerator() { }  // only static methods


  /** Generates a new Message-ID string.
      @param hostname   The `domain' part of the ID to generate.
                        <P>
                        Generally, this should be the fully-qualified name of
                        the local host.  In situations where that is impossible
                        to determine, or where the local host name might be
                        considered confidential information, the host part of
                        the return address of the authoring user will do.
                        <P>
                        If the provided host name is not syntactically valid,
                        an error is signalled.
                        <P>
                        The host name may be null; in that case, a random
                        sequence of characters will be used for the host name,
                        resulting in a syntactically valid Message-ID.  Note,
                        however, that this is strongly discouraged by the
                        relevant RFCs: Message-IDs should have valid host
                        names in them if at all possible.

      @return           The new ID, a String.
    */
  public static String generate(String host_name) {
    if (host_name != null) {
      checkHostName(host_name);
      if (host_name.length() == 0)
        host_name = null;
    }

    int radix = 36;  // keep the strings short
    long time = System.currentTimeMillis();             // millisec resolution
    long salt = Double.doubleToLongBits(Math.random()); // 64 random bits

    ByteBuf out = ByteBuf.Alloc();
    out.append((byte) '<');
    out.append(Long.toString(Math.abs(time), radix));
    out.append((byte) '.');
    out.append(Long.toString(Math.abs(salt), radix));
    out.append((byte) '@');

    if (host_name != null)
      out.append(host_name);
    else {
      out.append((byte) 'h');  // domain part must begin with a letter
      salt = (Double.doubleToLongBits(Math.random()) & 0xFFFFFFFFL); // 32 bits
      out.append(Long.toString(Math.abs(salt), radix));
    }
    out.append((byte) '>');

    String result = out.toString();
    ByteBuf.Recycle(out);
    return result;
  }

  private static void checkHostName(String string) {
    int L = string.length();
    for (int i = 0; i < L; i++) {
      char c = string.charAt(i);
      if (!Character.isLetterOrDigit(c) && c != '-' && c != '_' && c != '.')
        throw new Error("illegal character in host name for Message-ID: " +
                        "`" +
                        new String(new char[] { c }) +
                        "' (" + string + ")");
    }
  }
}
