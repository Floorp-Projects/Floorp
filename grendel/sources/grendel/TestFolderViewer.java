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
 * Created: Jamie Zawinski <jwz@mozilla.org>,  7-Sep-98.
*/

package grendel;

import java.io.File;
import java.io.RandomAccessFile;
import java.io.IOException;
import java.io.InputStream;
import java.io.DataInputStream;
import java.io.InputStreamReader;

import java.util.Properties;
import java.util.Hashtable;

import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.Folder;
import javax.mail.Session;

import java.util.Vector;
import java.util.Enumeration;
import java.util.NoSuchElementException;
import java.util.Date;

import calypso.util.ByteBuf;
import calypso.util.ArrayEnumeration;
import calypso.util.QSort;
import calypso.util.Comparer;
import calypso.util.NetworkDate;

import grendel.storage.BerkeleyStore;
import grendel.storage.MessageExtra;

import grendel.view.FolderView;
import grendel.view.FolderViewFactory;
import grendel.view.MessageSetView;
import grendel.view.ViewedMessage;


class TestFolderViewer {

  public static void main(String args[])
    throws IOException, MessagingException {
    if (args.length != 1) {
      System.out.println("usage: TestFolderViewer <mail-folder-pathname>\n");
      return;
    }

    // First, split the directory and file name, and store the directory
    // in the "mail.directory" property (since JavaMail expects that.)
    //
    File pathname = new File(new File(args[0]).getAbsolutePath());
    String dir = pathname.getParent();
    String name = pathname.getName();

    Properties props = new Properties();
    props.put("mail.directory", dir);

    // Make the global "session" and "store" object.
    //
    Session session = Session.getDefaultInstance(props, null);
    BerkeleyStore store = new BerkeleyStore(session);

    // Parse the folder.  This will use a .summary file if one exists and is
    // up to date; otherwise, it will parse the whole folder.  The `list'
    // array will hold Message (really, BerkeleyMessage) objects representing
    // each of the messages in the folder.  Message objects are lightweight:
    // really they are just info like sender/subject/offset-in-file, etc.
    //
    System.out.println("Folder " + new File(dir, name));
    Folder folder = store.getDefaultFolder().getFolder(name);
    Message list[] = folder.getMessages();
    System.out.println("Found " + list.length + " messages.");

    // Print out a summary of the contents of the folder.
    threadAndPrint(folder, MessageSetView.DATE, true);

    // Interact with the user.
    mainLoop(folder);
  }


  // Gag -- for some reason our Message objects don't have getMessageNumber()
  // set in them (see FolderBase.noticeInitialMessage()) so until this is
  // fixed, let's kludge around it by keeping track of the index of the
  // message in its folder externally, in this hash table.  This is totally
  // the wrong thing, but for now, it's expedient.
  //
  static Hashtable msgnum_kludge;

  static void threadAndPrint(Folder folder, int sort_type, boolean thread_p) {
    FolderView view = FolderViewFactory.Make(folder);
    int order[] = { sort_type, MessageSetView.NUMBER };

    // See "gag" comment above.
    // Populate the msgnum_kludge with message -> folder-index numbers.
    msgnum_kludge = new Hashtable();
    try {
      for (int i = 0; i < folder.getMessageCount(); i++)
        msgnum_kludge.put(folder.getMessage(i+1), new Integer(i+1));
    } catch (MessagingException e) {
      System.out.println("Error: " + e);
    }

    System.out.println("Sorting by " +
                       (sort_type == MessageSetView.NUMBER  ? "number" :
                        sort_type == MessageSetView.DATE    ? "date" :
                        sort_type == MessageSetView.SUBJECT ? "subject" :
                        sort_type == MessageSetView.AUTHOR  ? "author" :
                        sort_type == MessageSetView.READ    ? "read" :
                        sort_type == MessageSetView.FLAGGED ? "flagged" :
                        sort_type == MessageSetView.SIZE    ? "size" :
                        sort_type == MessageSetView.DELETED ? "deleted" :"?")
                       + ".");

    System.out.println(thread_p ? "Threading." : "Not threading.");

    // Tell the FolderView how to sort/thread, and then do it.
    //
    view.setSortOrder(order);
    view.setIsThreaded(thread_p);
    view.reThread();

    // Now show the result.
    printThread(view.getMessageRoot(), 0);
  }


