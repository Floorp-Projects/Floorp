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
 * Created: Jamie Zawinski <jwz@netscape.com>,  1 Dec 1997.
 */

package grendel.storage;

import calypso.util.Assert;

import java.io.IOException;

import java.util.Vector;
import java.util.Enumeration;

import javax.mail.FetchProfile;
import javax.mail.Flags;
import javax.mail.Folder;
import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.Store;
import javax.mail.event.MessageChangedEvent;
import javax.mail.event.MessageCountEvent;


/** This class implements a Folder representing the root-level of a news
    server.  That is, this is the folder that holds all of the user's
    subscribed newsgroups.  This folder has children and not messages;
    its children hold messages and (probably) not folders.
 */
class NewsFolderRoot extends Folder {

  NewsStore store = null;

  NewsFolderRoot(Store s) {
    super(s);
    store = (NewsStore) s;
  }

  public char getSeparator() {
    // #### If we're in "all groups" mode, this will be '.'.
    // Otherwise, there is no hierarchy, and therefore no separator.
    return '\000';
  }

  public int getType() {
    return HOLDS_FOLDERS;
  }

  public String getName() {
    return "";
  }

  public String getFullName() {
    return "";
  }

  public Folder getParent() {
    return null;
  }

  public Folder[] list(String pattern) {
    NewsRC newsrc = store.getNewsRC();
    Assert.Assertion(newsrc != null);
    Vector v = new Vector();
    for (Enumeration e = newsrc.elements(); e.hasMoreElements(); ) {
      NewsRCLine n = (NewsRCLine) e.nextElement();
      boolean match;

      if (!n.subscribed()) {
        match = false;
      } else if (pattern.equals("%") || pattern.equals("*")) {
        match = true;
      } else {
        // #### I'm not really expected to write a regexp matcher, am I?
        Assert.NotYetImplemented("NewsFolderRoot.list(String pattern)");
        match = false;
      }

      if (match) {
        NewsFolder f = new NewsFolder(store, this, n.name());
        v.addElement(f);
      }
    }

    Folder ff[] = new Folder[v.size()];
    Enumeration e = v.elements();
    for (int i = 0; e.hasMoreElements(); i++)
      ff[i] = (Folder) e.nextElement();
    return ff;
  }


  public Folder getFolder(String subfolder) {
    NewsRC newsrc = store.getNewsRC();
    for (Enumeration e = newsrc.elements(); e.hasMoreElements(); ) {
      NewsRCLine n = (NewsRCLine) e.nextElement();
      if (subfolder.equals(n.name()))
        return new NewsFolder(store, this, n.name());
    }
    return null;
  }

  public boolean create(int type) {
    return false;
  }

  public boolean exists() {
    return true;
  }

  public boolean hasNewMessages() {
    // #### descend into sub-folders?
    return false;
  }

  /** Returns the total number of messages in the folder, or -1 if unknown.
      This includes deleted and unread messages.
    */
  public int getMessageCount() {
    // #### descend into sub-folders?
    return -1;
  }

  /** Returns the number of non-deleted messages in the folder, or -1 if
      unknown.  This includes unread messages.
    */
  public int getUndeletedMessageCount() {
    return getMessageCount();
  }

  /** Returns the number of unread messages in the folder, or -1 if unknown.
      This does not include unread messages that are also deleted.
    */
  public int getUnreadMessageCount() {
    // #### descend into sub-folders?
    return -1;
  }


  /** Returns the number of bytes consumed by deleted but not expunged
      messages in the folder, or -1 if unknown.
    */
  long deletedMessageBytes() {
    return -1;
  }


  public void appendMessages(Message msgs[]) throws MessagingException {
    throw new
      MessagingException("can't append messages to the root news folder.");
  }


  public void fetch(Message msgs[], FetchProfile fp) {
  }

  public Message[] expunge() throws MessagingException {
    return null;
  }

  public boolean delete(boolean recurse) {
    // #### signal error?
    return false;
  }

  public boolean renameTo(Folder f) {
    // #### signal error?
    return false;
  }

  public void open(int mode) {
    // Not called on folders that don't hold messages.
  }

  public void close(boolean doExpunge) throws MessagingException {
  }

  public boolean isOpen() {
    return false;
  }

  public Flags getPermanentFlags() {
    // No messages, therefore, no flags.
    return new Flags();
  }

  public Message getMessage(int msgnum) {
    return null;
  }

}
