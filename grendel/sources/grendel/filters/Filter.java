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
 * The Original Code is the GFilt filter package, as integrated into 
 * the Grendel mail/news reader.
 * 
 * The Initial Developer of the Original Code is Ian Clarke.  
 * Portions created by Ian Clarke are
 * Copyright (C) 2000 Ian Clarke.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 */

package grendel.filters;  // formerly GFilt
import java.util.*;

public class Filter {
  /**
   * Set to true to output debugging information
   **/
  public boolean debug = false;
  Pattern patterns = null;

  /**
   * Get the number of patterns in the filter
   * @return The number of patterns in the filter
   **/
  public int getLength() {
    Pattern cur = patterns;
    int r = 0;
    while (cur != null) {
      cur = cur.next;
      r++;
    }
    return r;
  }

  public int getTLength() {
    Pattern cur = patterns;
    int r = 0;
    while (cur != null)
      {
        r += cur.str.length() + 1;
        cur = cur.next;
      }
    return r;
  }

  public void setString(String str, int p) {
    Pattern pat = patterns;
    for (int x = 0; x < p; x++) {
      pat = pat.next;
    }
    pat.str = str;
  }
  public void setAnd(boolean and, int p) {
    Pattern pat = patterns;
    for (int x = 0; x < p; x++) {
      pat = pat.next;
    }
    pat.and = and;
  }

  public void del(int p) {
    if (p == 0) {
      patterns = patterns.next;
    } else {
      Pattern cur = patterns;
      for (int x = 0; x < (p - 1); x++) {
        cur = cur.next;
      }
      cur.next = cur.next.next;
    }
  }


  public String getString(int p) {
    Pattern pat = patterns;
    for (int x = 0; x < p; x++) {
      pat = pat.next;
    }
    
    return pat.str;
    
  }

  public boolean getAnd(int p) {
    Pattern pat = patterns;
    for (int x = 0; x < p; x++) {
      pat = pat.next;
    }
    return pat.and;
  }

  public void insert(String s, boolean and, int p) {
    Pattern n = new Pattern(s, and);
    Pattern cur = patterns;
    if (p == 0) {
      n.next = patterns;
      patterns = n;
    } else {
      for (int x = 0; x < p - 1; x++) {
        cur = cur.next;
      }
      n.next = cur.next;
      cur.next = n;
    }
  }

  public void push(String s, boolean and) {

    if (patterns == null) {
      patterns = new Pattern(s, and);
    } else {
      Pattern p = patterns;
      
      while (p.next != null) {
        p = p.next;
      }
      p.next = new Pattern(s, and);
    }
  }

  public boolean match(String str) {
    return match(str, 0, patterns);
  }

  public Filter duplicate()
  {
    Filter n = new Filter();
    Pattern cur = patterns;
    while(cur != null)
      {
        n.push(cur.str, cur.and);
        cur = cur.next;
      }
    return n;
  }
    
  protected boolean match(String str, int pos, Pattern pat) 
  {
    if (pat == null)
      {
        System.err.println("Attempted to match with empty pattern list");
        return false;
      }
    int p = str.indexOf(pat.str, pos);
    if (debug)
      System.err.print("Matching "+pat.str + " from "+pos + " - ");
    if (p == -1) {
      if (debug)
        System.err.print("Not found - ");
      if (pat.next == null) {
        if (debug)
          System.err.println("Last pattern - failing");
        return false;
      }
      if (pat.and) {
        if (debug)
          System.err.println("And relationship - failing");
        return false;
      } else {
        if (debug)
          System.err.println("Or relationship - recursing");
        return match(str, pos, pat.next);
      }
    } else {
      if (debug)
        System.err.print("Found at "+p + " - ");
      Pattern tPat = pat;
      while (!(tPat.and)) {
        if (tPat.next == null) {
          if (debug)
            System.err.println("Last one - success");
          return true;
        }
        tPat = tPat.next;
      }
      if (tPat.next == null) {
        if (debug)
          System.err.println("All others are 'or' - success");
        return true;
      }
      if (debug)
        System.err.println("Skipping to end of or group");
      if (match(str, p + pat.str.length(), tPat.next))
        return true;
      else {
        if (debug)
          System.err.println("Backtracking..");
        return (match(str, pos, pat.next));
      }
    }
  }

  public String toString() {
    StringBuffer s = new StringBuffer();
    Pattern cur = patterns;
    while (cur != null) {
      String andV;
      if (cur.and)
        andV = " and then ";
      else
        andV = "or";
      if (cur.next != null)
        s.append("\""+cur.str + "\" "+andV + " ");
      else
        s.append("\""+cur.str + "\"");
      cur = cur.next;
    }
    return s.toString();
  }
}



class Pattern {
  public boolean and;
  public Pattern next = null;
  public String str;
  public Pattern(String s, boolean a) {
    and = a;
    str = s;
  }
}



