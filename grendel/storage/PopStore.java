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
 * Created: Terry Weissman <terry@netscape.com>, 29 Oct 1997.
 */

package grendel.storage;

/** Store for Berkeley mail folders.
  <p>
  This class really shouldn't be public, but I haven't figured out how to
  tie into javamail's Session class properly.  So, instead of using
  <tt>Session.getStore(String)</tt>, you instead need to call
  <tt>PopStore.GetDefaultStore(Session)</tt>.
  */

import calypso.util.Assert;

import java.io.DataOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;

import java.net.Socket;
import java.net.UnknownHostException;
import java.net.URL;

import java.util.Enumeration;
import java.util.Hashtable;
import java.util.StringTokenizer;

import javax.mail.Folder;
import javax.mail.MessagingException;
import javax.mail.Session;
import javax.mail.Store;
import javax.mail.URLName;
//import javax.mail.event.ConnectionEvent;
import javax.mail.event.StoreEvent;

import grendel.util.Constants;

public class PopStore extends Store {
  static final boolean DEBUG = true;
  static void Spew(String s) {
    if (DEBUG) System.out.println("PopStore: " + s);
  }


  protected PopFolder defaultFolder;

  String fHost;
  int fPort = 110;
  String fUser;
  Folder fFolder;
  int fNumWaiting = -1;
  Socket fSocket;
  InputStream fInput;
  DataOutputStream fOutput;
  DotTerminatedInputStream pendingstream;
  File popstatefile;

  public boolean uidlUnimplemented;
  public boolean xtndUnimplemented;
  public boolean topUnimplemented;

  static protected PopStore DefaultStore = null;
  public static Store GetDefaultStore(Session s) {
    if (DefaultStore == null) {
      DefaultStore = new PopStore(s);
    }
    return DefaultStore;
  }

  public PopStore(Session s) {
    this(s, null);
  }

  public PopStore(Session s, URLName u) {
    super(s, u);
    defaultFolder = new PopFolder(this);
    String popfilename = s.getProperty("mail.popstate");
    if (popfilename == null) {
      // ### Gotta do better than System.out.println!
      System.out.println("No mail.popstate property specified; won't be able");
      System.out.println("to remember state for messages left on server.");
    } else {
      popstatefile = new File(popfilename);
    }
  }

  synchronized public boolean protocolConnect(String host,
                                              int port,
                                              String user,
                                              String password)
    throws MessagingException
  {

    if (fSocket != null) return true;   // Already connected.
    try {
      if (port != -1) fPort = port;
      fSocket = new Socket(host, fPort);
      fInput = fSocket.getInputStream();
      fOutput = new DataOutputStream(fSocket.getOutputStream());
      check(readln(), "Connecting to server");
      writeln("USER " + user);
      check(readln(), "USER command not accepted");
      writeln("PASS " + password);
      if (!resultOK(readln())) {
      	fSocket.close();
      	fSocket = null;         // Otherwise it thinks we're still connected
        return false;           // Authentication failed.
      }
    } catch (UnknownHostException e) {
      throw new MessagingException("Unknown host: " + e.getMessage());
    } catch (IOException e) {
      throw new MessagingException("Can't open socket: " + e.getMessage());
    }
    // ### Should probably do GURL at this point, if necessary, to remember
    // the URL used to manage this account.


    fHost = host;
    fUser = user;

    notifyStoreListeners(StoreEvent.NOTICE, "opened"); // #### ???
//    notifyConnectionListeners(ConnectionEvent.OPENED);
    return true;
  }

  synchronized public void close() {
    dealWithPendingStream();
    if (fSocket != null) {
      try {
        writeln("QUIT");
        check(readln(), "Logout failed");
        fSocket.close();
      } catch (IOException e) {
      } catch (MessagingException e) {
      }
      fSocket = null;
      fInput = null;
      fOutput = null;
    }
    notifyStoreListeners(StoreEvent.NOTICE, "closed"); // #### ???
//    notifyConnectionListeners(ConnectionEvent.CLOSED);
  }

  public Folder getDefaultFolder() {
    return defaultFolder;
  }

  public Folder getFolder(String name) throws MessagingException {
    if (name.equalsIgnoreCase("inbox")) return defaultFolder;
    throw new MessagingException("Pop has only one folder, named 'inbox'");
  }

  public Folder getFolder(URLName url) {
    Assert.NotYetImplemented("PopStore.getFolder(URLName)");
    return null;
  }


  synchronized protected void writeln(String line) throws MessagingException {
    dealWithPendingStream();
    Spew(">> " + line);
    try {
      fOutput.writeBytes(line + "\n");
    } catch (IOException e) {
      throw new MessagingException("Failed writing to pop server: " + e);
    }
  }

