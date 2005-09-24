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
 * Created: Terry Weissman <terry@netscape.com>, 22 Aug 1997.
 * Kieran Maclean
 */

package grendel.storage;

import calypso.util.Assert;
import calypso.util.NullJavaEnumeration;

import java.io.IOException;
import java.util.Enumeration;
import java.util.Vector;        // Yes, I want the fully synchronized version.

import javax.mail.Flags;
import javax.mail.Folder;
import javax.mail.Message;
import javax.mail.Store;

abstract class FolderBase extends Folder implements FolderExtra {
  Vector fMessages;
  ByteStringTable fTable;
  MessageIDTable id_table;
  Vector observers = new Vector();

  FolderBase(Store s) {
    super(s);
    fMessages = new Vector();
  }

  public synchronized Message getMessage(int msgnum) {
    ensureLoaded();
    return (Message) fMessages.elementAt(msgnum - 1);
  }

  public synchronized Message[] getMessages() {
    ensureLoaded();
    Message[] result = new Message[fMessages.size()];
    fMessages.copyInto(result);
    return result;
  }

  public synchronized Message[] getMessages(int start,
                                            int end) {
    ensureLoaded();
    Message[] result = new Message[end - start + 1];
    for (int i=start ; i<=end ; i++) {
      result[i - start] = (Message) fMessages.elementAt(i - 1);
    }
    return result;
  }

  public synchronized Message[] getMessages(int msgnums[]) {
    ensureLoaded();
    Message[] result = new Message[msgnums.length];
    for (int i=msgnums.length - 1 ; i>=0 ; i--) {
      result[i] = (Message) fMessages.elementAt(msgnums[i] - 1);
    }
    return result;
  }

  void ensureLoaded() {
  }

  /** Returns the total number of messages in the folder, or -1 if unknown.
      This includes deleted and unread messages.
    */
  public int getMessageCount() {
    if (fMessages == null)
      return -1;
    else
      return fMessages.size();
  }

  void noticeInitialMessage(Message m) {
    fMessages.addElement(m);
// #### How the hell are we supposed to do this?
    // XXX This is not a good idea, this a general class
    //BerkeleyMessage bm=(BerkeleyMessage)m;
    //bm.setMessageNumber(fMessages.size() - 1);
  }

  ByteStringTable getStringTable() {
    if (fTable == null) {
      synchronized(this) {
        // Check again, making sure a different thread didn't just do this.
        if (fTable == null) {
          fTable = new ByteStringTable();
        }
      }
    }
    return fTable;
  }

  MessageIDTable getMessageIDTable() {
    if (id_table == null) {
      synchronized(this) {
        // Check again, making sure a different thread didn't just do this.
        if (id_table == null) {
          id_table = new MessageIDTable();
        }
      }
    }
    return id_table;
  }


  // This is just silly.  javamail won't let me call this directly, because
  // they protected the method; I have to put this stupid intermediate call in.
  void doNotifyMessageChangedListeners(int i, Message message) {
    notifyMessageChangedListeners(i, message);
  }

  static Flags permflags = null;
  public Flags getPermanentFlags() {
    if (permflags == null) {
      Flags result = new Flags();
      for (int i=0 ; i < MessageBase.FLAGDEFS.length ; i++) {
        if (MessageBase.FLAGDEFS[i].builtin != null)
          result.add(MessageBase.FLAGDEFS[i].builtin);
        else
          result.add(MessageBase.FLAGDEFS[i].non_builtin);
      }
      permflags = result;
    }
    return permflags;
  }



}
