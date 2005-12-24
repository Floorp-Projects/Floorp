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
 * Created: Terry Weissman <terry@netscape.com>, 25 Aug 1997.
 */

package grendel.view;

import calypso.util.ArrayEnumeration;
import calypso.util.Assert;
import calypso.util.Comparer;
import calypso.util.PrefetchEnumeration;
import calypso.util.QSort;

import java.util.Enumeration;
import java.util.Date;
import java.util.Vector;

import javax.mail.FetchProfile;
import javax.mail.Folder;
import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.event.MessageChangedEvent;
import javax.mail.event.MessageChangedListener;
import javax.mail.event.MessageCountEvent;
import javax.mail.event.MessageCountListener;

import grendel.storage.MessageExtraFactory;

class FolderViewBase implements FolderView, MessageChangedListener,
                      MessageCountListener
{
  ViewedMessageBase fRoot;
  Folder fFolder;
  Vector observers = new Vector();

  boolean threaded;
  int sortorder[] = {NUMBER};

  public void loadFrom(Folder f) {
    if (fFolder != null) {
      fFolder.removeMessageChangedListener(this);
      fFolder.removeMessageCountListener(this);
    }
    fFolder = f;
    fFolder.addMessageChangedListener(this);
    fFolder.addMessageCountListener(this);

    Message messages[] = null;
    try {
      if (!fFolder.isOpen()) {
        try {
          fFolder.open(Folder.READ_WRITE);
        } catch (MessagingException e) {
          fFolder.open(Folder.READ_ONLY);
        }
      }
      messages = fFolder.getMessages();
      /*FetchProfile fp = new FetchProfile();
      fp.add(FetchProfile.Item.ENVELOPE);
      f.fetch(messages, fp);*/
    } catch (MessagingException e) {
        e.printStackTrace();
       
    }

    if (messages != null) {
      for (int i=0 ; i<messages.length ; i++) {
        ViewedMessageBase v = new ViewedMessageBase(this, messages[i]);
        v.setNext(fRoot);
        fRoot = v;
      }
    }

    reThread();
  }

  protected int generateSortKeys(ViewedMessageBase msg, int key) {
    for ( ; msg != null ; msg = (ViewedMessageBase) msg.getNext()) {
      msg.sortkey = key++;
      Message m = msg.getMessage();
      key = generateSortKeys((ViewedMessageBase) msg.getChild(), key);
    }
    return key;
  }


  protected void flattenList(ViewedMessageBase msg) {
    while (msg != null) {
      ViewedMessageBase next = (ViewedMessageBase) msg.getNext();
      ViewedMessageBase child = (ViewedMessageBase) msg.getChild();
      msg.setNext(fRoot);
      msg.setChild(null);
      if (!msg.isDummy()) {
        fRoot = msg;
      }
      flattenList(child);
      msg = next;
    }
  }


  public void reThread() {

    // ### What a hack.  Tell our observers that everything has gone away;
    // in a while we'll tell them that everything has reappeared.
    for (int i=observers.size() - 1 ; i>=0 ; i--) {
      ((MessageSetViewObserver) observers.elementAt(i)).messagesChanged
        (null, new ViewedMessageEnumeration(fRoot), null);
    }



    if (threaded) {
      Threader t = new Threader();
      fRoot = (ViewedMessageBase) t.thread(fRoot);
    } else {
      // Flatten out the list, and remove any dummies.
      ViewedMessageBase base = fRoot;
      fRoot = null;
      flattenList(base);
    }

    if (fRoot == null) return;  // empty folder

    int nummsgs = fRoot.getMessageCount();

    int numkeys = sortorder.length;
    int n = 0;
    int count = generateSortKeys(fRoot, 0);
    Assert.Assertion(count == nummsgs);

    Comparer comp = new NullComparer();
    for (int i=sortorder.length - 1 ; i>=0 ; i--) {
      switch (sortorder[i]) {
        case NUMBER:
          comp = new NumberComparer(nummsgs, comp);
          break;
        case DATE:
          comp = new DateComparer(nummsgs, comp);
          break;
        case SUBJECT:
          comp = new SubjectComparer(nummsgs, comp);
          break;
        case AUTHOR:
          comp = new AuthorComparer(nummsgs, comp);
          break;
        case READ:
          comp = new ReadComparer(nummsgs, comp);
          break;
        case FLAGGED:
          comp = new FlaggedComparer(nummsgs, comp);
          break;
        case SIZE:
          comp = new SizeComparer(nummsgs, comp);
          break;
        case DELETED:
          comp = new DeletedComparer(nummsgs, comp);
          break;
        default:
          throw new IllegalArgumentException("Bad sort key " + sortorder[i]);
      }
    }

    Sorter s = new Sorter(comp);
    DummyThreadable dummy = new DummyThreadable(null, null);
    dummy.fChildren = fRoot;
    s.sortMessageChildren(dummy);
    fRoot = (ViewedMessageBase) dummy.fChildren;
    dummy = null;

    fRoot.setParentPointers(null);


    // ### What a hack, part two.  Tell our observers that everything
    // reappeared.
    for (int i=observers.size() - 1 ; i>=0 ; i--) {
      ((MessageSetViewObserver) observers.elementAt(i)).messagesChanged
        (new ViewedMessageEnumeration(fRoot), null, null);
    }
  }


  public void setSortOrder(int value[]) {
    sortorder = new int[value.length];
    int j=0;

    // Strip out duplicates in the sortorder array.
    NEXTI:
    for (int i=0 ; i<value.length ; i++) {
      int v = value[i];
      for (int k=0 ; k<i ; k++) {
        if (value[k] == v) continue NEXTI;
      }
      sortorder[j++] = v;
      if (v == NUMBER) break;   // Message number is always unique, so don't
                                // bother sorting beyond that.
    }

    if (j < value.length) {
      int t[] = new int[j];
      for (int k=0 ; k<j ; k++) {
        t[k] = sortorder[k];
      }
      sortorder = t;
    }

System.out.println("New sort order is " + sortorder);

  }

  public void prependSortOrder(int k) {
    int value[] = new int[sortorder.length + 1];
    value[0] = k;
    System.arraycopy(sortorder, 0, value, 1, sortorder.length);
    setSortOrder(value);
  }


  public int[] getSortOrder() {
    return sortorder;
  }

  public int getNumMessages() {
    Assert.NotYetImplemented("getNumMessages");
    return 0;
  }

  public ViewedMessage getMessageRoot() {
    return fRoot;
  }

  public void setIsThreaded(boolean b) {
    threaded = b;
  }
  public boolean isThreaded() {
    return threaded;
  }
  public Folder getFolder() {
    return fFolder;
  }

  public void dumpMessages() {
    fRoot.dump();
  }

  public void messageChanged(MessageChangedEvent event) {
    ViewedMessage viewed[] = new ViewedMessage[1];
    viewed[0] = findViewedMessage(event.getMessage());
    for (int i=observers.size() - 1 ; i>=0 ; i--) {
      ((MessageSetViewObserver) observers.elementAt(i)).messagesChanged
        (null, null, new ArrayEnumeration(viewed));
    }
  }

  public void messagesAdded(MessageCountEvent event) {
    Message[] messages = event.getMessages();
    ViewedMessage viewed[] = new ViewedMessage[messages.length];
    for (int i=0 ; i<messages.length ; i++) {
      viewed[i] = findViewedMessage(messages[i]);
    }
    for (int i=observers.size() - 1 ; i>=0 ; i--) {
      ((MessageSetViewObserver) observers.elementAt(i)).messagesChanged
        (new ArrayEnumeration(viewed), null, null);
    }
  }

  public void messagesRemoved(MessageCountEvent event) {
    Message[] messages = event.getMessages();
    ViewedMessage viewed[] = new ViewedMessage[messages.length];
    for (int i=0 ; i<messages.length ; i++) {
      viewed[i] = findViewedMessage(messages[i]);
    }
    for (int i=observers.size() - 1 ; i>=0 ; i--) {
      ((MessageSetViewObserver) observers.elementAt(i)).messagesChanged
        (null, new ArrayEnumeration(viewed), null);
    }
  }

  protected ViewedMessage findViewedMessage(Message m, ViewedMessage v) {
    for ( ; v != null ; v = v.getNext()) {
      if (v.getMessage() == m) return v;
      ViewedMessage result = findViewedMessage(m, v.getChild());
      if (result != null) return result;
    }
    return null;
  }

  protected ViewedMessage findViewedMessage(Message m) {
    ViewedMessage result = findViewedMessage(m, fRoot);
    if (result == null) {
      result = new ViewedMessageBase(this, m);
    }
    return result;
  }

  public void addObserver(MessageSetViewObserver obs) {
    observers.addElement(obs);
  }

  public void removeObserver(MessageSetViewObserver obs) {
    observers.removeElement(obs);
  }


  final class NullComparer implements Comparer {
    public int compare(Object a, Object b) {
      return 0;
    }
  }

  final class NumberComparer implements Comparer {
    Comparer next;
    int cache[];
    NumberComparer(int nummsgs, Comparer next) {
      cache = new int[nummsgs];
      this.next = next;
    }
    int getMessageNumber(Object a) {
      ViewedMessageBase v = (ViewedMessageBase) a;
      int key = v.sortkey;
      cache[key] = v.getMessage().getMessageNumber();
      return cache[key];
    }
    public int compare(Object a, Object b) {
      int k1 = cache[((ViewedMessageBase)a).sortkey];
      int k2 = cache[((ViewedMessageBase)b).sortkey];
      if (k1 == 0) k1 = getMessageNumber(a);
      if (k2 == 0) k2 = getMessageNumber(b);
      if (k1 != k2) return k2 - k1;
      return next.compare(a, b);
    }
  }

  final class DateComparer implements Comparer {
    Comparer next;
    long cache[];
    DateComparer(int nummsgs, Comparer next) {
      cache = new long[nummsgs];
      this.next = next;
    }
    long getTime(Object a) {
      ViewedMessageBase v = (ViewedMessageBase) a;
      int key = v.sortkey;
      try {
        cache[key] = v.getMessage().getSentDate().getTime();
      } catch (MessagingException e) {
      } catch (NullPointerException e) {
      }
      if (cache[key] == 0) {
        cache[key] = 1;
      }
      return cache[key];
    }
    public int compare(Object a, Object b) {
      long k1 = cache[((ViewedMessageBase)a).sortkey];
      long k2 = cache[((ViewedMessageBase)b).sortkey];
      if (k1 == 0) k1 = getTime(a);
      if (k2 == 0) k2 = getTime(b);
      if (k1 != k2) {
        // I'd like to just cast the difference to an int and return that, but
        // I'm afraid that sign conversion will screw up.  So we do it by hand.
        if (k1 < k2) return 1;
        else return -1;
      }
      return next.compare(a, b);
    }
  }

  final class SubjectComparer implements Comparer {
    Comparer next;
    String cache[];
    SubjectComparer(int nummsgs, Comparer next) {
      cache = new String[nummsgs];
      this.next = next;
    }
    String getSubject(Object a) {
      ViewedMessageBase v = (ViewedMessageBase) a;
      int key = v.sortkey;
      try {
        cache[key] =
          MessageExtraFactory.Get(v.getMessage()).simplifiedSubject();
      } catch (MessagingException e) {
      }
      if (cache[key] == null) {
        cache[key] = "";
      }
      return cache[key];
    }
    public int compare(Object a, Object b) {
      String k1 = cache[((ViewedMessageBase)a).sortkey];
      String k2 = cache[((ViewedMessageBase)b).sortkey];
      if (k1 == null) k1 = getSubject(a);
      if (k2 == null) k2 = getSubject(b);
      int result = k1.compareTo(k2);
      if (result != 0) return result;
      return next.compare(a, b);
    }
  }

  final class AuthorComparer implements Comparer {
    Comparer next;
    String cache[];
    AuthorComparer(int nummsgs, Comparer next) {
      cache = new String[nummsgs];
      this.next = next;
    }
    String getAuthor(Object a) {
      ViewedMessageBase v = (ViewedMessageBase) a;
      int key = v.sortkey;
      try {
        cache[key] =
          MessageExtraFactory.Get(v.getMessage()).getAuthor();
      } catch (MessagingException e) {
      }
      if (cache[key] == null) {
        cache[key] = "";
      }
      return cache[key];
    }
    public int compare(Object a, Object b) {
      String k1 = cache[((ViewedMessageBase)a).sortkey];
      String k2 = cache[((ViewedMessageBase)b).sortkey];
      if (k1 == null) k1 = getAuthor(a);
      if (k2 == null) k2 = getAuthor(b);
      int result = k1.compareTo(k2);
      if (result != 0) return result;
      return next.compare(a, b);
    }
  }

  final class ReadComparer implements Comparer {
    Comparer next;
    int cache[];
    ReadComparer(int nummsgs, Comparer next) {
      cache = new int[nummsgs];
      this.next = next;
    }
    int getRead(Object a) {
      ViewedMessageBase v = (ViewedMessageBase) a;
      int key = v.sortkey;
      try {
        cache[key] =
          MessageExtraFactory.Get(v.getMessage()).isRead() ? 1 : 2;
      } catch (MessagingException e) {
        cache[key] = 2;         // False by default.
      }
      return cache[key];
    }
    public int compare(Object a, Object b) {
      int k1 = cache[((ViewedMessageBase)a).sortkey];
      int k2 = cache[((ViewedMessageBase)b).sortkey];
      if (k1 == 0) k1 = getRead(a);
      if (k2 == 0) k2 = getRead(b);
      if (k1 != k2) return k2 - k1;
      return next.compare(a, b);
    }
  }

  final class FlaggedComparer implements Comparer {
    Comparer next;
    int cache[];
    FlaggedComparer(int nummsgs, Comparer next) {
      cache = new int[nummsgs];
      this.next = next;
    }
    int getFlagged(Object a) {
      ViewedMessageBase v = (ViewedMessageBase) a;
      int key = v.sortkey;
      try {
        cache[key] =
          MessageExtraFactory.Get(v.getMessage()).isFlagged() ? 1 : 2;
      } catch (MessagingException e) {
        cache[key] = 2;         // False by default.
      }
      return cache[key];
    }
    public int compare(Object a, Object b) {
      int k1 = cache[((ViewedMessageBase)a).sortkey];
      int k2 = cache[((ViewedMessageBase)b).sortkey];
      if (k1 == 0) k1 = getFlagged(a);
      if (k2 == 0) k2 = getFlagged(b);
      if (k1 != k2) return k2 - k1;
      return next.compare(a, b);
    }
  }


  final class SizeComparer implements Comparer {
    Comparer next;
    int cache[];
    SizeComparer(int nummsgs, Comparer next) {
      cache = new int[nummsgs];
      this.next = next;
    }
    int getSize(Object a) {
      ViewedMessageBase v = (ViewedMessageBase) a;
      int key = v.sortkey;
      try {
        cache[key] = v.getMessage().getSize();
      } catch (MessagingException e) {
      }
      if (cache[key] == 0) cache[key] = -1;
      return cache[key];
    }
    public int compare(Object a, Object b) {
      int k1 = cache[((ViewedMessageBase)a).sortkey];
      int k2 = cache[((ViewedMessageBase)b).sortkey];
      if (k1 == 0) k1 = getSize(a);
      if (k2 == 0) k2 = getSize(b);
      if (k1 != k2) return k2 - k1;
      return next.compare(a, b);
    }
  }


  final class DeletedComparer implements Comparer {
    Comparer next;
    int cache[];
    DeletedComparer(int nummsgs, Comparer next) {
      cache = new int[nummsgs];
      this.next = next;
    }
    int getDeleted(Object a) {
      ViewedMessageBase v = (ViewedMessageBase) a;
      int key = v.sortkey;
      try {
        cache[key] =
          MessageExtraFactory.Get(v.getMessage()).isDeleted() ? 1 : 2;
      } catch (MessagingException e) {
        cache[key] = 2;         // False by default.
      }
      return cache[key];
    }
    public int compare(Object a, Object b) {
      int k1 = cache[((ViewedMessageBase)a).sortkey];
      int k2 = cache[((ViewedMessageBase)b).sortkey];
      if (k1 == 0) k1 = getDeleted(a);
      if (k2 == 0) k2 = getDeleted(b);
      if (k1 != k2) return k2 - k1;
      return next.compare(a, b);
    }
  }


  final class ViewedMessageEnumeration extends PrefetchEnumeration {
    ViewedMessage cur;
    ViewedMessageEnumeration(ViewedMessage obj) {
      cur = obj;
    }
    protected Object fetch() {
      Object result = cur;
      if (cur != null) {
        ViewedMessage tmp = cur.getChild();
        if (tmp != null) cur = tmp;
        else {
          tmp = cur.getNext();
          if (tmp != null) cur = tmp;
          else {
            tmp = cur.getParent();
            while (tmp != null && tmp.getNext() == null) {
              tmp = tmp.getParent();
            }
            if (tmp != null) cur = tmp.getNext();
            else cur = null;
          }
        }
      }
      return result;
    }
  }

}
