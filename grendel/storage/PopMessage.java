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
 * Created: Terry Weissman <terry@netscape.com>, 24 Nov 1997.
 */

package grendel.storage;

import calypso.util.Assert;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import java.util.Date;

import javax.activation.DataHandler;

import javax.mail.Flags;
import javax.mail.IllegalWriteException;
import javax.mail.MessagingException;
import javax.mail.internet.InternetHeaders;

class PopMessage extends MessageReadOnly implements MessageExtra {
  InternetHeaders headers;
  String uidl;
  boolean deletedValid;
  boolean deleted;
  boolean recentValid;
  boolean recent = true;
  MessageExtraWrapper extra = null;


  PopMessage(PopFolder f, int num) {
    super(f, num);
  }

  protected InternetHeaders getHeadersObj()
    throws MessagingException
  {
    if (headers == null) {
      synchronized(this) {
        if (headers == null) {
          PopStore store = (PopStore) folder.getStore();
          InputStream stream = store.sendTopOrRetr(msgnum, 0);
          headers = new InternetHeaders(stream);
          try {
            while (stream.read() >= 0) {
              // Spin until we've sucked out all data, so that we know
              // the socket is back into a known state.
            }
          } catch (IOException e) {
          }
        }
      }
    }
    return headers;
  }

  protected MessageExtra getExtra() {
    if (extra == null) {
      synchronized(this) {
        if (extra == null) {
          extra = new MessageExtraWrapper(this);
        }
      }
    }
    return extra;
  }


  public Date getReceivedDate() throws MessagingException {
    return getSentDate();       // What else can we do???
  }

  public Flags getFlags() throws MessagingException {
    Flags result = new Flags();
    if (isRecent()) result.add(new Flags(Flags.Flag.RECENT));
    if (isDeleted()) result.add(new Flags(Flags.Flag.DELETED));
    return result;
  }

  public boolean isSet(Flags.Flag flag) throws MessagingException {
    if (flag == Flags.Flag.RECENT) {
      return (isRecent());
    } else if (flag == Flags.Flag.DELETED) {
      return (isDeleted());
    } else {
      return false;
    }
  }

  public void setFlags(Flags flags, boolean flag) throws MessagingException {
    loadFlags();

    Flags.Flag list[] = flags.getSystemFlags();

    for (int i = 0; i < list.length; i++) {
      if (list[i] == Flags.Flag.RECENT) {
        recent = flag;
        recentValid = true;
      } else if (list[i] == Flags.Flag.DELETED) {
        deleted = flag;
        deletedValid = true;
      } else {
        throw new IllegalWriteException("Can't change flag " + list[i] +
                                        " on message " + this);
      }
    }

    if (flags.getUserFlags().length > 0) {
        throw new IllegalWriteException("Can't change user flags on message " +
                                        this);
    }
  }

  public void writeTo(OutputStream outputStream) throws MessagingException {
    Assert.NotYetImplemented("PopMessage.writeTo");
  }

  public void saveChanges() throws MessagingException {
  }

  public int getSize() throws MessagingException {
    return -1;
  }

  public int getLineCount() throws MessagingException {
    return -1;
  }

  public InputStream getInputStreamWithHeaders() throws MessagingException {
    PopStore store = (PopStore) folder.getStore();
    return store.sendRetr(msgnum);
  }



  public DataHandler getDataHandler() {
    return null;
  }

  public Object getContent() {
    return null;
  }


  public String getAuthor() throws MessagingException {
    return getExtra().getAuthor();
  }

  /** The name of the recipient of this message (not this email address). */
  public String getRecipient() throws MessagingException {
    return getExtra().getRecipient();
  }

  /** The subject, minus any "Re:" part. */
  public String simplifiedSubject() throws MessagingException {
    return getExtra().simplifiedSubject();
  }

  /** Whether the subject has a "Re:" part." */
  public boolean subjectIsReply() throws MessagingException {
    return getExtra().subjectIsReply();
  }

  /** A short rendition of the sent date. */
  public String simplifiedDate() throws MessagingException {
    return getExtra().simplifiedDate();
  }

  /** A unique object representing the message-id for this message. */
  public Object getMessageID() throws MessagingException {
    return getExtra().getMessageID();
  }

  /** A list of the above messageid objects that this message has references
   to. */
  public Object[] messageThreadReferences() throws MessagingException {
    return getExtra().messageThreadReferences();
  }

  protected void loadFlags() {
    // ### Write me!!!
  }

  public boolean isRead() {
    return false;
  }
  public void setIsRead(boolean value) {
  }
  public boolean isReplied() {
    return false;
  }
  public void setReplied(boolean value) {
  }
  public boolean isForwarded() {
    return false;
  }
  public void setForwarded(boolean value) {
  }
  public boolean isFlagged() {
    return false;
  }
  public void setFlagged(boolean value) {
  }
  public boolean isDeleted() throws MessagingException {
    if (!deletedValid) {
      String state = getStateValue();
      deleted = state != null && state.equals(PopFolder.DELETED);
      deletedValid = true;
    }
    return deleted;
  }
  public void setDeleted(boolean value) {
    deletedValid = true;
    deleted = value;
  }

  public boolean isRecent() throws MessagingException {
    if (!recentValid) {
      recent = (getStateValue() == null);
      recentValid = true;
    }
    return recent;
  }
  public void setRecent(boolean value) {
    recentValid = true;
    recent = value;
  }

  protected String getStateValue() throws MessagingException {
    return ((PopFolder)folder).getStateValueForMessage(this);
  }

  public String getUidl() throws MessagingException {
    if (uidl == null) {
      synchronized(this) {
        if (uidl == null) {
          ((PopFolder)folder).setUidlForMessage(this);
        }
      }
    }
    return uidl;
  }

  synchronized public void setUidl(String u) {
    uidl = u;
  }
}
