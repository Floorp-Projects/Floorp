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
 * Created: Jamie Zawinski <jwz@netscape.com>, 20 Nov 1997.
 * Kieran Maclean
 */

package grendel.storage;

import calypso.util.Assert;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;

import java.util.Enumeration;

import java.awt.Component;
import java.net.URL;
import java.net.UnknownHostException;

import javax.mail.Folder;
import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.Session;
import javax.mail.Store;
import javax.mail.URLName;
//import javax.mail.event.ConnectionEvent;
import javax.mail.event.StoreEvent;

import grendel.util.Constants;


/** Store for News (NNTP) folders.
 * <p>
 * This class really shouldn't be public, but I haven't figured out how to
 * tie into javamail's Session class properly.  So, instead of using
 * <tt>Session.getStore(String)</tt>, you instead need to call
 * <tt>NewsStore.GetDefaultStore(Session)</tt>.
 */

public class NewsStore extends Store {
  
  protected NNTPConnection nntp = null;
  protected NewsRC newsrc = null;
  protected NewsFolderRoot root_folder = null;
  
  static protected NewsStore DefaultStore = null;
  
  public static Store GetDefaultStore(Session s) {
    if (DefaultStore == null) {
      DefaultStore = new NewsStore(s);
    }
    return DefaultStore;
  }
  
  
  public NewsStore(Session s) {
    super(s, null);
  }
  
  public NewsStore(Session s, URLName u) {
    super(s, u);
  }
  
  NewsRC getNewsRC() {
    return newsrc;
  }
  
  private void loadNewsRC(String host) throws IOException {
    Assert.Assertion(newsrc == null);
    String dir = System.getProperty("user.home");
    boolean secure = false;  // ####
    String name = null;
    
    if (Constants.ISUNIX) {
      
      String name1 = (secure ? ".snewsrc" : ".newsrc");
      String name2 = name1 + "-" + host;
      if (new File(dir, name2).exists())
        name = name2;
      else if (new File(dir, name1).exists())
        name = name1;
      else
        name = name2;
      
    } else if (Constants.ISWINDOWS) {
      
      String name1 = (secure ? "snews" : "news");
      String name2 = name1 + "-" + host + ".rc";
      String name3 = name1 + ".rc";
      if (new File(dir, name2).exists())
        name = name2;
      else if (new File(dir, name3).exists())
        name = name3;
      else
        name = name2;
      
    } else {
      Assert.NotYetImplemented("newsrc names only implemented for windows 'n' unix...");
    }
    
    File f = new File(dir, name);
    newsrc = new NewsRC(f);
    
    // If the newsrc file didn't exist, subscribe to some default newsgroups.
    if (!f.exists()) {
      String subs[] = getDefaultSubscriptions();
      for (int i = 0; i < subs.length; i++) {
        NewsRCLine ng = newsrc.getNewsgroup(subs[i]);
        ng.setSubscribed(true);
      }
    }
    
  }
  
  
  protected boolean protocolConnect(String host,
      int port,
      String user,
      String password)
      throws MessagingException {
    
    if (nntp != null)
      return true;   // Already connected.
    
    if (host == null) {
      // #### better name?
      host = session.getProperty("mail.default_nntp_server");
      if (host == null || host == "") {
        // #### pop up a dialog box instead?
        host = "news";
      }
    }
    
    try {
      loadNewsRC(host);
    } catch (IOException e) {
      throw new MessagingException("loading newsrc file", e);
    }
    
    nntp = new NNTPConnection();
    
    try {
      boolean status = nntp.connect(host, port, user, password);
      if (!status) return false;
      
    } catch (UnknownHostException e) {          // This sucks, Beavis!
      throw new MessagingException("Unknown host", e);
    } catch (IOException e) {
      throw new MessagingException("I/O Error", e);
    }
    
    if (!newsrc.file().exists()) {
      // #### subscribe to the server's default groups
    }
    
    notifyStoreListeners(StoreEvent.NOTICE, "opened"); // #### ???
//    notifyConnectionListeners(ConnectionEvent.OPENED);
    return true;
  }
  
  public void close() {
    
    if (nntp == null) {
      // already closed.
      Assert.Assertion(newsrc == null);
      return;
    }
    
    nntp.close();
    nntp = null;
    
    Assert.Assertion(newsrc != null);
    if (newsrc != null) {
      try {
        newsrc.save();
      } catch (IOException e) {
        // Sun doesn't allow us to signal here.  Bastards...
        // Just ignore it, I guess...
      }
    }
    
    notifyStoreListeners(StoreEvent.NOTICE, "closed"); // #### ???
//    notifyConnectionListeners(ConnectionEvent.CLOSED);
    root_folder = null;
  }
  
  
  public Folder getDefaultFolder() {
    if (root_folder == null) {
      synchronized (this) {
        if (root_folder == null) {  // double check
          root_folder = new NewsFolderRoot(this);
        }
      }
    }
    return root_folder;
  }
  
  public Folder getFolder(String name) throws MessagingException {
    return getDefaultFolder().getFolder(name);
  }
  
  public Folder getFolder(URL url) throws MessagingException {
    return getFolder(new URLName(url));
  }
  
  public Folder getFolder(URLName urlName) throws MessagingException {
    if (! urlName.getHost().equalsIgnoreCase(url.getHost()))
      throw new MessagingException("Not the same host.");
    if (urlName.getPort() != url.getPort())
      throw new MessagingException("Not the same port.");
    String file = urlName.getFile();
    if (file.contains("/"))
      throw new MessagingException("Invalid URL.");
    return getFolder(file);
  }
  
  InputStream getMessageStream(NewsMessage message, boolean headers_too)
  throws NNTPException, IOException {
    Folder f = message.getFolder();
    String group = (f == null ? null : f.getFullName());
    int n = (group == null ? -1 : message.getStorageFolderIndex());
    if (n == -1) {
/*
      String id = #### ;
      if (headers_too)
        return nntp.ARTICLE(id);
      else
        return nntp.BODY(id);
 */
      Assert.NotYetImplemented("NewsStore.getMessageStream via Message-ID");
      return null;
    } else {
      if (headers_too)
        return nntp.ARTICLE(group, n);
      else
        return nntp.BODY(group, n);
    }
  }
  
  /** Returns array of int: [ nmessages low hi ]
   */
  int[] getGroupCounts(NewsFolder folder) {
    String group = folder.getFullName();
    try {
      return nntp.GROUP(group);
    } catch (IOException e) {
      return null;
    }
  }
  
  /** Returns a list of newsgroups to which new users should be subscribed,
   * if they don't have a newsrc file.  Tries to ask the news server for
   * this list; otherwise, uses some builtin defaults.
   */
  String[] getDefaultSubscriptions() {
    String s[] = null;
    try {
      s = nntp.LIST_SUBSCRIPTIONS();
    } catch (NNTPException e) {           // command unsupported
    } catch (IOException e) {             // something else
    }
    if (s == null || s.length == 0)
      s = new String[] { "news.announce.newusers",
          "news.newusers.questions",
          "news.groups.questions",
          "alt.fan.mozilla"
      };
      return s;
  }
  
  void openNewsgroup(NewsFolder folder, long from, long to) {
    Enumeration e;
    try {
      e = nntp.getMessages(folder, from, to);
    } catch (NNTPException ex) {
      return;
    } catch (IOException ex) {
      return;
    }
    
    while (e.hasMoreElements()) {
      Message m = (Message) e.nextElement();
      folder.noticeInitialMessage(m);
    }
  }
}
