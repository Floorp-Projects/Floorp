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
 * Class FilterFolder.
 * Implements a filtered projection of anything implimenting Folder.
 *
 * Created: David Williams <djw@netscape.com>,  1 Oct 1997.
 */

package grendel.storage;

import javax.mail.Flags;
import javax.mail.Folder;
import javax.mail.Message;
import javax.mail.Store;
import javax.mail.MessagingException;
import javax.mail.search.SearchTerm;
import javax.mail.event.MessageCountEvent;

class FilterFolder extends FolderBase {

  private String     fName = null;
  private Folder     fTargetFolder = null;
  private SearchTerm fTargetTerm = null;
  private Message[]  fSearchResult = null;
  private Message[]  fOld = null;
  private boolean    fSearchResultsDirty = true;

  FilterFolder(Folder f, String n, SearchTerm term) {
        // Set store to null, as the hstore makes no sense to a FilterFolder.
        // This seems to work ok in current JavaMail API. Maybe not in future.
        // djw Oct/01/1997
        super(null);
        fName = n;
        fTargetFolder = f;
        fTargetTerm = term;
        fSearchResultsDirty = true;
        fOld = null;
  }
  FilterFolder(Folder f, SearchTerm term) {
        this(f, "Filtered " + f.getName(), term);
  }
  void setTarget(Folder f, SearchTerm t) {
        fOld = fSearchResult;
        fSearchResult = null; // conservative, don't bother to check for match
        fTargetFolder = f;
        fTargetTerm = t;
        fSearchResultsDirty = true;
        // this can be aggressive or lazy.
        // Let's do it aggresive, the user will get the hit when they do
        // a search, not when they open the filter folder.
        resync();
  }

  // javax.mail.Folder implimentations that make sense in FilterFolder:
  public String getName() {
        return fName;
  }
  public String getFullName() {
        return getName();
  }
  public int getType() {
        return HOLDS_MESSAGES;
  }
  public boolean exists() {
        return false; // folder never physically exists
  }
  public boolean renameTo(Folder f) {
        fName = f.getName();
        return true;
  }
  private void resync() {
        Message[] old = fOld;
        if (fSearchResultsDirty == true) { // need to make a new fSearchResult.
          if (fTargetFolder != null) {
                if (fTargetTerm != null) { // do a search
                  try {
                        fSearchResult = fTargetFolder.search(fTargetTerm);
                  } catch (MessagingException e) {
                        fSearchResult = null;
                  }
                } else { // return target folders messages
                  try {
                        fSearchResult = fTargetFolder.getMessages();
                  } catch (MessagingException e) {
                        fSearchResult = null;
                  }
                }
          } else { // no target folder yet.
                fSearchResult = null;
          }
          fSearchResultsDirty = false;
        }
        if (old != fSearchResult) {
          // This is pretty hacky.
          if (old != null) {
                notifyMessageRemovedListeners(true, old);
          }
          notifyMessageAddedListeners(fSearchResult);
        }
  }
  // Returns the set of messages that match the current search term,
  // all messages if there is no term, and null if there is no target folder.
  // I think this should propogate the MessagingException up, but FolderBase
  // is not doing this, so I need to talk to Terry.
  public synchronized Message[] getMessages() {
        resync();
        return fSearchResult;
  }

  // to do with sub-folders:
  public Folder getParent() {
        return null; // for now we are always on top
  }
  public Folder[] list(String notused) {
        return null; // no sub-folders
  }
  public char getSeparator() {
        // This should never be called as we don't support sub-folders.
    return '/';
  }

  // Things that should never be called:
  public Folder getFolder(String notused) throws MessagingException {
        throw new MessagingException("FilterFolder.getFolder() not implimented");
  }
  public boolean delete(boolean notused) throws MessagingException {
        throw new MessagingException("FilterFolder.delete() not implimented");
  }
  public Message[] expunge() throws MessagingException {
        throw new MessagingException("FilterFolder.expunge() not implimented");
  }
  public void appendMessages(Message[] notused) throws MessagingException {
        throw new
          MessagingException("FilterFolder.appendMessages() not implimented");
  }
  public void deleteMessage(Message m) throws MessagingException {
        throw new
          MessagingException("FilterFolder.deleteMessage() not implimented");
  }

  // FIXME:
  // Methods that I don't know what to do for.
  public boolean create(int notused) {
        return true; // return success, as this method should be a no-op
  }
  public void open(int mode) {
        // noop
  }
  public void close(boolean expunge) {
        // noop
  }
  public boolean isOpen() {
        // noop
        return false;
  }
  public boolean hasNewMessages() {
        return fSearchResultsDirty; // has the target set changed?
  }

  // FolderExtra stuff..
  public int getUndeletedMessageCount() {
        return 0;
  }
}


