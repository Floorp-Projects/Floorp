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
 * Created: Terry Weissman <terry@netscape.com>,  2 Dec 1997.
 */

package grendel.storage;



import javax.mail.Folder;
import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.Store;

public class SelfTest extends grendel.SelfTest {


  public static void main(String args[]) {
    SelfTest st = new SelfTest();
    st.startTests(args);
    st.runTests();
    st.endTests();
  }


  public void startTests(String args[]) {
    super.startTests(args);
  }

  public void runTests() {
    try {
      writeMessage(null, "berkeley", "Starting test of Berkeley folders...");
      berkeley();
      writeMessage(null, "berkeley", "... finished test of Berkeley folders.");
      writeMessage(null, "pop3", "Starting test of Pop3 folders...");
      pop3();
      writeMessage(null, "pop3", "... finished test of Pop3 folders.");
    } catch (Throwable e) {
      writeFailure(e, "punt", "Can't continue SelfTest because of fatal " +
                   "problem: " + getStackTrace(e));
    }
  }


  public void endTests() {
    super.endTests();
  }


  public void checkFolder_1(String mname,
                            Folder f,
                            int numTotal,
                            int numUndeleted,
                            int numUnread,
                            int numNew,
                            String[] authors,
                            String[] subjects,
                            long[] sentdates)
    throws MessagingException
  {
    FolderExtra fextra = FolderExtraFactory.Get(f);
    if (fextra.getUndeletedMessageCount() != numUndeleted) {
      if (fextra.getUndeletedMessageCount() == -1) {
        // Bug97114
        writeKnownBug(f, mname, 97114,
                     "Got -1 undeleted messages; kinda rude and weird.");
      } else {
        writeFailure(f, mname,
                     "Expected " + numUndeleted + " undeleted messages; got " +
                     fextra.getUndeletedMessageCount());
      }
    }
    if (f.getUnreadMessageCount() != numUnread) {
      if (f.getUnreadMessageCount() == -1) {
        // Bug97114
        writeKnownBug(f, mname, 97114,
                     "Got -1 unread messages; kinda rude and weird.");
      } else {
        writeFailure(f, mname,
                     "Expected " + numUnread + " unread messages; got " +
                     f.getUnreadMessageCount());
      }
    }
    if (f.getNewMessageCount() != numNew) {
      writeFailure(f, mname,
                   "Expected " + numNew + " new messages; got " +
                   f.getNewMessageCount());
    }
    if (f.getMessageCount() != numTotal) {
      writeFailure(f, mname,
                   "Expected " + numTotal + " total messages; got " +
                   f.getMessageCount());
      return;                   // No use proceeding if this one fails;
                                // we'll just get array subscript errors or
                                // other confusing stuff.
    }
    Message msgs[] = f.getMessages();
    if (msgs.length != numTotal) {
      writeFailure(f, mname,
                   "getMessages() returned " +
                   msgs.length +
                   " messages; expected " + numTotal);
      return;
    }
    for (int i=0 ; i<numTotal ; i++) {
      Message msg = msgs[i];
      MessageExtra mextra = MessageExtraFactory.Get(msg);
      if (!mextra.getAuthor().equals(authors[i])) {
        writeFailure(f, mname,
                     "message " + i + " has author " + mextra.getAuthor() +
                     "; expected " + authors[i]);
      }
      if (!msg.getSubject().equals(subjects[i])) {
        writeFailure(f, mname,
                     "message " + i + " has subject " + msg.getSubject() +
                     "; expected " + subjects[i]);
      }
      if (msg.getSentDate().getTime() != sentdates[i]) {
        writeFailure(f, mname,
                     "message " + i + " has sentdate " +
                     prettyTime(msg.getSentDate().getTime()) +
                     "; expected " +
                     prettyTime(sentdates[i]));
      }
    }
  }

  public void checkFolder(String mname,
                          Folder f,
                          int numTotal,
                          int numUndeleted,
                          int numUnread,
                          int numNew,
                          String[] authors,
                          String[] subjects,
                          long[] sentdates)
    throws MessagingException
  {
    writeMessage(f, mname, "Checking out folder " + f.getName() + "...");
    f.open(Folder.READ_WRITE);
    checkFolder_1(mname, f, numTotal, numUndeleted, numUnread, numNew, authors,
                  subjects, sentdates);
    writeMessage(f, mname, "Closing and reopening " + f.getName() +
                 " and checking again...");
    f.close(false);
    f.open(Folder.READ_WRITE);
    checkFolder_1(mname, f, numTotal, numUndeleted, numUnread, numNew, authors,
                  subjects, sentdates);
  }


  public void berkeley() throws MessagingException {
    makePlayDir();
    installFile("selftestdata/Inbox", "Inbox");
    props.put("mail.directory", playdir.getAbsolutePath());

    BerkeleyStore store = new BerkeleyStore(session);
    store.connect();
    Folder f = store.getDefaultFolder();
    if (!f.getName().equalsIgnoreCase(playdir.getName())) {
      writeFailure(f, "berkeley",
                   "Folder should be named " +
                   playdir.getName() +
                   "; is instead named " +
                   f.getName());
      return;
    }

    try {
      f.open(Folder.READ_WRITE);
    } catch (Throwable e) {
      writeKnownBug(f, "berkeley", 97109,
                    "can't open a folder which is just a directory." +
                    getStackTrace(e)); //Bug97109 ###
    }

    Folder flist[] = f.list();
    if (flist == null || flist.length != 1) {
      writeFailure(f, "berkeley",
                   "Failed to get back one subfolder of root folder");
      return;
    }

    f = flist[0];
    if (!f.getName().equalsIgnoreCase("Inbox")) {
      writeFailure(f, "berkeley",
                   "Subfolder isn't named Inbox; is instead named" +
                   f.getName());
      return;
    }


    checkFolder("berkeley", f, 3, 2, 1, 0,
                new String[] {"Jamie Zawinski <jwz@netscape.com>",
                                "Eric Mader <emader@netscape.com>",
                                "dcasados@netscape.com (Debbie Casados)"},
                new String[] {"Re: Email bug was [Fwd: Apologies and complaints]",
                                "[Fwd: Luggage]",
                                "SITN REGISTRATION INFO"},
                new long[] {834197921000L,
                              879964342000L,
                              881090639000L});
  }

  void pop3() throws MessagingException {
//    props.put("pop.host", "dome");
//    props.put("pop.user", "poptest");
//    props.put("pop.password", "poptest");
    props.put("mail.host", "dome");
    setUserAndPassword("poptest", "poptest");
    PopStore store = new PopStore(session);
    store.connect();
    Folder f = store.getDefaultFolder();
    if (!f.getName().equalsIgnoreCase("INBOX")) {
      writeFailure(f, "pop3",
                   "Folder should be named INBOX; is instead named " +
                   f.getName());
    }
    Folder flist[] = f.list();
    if (flist != null && flist.length != 0) {
      writeFailure(f, "pop3", f.getName() + " claims to have " + flist.length +
                   "subfolders, not possible in pop3.");
    }

    checkFolder("pop3", f, 2, 2, 0, 2,
                new String [] {"Accounts@dome.mcom.com",
                                 "\"Terry Weissman\""},
                new String [] {"Form: Greeting Pop Test",
                                 "Welcome, oh pop test account."},
                new long [] {881356029000L,
                               881356087000L});
  }

}
