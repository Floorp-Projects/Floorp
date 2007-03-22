/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Java XPCOM Bindings.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Javier Pedemonte (jhpedemonte@gmail.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.xpcom;

import java.util.Enumeration;
import java.util.StringTokenizer;

import org.mozilla.interfaces.nsISupports;
import org.mozilla.interfaces.nsIVersionComparator;


/**
 * Version strings are dot-separated sequences of version-parts.
 * <p>
 * A version-part consists of up to four parts, all of which are optional:
 * <br><code>
 * &lt;number-a&gt;&lt;string-b&gt;&lt;number-c&gt;
 * &lt;string-d (everything else)&gt;
 * </code> <p>
 * A version-part may also consist of a single asterisk "*" which indicates
 * "infinity".
 * <p>
 * Numbers are base-10, and are zero if left out.
 * Strings are compared bytewise.
 * <p>
 * For additional backwards compatibility, if "string-b" is "+" then
 * "number-a" is incremented by 1 and "string-b" becomes "pre".
 * <p> <pre>
 * 1.0pre1
 * < 1.0pre2  
 *   < 1.0 == 1.0.0 == 1.0.0.0
 *     < 1.1pre == 1.1pre0 == 1.0+
 *       < 1.1pre1a
 *         < 1.1pre1
 *           < 1.1pre10a
 *             < 1.1pre10
 * </pre>
 * Although not required by this interface, it is recommended that
 * numbers remain within the limits of a signed char, i.e. -127 to 128.
 */
public class VersionComparator implements nsIVersionComparator {

  public nsISupports queryInterface(String aIID) {
    return Mozilla.queryInterface(this, aIID);
  }

  /**
   * Compare two version strings
   * @param A   a version string
   * @param B   a version string
   * @return a value less than 0 if A < B;
   *         the value 0 if A == B;
   *         or a value greater than 0 if A > B
   */
  public int compare(String A, String B) {
    int result;
    String a = A, b = B;

    do {
      VersionPart va = new VersionPart();
      VersionPart vb = new VersionPart();
      a = parseVersionPart(a, va);
      b = parseVersionPart(b, vb);

      result = compareVersionPart(va, vb);
      if (result != 0) {
        break;
      }
    } while (a != null || b != null);

    return result;
  }

  private class VersionPart {
    int     numA = 0;
    String  strB;
    int     numC = 0;
    String  extraD;
  }

  private static String parseVersionPart(String aVersion, VersionPart result) {
    if (aVersion == null || aVersion.length() == 0) {
      return aVersion;
    }

    StringTokenizer tok = new StringTokenizer(aVersion.trim(), ".");
    String part = tok.nextToken();

    if (part.equals("*")) {
      result.numA = Integer.MAX_VALUE;
      result.strB = "";
    } else {
      VersionPartTokenizer vertok = new VersionPartTokenizer(part);
      try {
        result.numA = Integer.parseInt(vertok.nextToken());
      } catch (NumberFormatException e) {
        // parsing error; default to zero like 'strtol' C function
        result.numA = 0;
      }

      if (vertok.hasMoreElements()) {
        String str = vertok.nextToken();

        // if part is of type "<num>+"
        if (str.charAt(0) == '+') {
          result.numA++;
          result.strB = "pre";
        } else {
          // else if part is of type "<num><alpha>..."
          result.strB = str;

          if (vertok.hasMoreTokens()) {
            try {
              result.numC = Integer.parseInt(vertok.nextToken());
            } catch (NumberFormatException e) {
              // parsing error; default to zero like 'strtol' C function
              result.numC = 0;
            }
            if (vertok.hasMoreTokens()) {
              result.extraD = vertok.getRemainder();
            }
          }
        }
      }
    }

    if (tok.hasMoreTokens()) {
      // return everything after "."
      return aVersion.substring(part.length() + 1);
    }
    return null;
  }

  private int compareVersionPart(VersionPart va, VersionPart vb) {
    int res = compareInt(va.numA, vb.numA);
    if (res != 0) {
      return res;
    }

    res = compareString(va.strB, vb.strB);
    if (res != 0) {
      return res;
    }

    res = compareInt(va.numC, vb.numC);
    if (res != 0) {
      return res;
    }

    return compareString(va.extraD, vb.extraD);
  }

  private int compareInt(int n1, int n2) {
    return n1 - n2;
  }

  private int compareString(String str1, String str2) {
    // any string is *before* no string
    if (str1 == null) {
      return (str2 != null) ? 1 : 0;
    }

    if (str2 == null) {
      return -1;
    }

    return str1.compareTo(str2);
  }

}

/**
 * Specialized tokenizer for Mozilla version strings.  A token can
 * consist of one of the four sections of a version string: <code>
 * &lt;number-a&gt;&lt;string-b&gt;&lt;number-c&gt;
 * &lt;string-d (everything else)&gt;</code>.
 */
class VersionPartTokenizer implements Enumeration {

  String part;

  public VersionPartTokenizer(String aPart) {
    part = aPart;
  }

  public boolean hasMoreElements() {
    return part.length() != 0;
  }

  public boolean hasMoreTokens() {
    return part.length() != 0;
  }

  public Object nextElement() {
    if (part.matches("[\\+\\-]?[0-9].*")) {
      // if string starts with a number...
      int index = 0;
      if (part.charAt(0) == '+' || part.charAt(0) == '-') {
        index = 1;
      }

      while (index < part.length() && Character.isDigit(part.charAt(index))) {
        index++;
      }

      String numPart = part.substring(0, index);
      part = part.substring(index);
      return numPart;
    } else {
      // ... or if this is the non-numeric part of version string
      int index = 0;
      while (index < part.length() && !Character.isDigit(part.charAt(index))) {
        index++;
      }

      String alphaPart = part.substring(0, index);
      part = part.substring(index);
      return alphaPart;
    }
  }

  public String nextToken() {
    return (String) nextElement();
  }

  /**
   * Returns what remains of the original string, without tokenization.  This
   * method is useful for getting the <code>&lt;string-d (everything else)&gt;
   * </code> section of a version string.
   * 
   * @return remaining version string
   */
  public String getRemainder() {
    return part;
  }

}

