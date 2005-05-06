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
 * Created: Jamie Zawinski <jwz@netscape.com>, 13 Jun 1995.
 * Ported from C on 14 Aug 1997.
 */

/** Test harness for Threader and Sorter.
    @see Threader
    @see Sorter
 */

package grendel.view;

import java.util.StringTokenizer;
import java.util.Vector;
import java.util.Enumeration;
import java.util.NoSuchElementException;
import java.util.Date;
import calypso.util.ByteBuf;
import calypso.util.ArrayEnumeration;
import calypso.util.QSort;
import calypso.util.Comparer;
import calypso.util.NetworkDate;

class TestMessageThread implements IThreadable, ISortable {

  TestMessageThread next;
  TestMessageThread kid;
  String subject;
  String author;
  long date;
  Object id;
  Object[] refs;
  int message_number;

  private String subject2;
  private boolean has_re;

  TestMessageThread() {
    subject = null;  // this means "dummy".
  }
  public TestMessageThread (TestMessageThread next, String subject, Object id,
                            Object references[]) {
    this.next = next;
    this.subject = subject;
    this.id = id;
    this.refs = references;
  }

  public String toString() {
    if (isDummy())
      return "[dummy]";

    String s = "[ " + id + ": " + subject + " (";
    if (refs != null)
      for (int i = 0; i < refs.length; i++)
        s += " " + refs[i];
    if (date > 0)
      s += " \"" + new Date(date) + "\"";
    return s + " ) ]";
  }

  void simplifySubject() {

    int start = 0;
    int L = subject.length();

    boolean done = false;
    while (!done) {
      done = true;

      // skip whitespace.
      while (subject.charAt(start) <= ' ')
        start++;

      if (start < (L-2) &&
          (subject.charAt(start)   == 'r' || subject.charAt(start)   == 'R') &&
          (subject.charAt(start+1) == 'e' || subject.charAt(start+1) == 'E')) {
        if (subject.charAt(start+2) == ':') {
          start += 3;       // Skip over "Re:"
          has_re = true;    // yes, we found it.
          done = false;     // keep going.
          done = false;

        } else if (start < (L-2) &&
                   (subject.charAt(start+2) == '[' ||
                    subject.charAt(start+2) == '(')) {
          int i = start+3;      // skip over "Re[" or "Re("

          // Skip forward over digits after the "[" or "(".
          while (i < L &&
                 subject.charAt(i) >= '0' &&
                 subject.charAt(i) <= '9')
            i++;

          // Now ensure that the following thing is "]:" or "):"
          // Only if it is do we alter `start'.
          if (i < (L-1) &&
              (subject.charAt(i) == ']' ||
               subject.charAt(i) == ')') &&
              subject.charAt(i+1) == ':') {
            start = i+2;        // Skip over "]:"
            has_re = true;      // yes, we found it.
            done = false;       // keep going.
          }
        }
      }

      if (subject2 == "(no subject)")   // #### i18n
        subject2 = "";
    }

    int end = L;
    // Strip trailing whitespace.
    while (end > start && subject.charAt(end-1) < ' ')
      end--;

    if (start == 0 && end == L)
      subject2 = subject;
    else
      subject2 = subject.substring(start, end);
  }

  void flushSubjectCache() {
    subject2 = null;
  }


  // for IThreadable

  public synchronized Enumeration allElements() {
    return new TestMessageThreadEnumeration(this, true);
  }

  public Object messageThreadID() {
    return id;
  }

  public Object[] messageThreadReferences() {
    return refs;
  }

  public String simplifiedSubject() {
    if (subject2 == null) simplifySubject();
    return subject2;
  }

  public boolean subjectIsReply() {
    if (subject2 == null) simplifySubject();
    return has_re;
  }

  // Used by both IThreadable and ISortable
  public void setNext (Object next) {
    this.next = (TestMessageThread) next;
    flushSubjectCache();
  }

  // Used by both IThreadable and ISortable
  public void setChild (Object kid) {
    this.kid = (TestMessageThread) kid;
    flushSubjectCache();
  }

  public IThreadable makeDummy() {
    return (IThreadable) new TestMessageThread();
  }