  static void printThread(ViewedMessage vm, int depth) {
    Message msg = (Message) vm.getMessage();
    String str = "";

    for (int i = 0; i < depth; i++)
      str += "  ";

    if (msg == null) {
      // A ViewedMessage with no Message inside is a dummy container, holding
      // a thread together (for example, when the parent message of two
      // siblings is not present in the folder (expired or deleted.))
      //
      str += "    [dummy]";

    } else {

      // Construct a string describing this message.
      //
      String a = "", s = "", n = "";
      Date d = null;

      try {
        a = ((MessageExtra) msg).getAuthor();
      } catch (MessagingException e) {
      }

      try {
        s = msg.getSubject();
      } catch (MessagingException e) {
      }

      // See "gag" comment, above.
      int ni = ((Integer) msgnum_kludge.get(msg)).intValue() - 1;
//      int ni = msg.getMessageNumber();

      n = "" + ni;
      if      (ni < 10) n += "    ";
      else if (ni < 100) n += "   ";
      else if (ni < 1000) n += "  ";
      else if (ni < 10000) n += " ";

      str = n + str;

      int L = str.length();

      if (a.length() > 23-L) a = a.substring(0, 23-L);
      if (s.length() > 23)   s = s.substring(0, 23);

      str += a;
      for (int i = L+a.length(); i < 25; i++)
        str += " ";

      str += s;
      for (int i = s.length(); i < 25; i++)
        str += " ";

      try {
        d = msg.getSentDate();
      } catch (MessagingException e) {
      }
      if (d != null && d.getTime() != 0)
        str += d;
      else
        str += "date unknown";
    }

    // Print the string describing this message.
    System.out.println(str);

    // If this message has children, print them now (indented.)
    // After printing this message's children/grandchildren,
    // move on and print the next message in the list.  (Note
    // that we're walking the list by recursing, but that's
    // probably ok, as its tail-recursion.)
    //
    ViewedMessage next = vm.getNext();
    ViewedMessage kid = vm.getChild();
    if (kid != null) printThread(kid, depth+1);
    if (next != null) printThread(next, depth);
  }


  static void mainLoop(Folder f) throws IOException {

    DataInputStream in = new DataInputStream(System.in);

    while (true) {

      // Read a line from the user; parse an integer from it; and dump
      // the selected message to stdout.  Then repeat.
      //
      System.out.print("\nDisplay which message: ");
      String s = in.readLine();
      try {
        int n = Integer.parseInt(s, 10);
        System.out.println("Displaying message " + n + ".");

        try {
          Message m = f.getMessage(n+1);

          try {
            InputStream stream =
              ((MessageExtra)m).getInputStreamWithHeaders();
//            if (makeRealHTML) {
//              stream = new MakeItHTML(stream).getHTMLInputStream();
//            }
            InputStreamReader reader = new InputStreamReader(stream);
            char buff[] = new char[4096];
            int count;
            System.out.println("\n-----------");
            while ((count = reader.read(buff, 0, 4096)) != -1) {
              System.out.println(new String(buff, 0, count));
            }
            System.out.println("\n-----------");
          } catch (MessagingException e) {
            System.out.println("Error: " + e);
          } catch (IOException e) {
            System.out.println("Error: " + e);
          }

        } catch (MessagingException e) {
          System.out.println("Error: " + e);
        }

      } catch (NumberFormatException e) {
        System.out.println("That's not a number.");
      }
    }
  }


}
