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
 */

package grendel.storage;

import java.io.File;
import java.io.RandomAccessFile;
import java.io.IOException;
import java.util.Properties;

import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.Folder;
import javax.mail.Session;

class TestBerkeley {
  public static void main(String args[])
    throws IOException, MessagingException
  {
    Properties props = new Properties();
    String dir = "/u/jwz/tmp/.netscape/nsmail/";
    props.put("mail.directory", dir);
    Session session = Session.getDefaultInstance(props, null);
    BerkeleyStore store = new BerkeleyStore(session);
    if (args.length == 0) {
      String def[] = { "Inbox" };
      args = def;
    }
    for (int i = 0; i < args.length; i++) {
      System.out.println("Folder " + dir + args[i]);
      Folder folder = store.getDefaultFolder().getFolder(args[i]);
      Message list[] = folder.getMessages();
      System.out.println("Found " + list.length + " messages.");
    }
  }
}