  public boolean isDummy() {
    return (subject == null);
  }

  // For ISortable

  public synchronized Enumeration children() {
    return new TestMessageThreadEnumeration(this, false);
  }
}

class TestMessageThreadEnumeration implements Enumeration {

  TestMessageThread tail;
  Enumeration kids;
  boolean recursive_p;

  TestMessageThreadEnumeration(TestMessageThread thread, boolean recursive_p) {
    this.recursive_p = recursive_p;
    if (recursive_p)
      tail = thread;
    else
      tail = thread.kid;
  }

  public synchronized Object nextElement() {
    if (kids != null) {
      // if `kids' is non-null, then we've already returned a node,
      // and we should now go to work on its children.
      TestMessageThread result = (TestMessageThread) kids.nextElement();
      if (!kids.hasMoreElements())
        kids = null;
      return result;

    } else if (tail != null) {
      // Return `tail', but first note its children, if any.
      // We will descend into them the next time around.
      TestMessageThread result = tail;
      if (recursive_p && tail.kid != null)
        kids = new TestMessageThreadEnumeration(tail.kid, true);
      tail = tail.next;
      return result;

    } else {
      throw new NoSuchElementException();
    }
  }

  public synchronized boolean hasMoreElements() {
    if (tail != null)
      return true;
    else if (kids != null && kids.hasMoreElements())
      return true;
    else
      return false;
  }
}

class TestThreader {

  private static TestMessageThread first, last;
  private static int count = 0;

  private static final int SORT_NUMBER  = 0;
  private static final int SORT_DATE    = 1;
  private static final int SORT_SUBJECT = 2;
  private static final int SORT_AUTHOR  = 3;

  private static void make_thread(String subject, String id, String refs) {
    TestMessageThread thread = new TestMessageThread();
    thread.subject = subject;
    thread.id = id;
    thread.message_number = ++count;
    if (refs != null) {
      StringTokenizer st = new StringTokenizer(refs);
      Vector v = new Vector(5);
      while (st.hasMoreTokens())
        v.addElement(st.nextToken());
      thread.refs = new Object[v.size()];
      for (int i = 0; i < v.size(); i++)
        thread.refs[i] = (String) v.elementAt(i);
    }
    if (first == null)
      first = thread;
    else
      last.next = thread;
    last = thread;
  }

  private static void make_thread2(String subject, String id, String date,
                                   String refs) {
    TestMessageThread thread = new TestMessageThread();
    thread.subject = subject;
    thread.id = id;
    thread.date = NetworkDate.parseLong(new ByteBuf(date), false);
    thread.message_number = ++count;
    if (refs != null) {
      StringTokenizer st = new StringTokenizer(refs);
      Vector v = new Vector(5);
      while (st.hasMoreTokens())
        v.addElement(st.nextToken());
      thread.refs = new Object[v.size()];
      for (int i = 0; i < v.size(); i++)
        thread.refs[i] = (String) v.elementAt(i);
    }
    if (first == null)
      first = thread;
    else
      last.next = thread;
    last = thread;
  }

