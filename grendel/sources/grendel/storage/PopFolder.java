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
 * Created: Terry Weissman <terry@netscape.com>, 30 Oct 1997.
 */

package grendel.storage;

import java.io.IOException;

import java.util.Hashtable;
import java.util.StringTokenizer;

import javax.mail.Folder;
import javax.mail.FolderNotFoundException;
import javax.mail.Flags;
import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.MethodNotSupportedException;
import javax.mail.event.ConnectionEvent;

class PopFolder extends Folder implements FolderExtra {

  static public final String DELETED = "d";
  static public final String KEEP = "k";

  Hashtable leave;
  PopMessage msgs[];




  PopFolder(PopStore store) {
    super(store);
  }


  public String getName() {
    return "INBOX";
  }

  public String getFullName() {
    return "INBOX";
  }

  public Folder getParent() {
    return null;
  }

  public boolean exists() {
    return true;
  }

  public Folder[] list(String string) {
    return null;
  }

  public char getSeparator() {
    return '/';
  }

  public int getType() {
    return HOLDS_MESSAGES;
  }

  public boolean create(int i) {
    return true;
  }

  public boolean hasNewMessages() {
    return false;               // ### Is this right?  Or should it really
                                // go do the work and see?
  }

  public Folder getFolder(String string) throws MessagingException {
    throw new FolderNotFoundException();
  }

  public boolean delete(boolean flag) throws MessagingException {
    throw new MethodNotSupportedException();
  }

  public boolean renameTo(Folder folder) throws MessagingException {
    throw new MethodNotSupportedException();
  }

  public void open(int mode) throws MessagingException {
    if (isOpen()) return;
    PopStore store = (PopStore) this.store;
    store.connect();
    store.writeln("STAT");
    String line = store.readln();
    store.check(line, "Stat failed");
    StringTokenizer st = new StringTokenizer(line);
    st.nextToken();                                     //skip +OK
    int nummsgs = Integer.parseInt(st.nextToken());
    // stat_octetcount = Integer.parseInt(st.nextToken());
    msgs = new PopMessage[nummsgs];
    for (int i=0 ; i<nummsgs ; i++) {
      msgs[i] = new PopMessage(this, i + 1);
    }
    notifyConnectionListeners(ConnectionEvent.CLOSED);
  }

  public void close(boolean expunge) throws MessagingException {
    PopStore store = (PopStore) this.store;
    // Do the expunge, and create a new hashtable to save.
    Hashtable newleave = new Hashtable();
    for (int i=0 ; i<msgs.length ; i++) {
      PopMessage msg = msgs[i];
      boolean deleted = msg.isDeleted();
      if (expunge && deleted) {
        store.writeln("DELE " + msgs[i].getMessageNumber());
        store.check(store.readln(), "DELE failed");
      } else {
        String uidl = msg.getUidl();
        if (uidl != null) newleave.put(uidl, deleted ? DELETED : KEEP);
      }
    }
    try {
      store.saveLeaveOnServerList(newleave);
    } catch (IOException e) {
      throw new MessagingException("Couldn't save popstate file " + e);
    }
    leave = null;
    msgs = null;

    notifyConnectionListeners(ConnectionEvent.CLOSED);
    store.close();
  }


  public void setUidlForMessage(PopMessage m) throws MessagingException {
    int id = m.getMessageNumber();
    PopStore store = (PopStore) this.store;
    String response;
    String uidl = null;
    synchronized(store) {
      if (!store.uidlUnimplemented) {
        store.writeln("UIDL " + id);
        response = store.readln();
        if (!store.resultOK(response)) {
          store.uidlUnimplemented = true;
        } else {
          StringTokenizer st = new StringTokenizer(response);
          st.nextToken();
          st.nextToken();
          uidl = st.nextToken();
          m.setUidl(uidl);
          return;
        }
        boolean done = false;
        if (!store.xtndUnimplemented) {
          // So, we can't use UIDL.  Instead, see if we can use XTND XLST.
          // If we can, then we'll get back the message-id's for every
          // message.  So, we'll just go set all the messages's UIDLs now.
          store.writeln("XTND XLST Message-Id");
          response = store.readln();
          if (!store.resultOK(response)) {
            store.xtndUnimplemented = true;
          } else {
            int curid = 1;
            for (;;) {
              response = store.readln();
              if (response.startsWith(".")) break;
              StringTokenizer st = new StringTokenizer(response);
              int i = Integer.parseInt(st.nextToken());
              if (i != curid) {
                // Huh?  xtnd skipped a message or something.  Uh, go
                // use TOP instead.
                store.xtndUnimplemented = true;
              }
              if (!store.xtndUnimplemented) {
                st.nextToken();
                uidl = st.nextToken();
                msgs[i].setUidl(uidl);
                if (i == id) done = true;
                curid++;
              }
            }
          }
        }
        if (done) return;
        // So, we can't use UIDL, and we can't use XTND XLST.  So, try and
        // use TOP instead.  And if we can't do that, retrieve the whole
        // bloody message.
        if (!store.topUnimplemented) {
          store.writeln("TOP " + id + " 0");
          response = store.readln();
          if (!store.resultOK(response)) {
            store.topUnimplemented = true;
          }
        }
        if (store.topUnimplemented) {
          store.writeln("RETR " + id);
          store.check(store.readln(), "Can't get UIDL");
        }
        do {
          response = store.readln();
          if (uidl == null &&
              response.regionMatches(true, 0, "Message-Id:", 0, 11)) {
            uidl = response.substring(11).trim();
            m.setUidl(uidl);
          }
        } while (!response.startsWith("."));
        if (uidl == null) {
          throw new MessagingException("The POP3 server (%s) does not\n" +
                  "support UIDL, which Netscape Mail needs to implement\n" +
                  "the ``Leave on Server'' and ``Maximum Message Size''\n" +
                  "options.\n\n" +
                  "To download your mail, turn off these options in the\n" +
                  "Servers panel of the ``Mail and News'' Preferences.");
        }
      }
    }
  }



  public boolean isOpen() {
    return msgs != null;
  }

  static protected Flags permflags = null;
  public Flags getPermanentFlags() {
    if (permflags == null) {
      Flags result = new Flags();
      result.add(new Flags(Flags.Flag.DELETED));
      result.add(new Flags(Flags.Flag.RECENT));
      permflags = result;
    }
    return permflags;
  }

  public int getMessageCount() throws MessagingException {
    if (!isOpen()) {
      throw new IllegalStateException();
    }
    return msgs.length;
  }

  public Message getMessage(int i) throws MessagingException {
    if (!isOpen()) {
      throw new IllegalStateException();
    }
    return msgs[i - 1];
  }


  public void appendMessages(Message amessage[]) throws MessagingException {
    throw new MethodNotSupportedException();
  }

  public Message[] expunge() {
    return null;
  }

  public int getUndeletedMessageCount() throws MessagingException {
    if (!isOpen()) return 0;
    int result = msgs.length;
    for (int i=0 ; i<msgs.length ; i++) {
      if (msgs[i].isDeleted()) result--;
    }
    return result;
  }

  String getStateValueForMessage(PopMessage m) throws MessagingException {
    if (leave == null) return null;
    // Sigh.  Generally, we hope that leave is null.  It should be if we
    // aren't leaving stuff on the server.  But, if it's not, then we got
    // to go check it, which means we have to get the UIDL for this message,
    // which can be expensive.
    return (String) leave.get(m.getUidl());
  }


}
