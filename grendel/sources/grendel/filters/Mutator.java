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

/**
 * Allows a filter to be mutated in a variety of ways, using a
 * string as a source for new material
 **/
public class Mutator
{
  String source;
  Random r = new Random();
  
  public static void main(String[] args)
  {
    Mutator s = new Mutator("abcdefghijklmnopqrstuvwxyz");
    for (int x=0; x<10; x++)
      System.out.println("\"vwxyz\" -> \""+s.modify("vwxyz")+"\"");
  }
  
  public Mutator(String s)
  {
    source = s;
  }
  
  public void mutate(Filter f)
  {
    int p = Math.abs(r.nextInt()) % 4;
    int ps;
    ps = (Math.abs(r.nextInt()) % f.getLength());
    if ((p==0) && (f.getLength() > 1))
      {  // Delete a pattern
        f.del(ps);
      }
    else if (p==1)
      { // Modify a string in a pattern
        String s = f.getString(ps);
        f.setString(modify(s), ps);
      }
    else if (p==2)
      { // Invert the 'and' in a pattern
        f.setAnd(!(f.getAnd(ps)), ps);
      }
    else if (p==3)
      { // Add a new pattern
        addPattern(f);
      }
  }
  
  public void addPattern(Filter f)
  {
    
    int ps;
    if (f.getLength() > 0)
      ps = (Math.abs(r.nextInt()) % f.getLength());
    else
      ps = 0;
    int len = Math.abs(r.nextInt()) % 10;
    int pos = Math.abs(r.nextInt()) % (source.length()-len);
    f.insert(source.substring(pos, pos+len),
             ((r.nextInt() % 2) == 0), ps);
  }
  
  public Filter breed(Filter a, Filter b)
  {
    Filter shorter, longer, ret = new Filter();
    if (a.getLength() < b.getLength())
      {
        shorter = a;
        longer = b;
      }
    else
      {
        shorter = b;
        longer = a;
      }
    int p = (Math.abs(r.nextInt()) % shorter.getLength());
    for(int x=0; x<p; x++)
      {
        ret.push(shorter.getString(x), shorter.getAnd(x));
      }
    for(int x=p; x<longer.getLength(); x++)
      {
        ret.push(longer.getString(x), longer.getAnd(x));
      }
    return ret;
  }
  
  protected String modify(String s)
  {
    int len = s.length();
    len += (Math.abs(r.nextInt()) % 6) - 3;
    if (len < 1)
      len = 1;
    int pos = source.indexOf(s);
    if (pos == -1)
      {
        pos = (Math.abs(r.nextInt())% (source.length()-len));
      }
    else
      {
        pos += (Math.abs(r.nextInt()) % 6) - 3;
      }
    if (pos < 1)
      pos = 1;

    if ((source.length() - pos) < len)
      {
        len = source.length() - pos;
      }

    return source.substring(pos, pos+len);
  }
}
