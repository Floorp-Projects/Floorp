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

import calypso.util.Assert;

import grendel.storage.MessageExtra;
import grendel.storage.MessageExtraFactory;

import javax.mail.Message;
import javax.mail.MessagingException;

import java.util.Enumeration;
import java.util.NoSuchElementException;

class ViewedMessageBase implements ViewedMessage, IThreadable, ISortable {
  MessageSetView fView;
  Message fMessage;

  ViewedMessage fParent;
  ViewedMessage fChildren;
  ViewedMessage fSibling;

  public int sortkey;           // Used to index into cached info
                                // when sorting.

  public ViewedMessageBase(MessageSetView v, Message m) {
    fView = v;
    fMessage = m;
  }

  public final MessageSetView getView() {
    return fView;
  }

  public final Message getMessage() {
    return fMessage;
  }

    // ******************* IThreadable methods ****************

  class AllEnumerator implements Enumeration {

    ViewedMessageBase tail;
    Enumeration kids;

    AllEnumerator(ViewedMessageBase thread) {
      tail = thread;
    }

    public synchronized Object nextElement() {
      if (kids != null) {
        // if `kids' is non-null, then we've already returned a node,
        // and we should now go to work on its children.
        ViewedMessageBase result = (ViewedMessageBase) kids.nextElement();
        if (!kids.hasMoreElements()) {
          kids = null;
        }
        return result;

      } else if (tail != null) {
        // Return `tail', but first note its children, if any.
        // We will descend into them the next time around.
        ViewedMessageBase result = tail;
        if (tail.fChildren != null) {
          kids = new AllEnumerator((ViewedMessageBase) tail.fChildren);
        }
        tail = (ViewedMessageBase) tail.fSibling;
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

  public Enumeration allElements() {
    return new AllEnumerator(this);
  }

  public Object messageThreadID() {
    try {
      return MessageExtraFactory.Get(fMessage).getMessageID();
    } catch (MessagingException e) {
      return null;
    }
  }

  public Object[] messageThreadReferences() {
    try {
      return MessageExtraFactory.Get(fMessage).messageThreadReferences();
    } catch (MessagingException e) {
      return null;
    }
  }

  public String simplifiedSubject() {
    try {
      return MessageExtraFactory.Get(fMessage).simplifiedSubject();
    } catch (MessagingException e) {
      return "";
    }
  }

  public boolean subjectIsReply() {
    try {
      return MessageExtraFactory.Get(fMessage).subjectIsReply();
    } catch (MessagingException e) {
      return false;
    }
  }

  public Enumeration children() {
    return new Enumeration() {
      ViewedMessageBase cur = (ViewedMessageBase) fChildren;
      public boolean hasMoreElements() {
        return (cur != null);
      }
      public Object nextElement() {
        Object result = cur;
        cur = (ViewedMessageBase) cur.fSibling;
        return result;
      }
    };
  }

  public void setNext(Object next) {
    fSibling = (ViewedMessage) next;
  }

  public void setChild(Object kid) {
    fChildren = (ViewedMessage) kid;
  }

  public void setParentPointers(ViewedMessage parent) {
    fParent = parent;
    ViewedMessageBase v = (ViewedMessageBase) fChildren;
    if (v != null) v.setParentPointers(this);
    v = (ViewedMessageBase) fSibling;
    if (v != null) v.setParentPointers(parent);
  }

  public IThreadable makeDummy() {
    return new DummyThreadable(fView, fMessage);
  }

  public boolean isDummy() {
    return false;
  }

  public void dump() {
    dump(0);
  }

  void dump(int indent) {
    for (int i=0 ; i<indent ; i++) {
      System.out.print(" ");
    }
    System.out.println(this);
    if (fChildren != null) {
      Assert.Assertion(fChildren.getParent() == this);
      ((ViewedMessageBase) fChildren).dump(indent + 1);
    }
    if (fSibling != null) {
      Assert.Assertion(fSibling.getParent() == getParent());
      ((ViewedMessageBase) fSibling).dump(indent);
    }
  }

  public String toString() {
    Message m = fMessage;
    if (m == null)
      return "null";
    MessageExtra mextra = MessageExtraFactory.Get(m);
    try {
      return "Sub: " + (mextra.subjectIsReply() ? "Re: " : "") +
        mextra.simplifiedSubject() + "    ID: " + mextra.getMessageID();
    } catch (MessagingException e) {
      return "???";
    }
  }

  public ViewedMessage getParent() {
    return fParent;
  }

  public ViewedMessage getChild() {
    return fChildren;
  }

  public ViewedMessage getNext() {
    return fSibling;
  }

  int getMessageCount() {
    int result = 0;
    for (ViewedMessageBase m = this ;
         m != null ;
         m = (ViewedMessageBase) m.fSibling) {
      result++;
      if (m.fChildren != null) {
        result += ((ViewedMessageBase) m.fChildren).getMessageCount();
      }
    }
    return result;
  }
}
