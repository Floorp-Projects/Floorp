/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil -*-
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
 */

package calypso.util;

/**
 * Simple attribute/value pair class
 *
 * XXX add in a recylcer
 * XXX add in a method to reconsitute one from an InputStream/String?
 */
public class AttributeValuePair {
  protected Atom fName;
  protected String fValue;

  public AttributeValuePair(String aName, String aValue) {
    this(Atom.Find(aName), aValue);
  }

  public AttributeValuePair(Atom aName, String aValue) {
    fName = aName;
    fValue = aValue;
  }

  public Atom getName() {
    return fName;
  }

  public String getValue() {
    return fValue;
  }

  public boolean equals(Object aObject) {
    if ((null != aObject) && (aObject instanceof AttributeValuePair)) {
      AttributeValuePair other = (AttributeValuePair) aObject;
      if (fName.equals(other.fName)) {
        if (fValue != null) {
          return fValue.equals(other.fValue);
        }
        if (other.fValue == null) {
          return true;
        }
      }
    }
    return false;
  }

  public int hashCode() {
    int h = fName.hashCode();
    if (null != fValue) {
      h = (h << 5) + fValue.hashCode();
    }
    return h;
  }

  public String toString() {
    StringBuf sbuf = StringBuf.Alloc();
    sbuf.append(fName);
    if (fValue != null) {
      sbuf.append("=");
      sbuf.append(quoteValue());
    }
    String rv = sbuf.toString();
    StringBuf.Recycle(sbuf);
    return rv;
  }

  /**
   * Quote the value portion of an attribute,value pair making it
   * suitable for reading in later. We assume "C" style quoting
   * by backslashing quotes embedded in the value.
   */
  protected String quoteValue() {
    if (fValue == null) {
      return "\"\"";
    }
    StringBuf buf = StringBuf.Alloc();
    buf.append('"');
    int n = fValue.length();
    for (int i = 0; i < n; i++) {
      char ch = fValue.charAt(i);
      if (ch == '"') {
        buf.append("\"");
      } else {
        buf.append(ch);
      }
    }
    buf.append('"');
    String rv = buf.toString();
    StringBuf.Recycle(buf);
    return rv;
  }
}