  public static void main(String[] args) {

    int sort_type = SORT_NUMBER;

    make_thread("A", "1", null);
    make_thread("B", "2", "1");
    make_thread("C", "3", "1 2");
    make_thread("D", "4", "1");
    make_thread("E", "5", "3 x1 x2 x3");
    make_thread("F", "6", "2");
    make_thread("G", "7", "nonesuch");
    make_thread("H", "8", "nonesuch");

    make_thread("Loop1", "loop1", "loop2 loop3");
    make_thread("Loop2", "loop2", "loop3 loop1");
    make_thread("Loop3", "loop3", "loop1 loop2");

    make_thread("Loop4", "loop4", "loop5");
    make_thread("Loop5", "loop5", "loop4");

    make_thread("Loop6", "loop6", "loop6");

    make_thread("Loop7",  "loop7", "loop8  loop9  loop10 loop8  loop9 loop10");
    make_thread("Loop8",  "loop8", "loop9  loop10 loop7  loop9  loop10 loop7");
    make_thread("Loop8",  "loop9", "loop10 loop7  loop8  loop10 loop7  loop8");
    make_thread("Loop10", "loop10","loop7  loop8  loop9  loop7  loop8  loop9");

    make_thread("Ambig1",  "ambig1",  null);
    make_thread("Ambig2",  "ambig2",  "ambig1");
    make_thread("Ambig3",  "ambig3",  "ambig1 ambig2");
    make_thread("Ambig4",  "ambig4",  "ambig1 ambig2 ambig3");
    make_thread("Ambig5a", "ambig5a", "ambig1 ambig2 ambig3 ambig4");
    make_thread("Ambig5b", "ambig5b", "ambig1 ambig3 ambig2 ambig4");

    make_thread("dup",       "dup",       null);
    make_thread("dup-kid",   "dup-kid",   "dup");
    make_thread("dup-kid",   "dup-kid",   "dup");
    make_thread("dup-kid-2", "dup-kid-2", "dup");
    make_thread("dup-kid-2", "dup-kid-2", "dup");
    make_thread("dup-kid-2", "dup-kid-2", "dup");

    make_thread("same subject 1", "ss1.1", null);
    make_thread("same subject 1", "ss1.2", null);

    make_thread("missingmessage", "missa", null);
    make_thread("missingmessage", "missc", "missa missb");

    make_thread2("liar 1", "<liar.1>", "", "<liar.a> <liar.c>");
    make_thread2("liar 2", "<liar.2>", "", "<liar.a> <liar.b> <liar.c>");


    make_thread2("liar2 1", "<liar2.1>", "", "<liar2.a> <liar2.b> <liar2.c>");
    make_thread2("liar2 2", "<liar2.2>", "", "<liar2.a> <liar2.c>");


    make_thread2("xx",
                 "<331F7D61.2781@netscape.com>",
                 "Thu, 06 Mar 1997 18:28:50 -0800",
                 null);
    make_thread2("lkjhlkjh",
                 "<3321E51F.41C6@netscape.com>",
                 "Sat, 08 Mar 1997 14:15:59 -0800",
                 null);
    make_thread2("test 2",
                 "<3321E5A6.41C6@netscape.com>",
                 "Sat, 08 Mar 1997 14:18:14 -0800",
                 null);
    make_thread2("enc",
                 "<3321E5C0.167E@netscape.com>",
                 "Sat, 08 Mar 1997 14:18:40 -0800",
                 null);
    make_thread2("lkjhlkjh",
                 "<3321E715.15FB@netscape.com>",
                 "Sat, 08 Mar 1997 14:24:21 -0800",
                 null);
    make_thread2("eng",
                 "<3321E7A4.59E2@netscape.com>",
                 "Sat, 08 Mar 1997 14:26:44 -0800",
                 null);
    make_thread2("lkjhl",
                 "<3321E7BB.1CFB@netscape.com>",
                 "Sat, 08 Mar 1997 14:27:07 -0800",
                 null);
    make_thread2("Re: certs and signed messages",
                 "<332230AA.41C6@netscape.com>",
                 "Sat, 08 Mar 1997 19:38:18 -0800",
                 "<33222A5E.ED4@netscape.com>");
    make_thread2("from dogbert",
                 "<3323546E.BEE44C78@netscape.com>",
                 "Sun, 09 Mar 1997 16:23:10 -0800",
                 null);
    make_thread2("lkjhlkjhl",
                 "<33321E2A.1C849A20@netscape.com>",
                 "Thu, 20 Mar 1997 21:35:38 -0800",
                 null);
    make_thread2("le:/u/jwz/mime/smi",
                 "<33323C9D.ADA4BCBA@netscape.com>",
                 "Thu, 20 Mar 1997 23:45:33 -0800",
                 null);
    make_thread2("ile:/u/jwz",
                 "<33323F62.402C573B@netscape.com>",
                 "Thu, 20 Mar 1997 23:57:22 -0800",
                 null);
    make_thread2("ljkljhlkjhl",
                 "<336FBAD0.864BC1F4@netscape.com>",
                 "Tue, 06 May 1997 16:12:16 -0700",
                 null);
    make_thread2("lkjh",
                 "<336FBB46.A0028A6D@netscape.com>",
                 "Tue, 06 May 1997 16:14:14 -0700",
                 null);
    make_thread2("foo",
                 "<337265C1.5C758C77@netscape.com>",
                 "Thu, 08 May 1997 16:46:09 -0700",
                 null);
    make_thread2("Welcome to Netscape",
                 "<337AAB3D.C8BCE069@netscape.com>",
                 "Wed, 14 May 1997 23:20:45 -0700",
                 null);
    make_thread2("Re: Welcome to Netscape",
                 "<337AAE46.903032E4@netscape.com>",
                 "Wed, 14 May 1997 23:33:45 -0700",
                 "<337AAB3D.C8BCE069@netscape.com>");
    make_thread2("[Fwd: enc/signed test 1]",
                 "<338B6EE2.BB26C74C@netscape.com>",
                 "Tue, 27 May 1997 16:31:46 -0700",
                 null);

    Threader t = new Threader();
    last = null;
    first = (TestMessageThread) t.thread (first);
    System.out.print("\n------- threaded:\n\n");
    printThread(first, 0);

//    System.out.print("\n------- pass 2\n\n");
//    t = new Threader();
//    first = (TestMessageThread) t.thread (first);
//    printThread(first, 0);

//    System.out.print("\n------- pass 3\n\n");
//    t = new Threader();
//    first = (TestMessageThread) t.thread (first);
//    printThread(first, 0);

    Comparer comparer;

    if (sort_type == SORT_DATE) {
      comparer = new Comparer() {
        public int compare(Object oa, Object ob) {
          TestMessageThread ta = (TestMessageThread) oa;
          TestMessageThread tb = (TestMessageThread) ob;
          TestMessageThread a = (ta.subject == null ? ta.kid : ta);
          TestMessageThread b = (tb.subject == null ? tb.kid : tb);
          if (a.date == b.date) return 0;
          else if (a.date < b.date) return -1;
          else return 1;
        }
      };

    } else if (sort_type == SORT_SUBJECT) {
      comparer = new Comparer() {
        public int compare(Object oa, Object ob) {
          TestMessageThread ta = (TestMessageThread) oa;
          TestMessageThread tb = (TestMessageThread) ob;
          TestMessageThread a = (ta.subject == null ? ta.kid : ta);
          TestMessageThread b = (tb.subject == null ? tb.kid : tb);
          return a.simplifiedSubject().compareTo(b.simplifiedSubject());
        }
      };

    } else if (sort_type == SORT_AUTHOR) {
      comparer = new Comparer() {
        public int compare(Object oa, Object ob) {
          TestMessageThread ta = (TestMessageThread) oa;
          TestMessageThread tb = (TestMessageThread) ob;
          TestMessageThread a = (ta.subject == null ? ta.kid : ta);
          TestMessageThread b = (tb.subject == null ? tb.kid : tb);
          return a.author.compareTo(b.author);
        }
      };

    } else {     //  (sort_type == SORT_NUMBER)

      comparer = new Comparer() {
        public int compare(Object oa, Object ob) {
          TestMessageThread ta = (TestMessageThread) oa;
          TestMessageThread tb = (TestMessageThread) ob;
          TestMessageThread a = (ta.subject == null ? ta.kid : ta);
          TestMessageThread b = (tb.subject == null ? tb.kid : tb);

          if (a.message_number == b.message_number) return 0;
          else if (a.message_number < b.message_number) return -1;
          else return 1;
        }
      };
    }

    Sorter s = new Sorter(comparer);
    TestMessageThread dummy = new TestMessageThread();
    dummy.kid = first;
    s.sortMessageChildren(dummy);
    first = dummy.kid;
    dummy = null;

    System.out.print("\n------- sorted:\n\n");
    printThread(first, 0);
  }

  private static void printThread(TestMessageThread thread, int depth) {
    for (int i = 0; i < depth; i++) System.out.print("  ");
    System.out.println(thread.toString());
    if (thread.kid != null) printThread(thread.kid, depth+1);
    if (thread.next != null) printThread(thread.next, depth);
  }
}