  StringBuffer inbuffer = new StringBuffer();
  synchronized protected String readln() throws MessagingException {
    dealWithPendingStream();
    inbuffer.setLength(0);
    int c;
    do {
      try {
        c = fInput.read();
      } catch (IOException e) {
        throw new MessagingException("Failed reading from pop server: " + e);
      }
      if (c >= 0) inbuffer.append((char) c);
    } while (c != '\n');
    Spew("<< " + inbuffer);
    return inbuffer.toString();
  }

  protected void check(String line, String err) throws MessagingException {
    if (!resultOK(line)) {
      throw new MessagingException(err + " (Server response: " + line + ")");
                                // ### I18N
    }
  }

  protected boolean resultOK(String line) {
    return line.startsWith("+OK");
  }

  protected void dealWithPendingStream() {
    if (pendingstream != null) {
      pendingstream.bufferUpEverything();
      pendingstream = null;
    }
  }

  synchronized InputStream sendTopOrRetr(int msgnum, int numlines)
    throws MessagingException
  {
    dealWithPendingStream();
    if (!topUnimplemented) {
      writeln("TOP " + msgnum + " " + numlines);
      String result = readln();
      if (resultOK(result)) {
        pendingstream = new DotTerminatedInputStream(fInput);
        return pendingstream;
      }
      topUnimplemented = true;
    }
    return sendRetr(msgnum);
  }

  synchronized InputStream sendRetr(int msgnum) throws MessagingException {
    dealWithPendingStream();
    writeln("RETR " + msgnum);
    check(readln(), "RETR failed");
    pendingstream = new DotTerminatedInputStream(fInput);
    return pendingstream;
  }


  protected Hashtable loadLeaveOnServerList() {
    if (popstatefile == null) return null;
    RandomAccessFile f = null;
    Hashtable result = null;
    try {
      f = new RandomAccessFile(popstatefile, "r");
      boolean found = false;
      for (String line = f.readLine() ; line != null ; line = f.readLine()) {
        if (line.length() == 0) continue;
        if (!found) {
          if (line.charAt(0) == '*') {
            StringTokenizer st = new StringTokenizer(line.substring(1));
            if (fHost.equals(st.nextToken())) {
              if (fUser.equals(st.nextToken())) {
                found = true;
              }
            }
          }
        } else {
          if (line.charAt(0) == '*') break; // Start of next host; we're done.
          if (result == null) {
            result = new Hashtable();
          }
          result.put(line.substring(2).trim(), line.substring(0, 1));
        }
      }
    } catch (IOException e) {
    } finally {
      if (f != null) {
        try {
          f.close();
        } catch (IOException e) {
        }
      }
    }
    return result;
  }

  protected void saveLeaveOnServerList(Hashtable table) throws IOException {
    if (popstatefile == null) return;
    synchronized(popstatefile) {
      // This is all done synchronized via the popstatefile, so that if we
      // have multiple pop servers they will not attempt to write to the file
      // at the same time.  This assumes that every pop server was given the
      // same File object to represent the popstate file.
      // ### Right now, that's not gonna happen, because every PopStore object
      // creates its own popstatefile.  Gotta fix that!

      RandomAccessFile in = null;
      RandomAccessFile out = null;
      File outfile = new File(popstatefile.getPath() + "-temp");
      try {
        out = new RandomAccessFile(outfile, "rw");
        out.writeBytes("# Netscape POP3 State File" + Constants.LINEBREAK +
                       "# This is a generated file!  Do not edit." +
                       Constants.LINEBREAK + Constants.LINEBREAK);

        if (table != null) {
          out.writeBytes("*" + fHost + " " + fUser + Constants.LINEBREAK);
          for (Enumeration e = table.keys() ; e.hasMoreElements() ; ) {
            String key = (String) e.nextElement();
            out.writeBytes((String) (table.get(key)) + " " + key +
                           Constants.LINEBREAK);
          }
        }
        if (popstatefile.exists()) {
          in = new RandomAccessFile(popstatefile, "r");
          boolean copyit = false;
          String line;
          for (line = in.readLine() ; line != null ; line = in.readLine()) {
            if (line.length() == 0) continue;
            if (line.charAt(0) == '*') {
              copyit = true;
              StringTokenizer st = new StringTokenizer(line.substring(1));
              if (fHost.equals(st.nextToken())) {
                if (fUser.equals(st.nextToken())) {
                  copyit = false;
                }
              }
            }
            if (copyit) {
              out.writeBytes(line.trim() + Constants.LINEBREAK);
            }
          }
          in.close();
          in = null;
        }
        out.close();
        out = null;
        outfile.renameTo(popstatefile);
      } finally {
        if (in != null) in.close();
        if (out != null) out.close();
      }
    }
  }
}
