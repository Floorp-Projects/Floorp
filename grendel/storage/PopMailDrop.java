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
 * Created: Terry Weissman <terry@netscape.com>, 28 Aug 1997.
 */

package grendel.storage;

import java.io.BufferedReader;
import java.io.DataOutputStream;
import java.io.File;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.InterruptedIOException;
import java.io.RandomAccessFile;
import java.io.Reader;
import java.net.Socket;
import java.net.UnknownHostException;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.StringTokenizer;
import java.util.Vector;

import javax.mail.Folder;
import javax.mail.Message;
import javax.mail.MessagingException;

import grendel.util.Constants;

//import netscape.pop3.POP3Client;
//import netscape.pop3.POP3Status;
//import netscape.pop3.POP3MailDropStats;

class PopMailDrop implements MailDrop {
  static final String KEEP = "k";
  static final String DELETE = "d";

  String fHost;
  int fPort = 110;
  String fUser;
  String fPass;
  Folder fFolder;
  int fNumWaiting = -1;
  Socket fSocket;
  InputStream fInput;
  DataOutputStream fOutput;
  int stat_msgcount;
  int stat_octetcount;
  File popstatefile;
  boolean uidlUnimplemented;
  boolean xtndUnimplemented;
  boolean leaveMessagesOnServer;

  static final boolean DEBUG = false;
  static void Spew(String s) {
    if (DEBUG) System.out.println("PopMailDrop: " + s);
  }

  PopMailDrop() {
  }

  protected void clearAssumptionsAboutServer() {
    uidlUnimplemented = false;
    xtndUnimplemented = false;
  }

  void setHost(String hostname) {
    fHost = hostname;
    clearAssumptionsAboutServer();
  }

  void setPort(int port) {
    fPort = port;
    clearAssumptionsAboutServer();
  }

  void setUser(String username) {
    fUser = username;
    clearAssumptionsAboutServer();
  }

  void setPassword(String password) {
    fPass = password;
    clearAssumptionsAboutServer();
  }

  void setDestinationFolder(Folder dest) {
    fFolder = dest;
  }

  Folder getDestinationFolder() {
    return fFolder;
  }

  void setPopStateFile(File f) {
    popstatefile = f;
    Spew("popstatefile set to " + f);
  }

  File getPopStateFile() {
    return popstatefile;
  }

  void setLeaveMessagesOnServer(boolean value) {
    leaveMessagesOnServer = value;
  }

  boolean getLeaveMessagesOnServer() {
    return leaveMessagesOnServer;
  }

  /** All the real work is done here.  Go to the POP server, determine
    what messages need downloading, and optionally actually download them.
    (The only reason not to go ahead and download them is if we're doing a
    biff check.) */
  protected void fetchOrCheck(boolean dofetch) throws IOException {
    try {
      fNumWaiting = -1;
      String response;
      int bytesToLoad = 0;      // How many bytes total we're going to
                                // download.  Someday, this will be used for
                                // progress indication.  ###
      login();
      Hashtable leave = loadLeaveOnServerList();
      Hashtable newleave = null;
      stat();
      Vector getlist = new Vector();
      if (leave == null && !leaveMessagesOnServer) {
        for (int id=1 ; id<=stat_msgcount ; id++) {
          getlist.addElement(new Integer(id));
        }
        bytesToLoad = stat_octetcount;
      } else {
        int sizes[] = new int[stat_msgcount + 1];
        if (dofetch) {          // Only bother getting sizes if we're going
                                // to be downloading some of these.
          writeln("LIST");
          check(readln(), "LIST command failed");
          for (;;) {
            String line = readln();
            if (line.charAt(0) == '.') break;
            StringTokenizer st = new StringTokenizer(line);
            int id = Integer.parseInt(st.nextToken());
            int count = Integer.parseInt(st.nextToken());
            if (id >= 1 && id <= stat_msgcount) {
              sizes[id] = count;
            }
          }
        }
        boolean processingXtnd = false;
        for (int id=1 ; id<=stat_msgcount ; id++) {
          String uidl = null;
          if (!uidlUnimplemented) {
            writeln("UIDL " + id);
            response = readln();
            if (!resultOK(response)) {
              uidlUnimplemented = true;
            } else {
              StringTokenizer st = new StringTokenizer(response);
              st.nextToken();
              st.nextToken();
              uidl = st.nextToken();
            }
          }
          if (uidl == null && !xtndUnimplemented && id == 1) {
            writeln("XTND XLST Message-Id");
            response = readln();
            if (!resultOK(response)) {
              xtndUnimplemented = true;
            } else {
              processingXtnd = true;
            }
          }
          if (processingXtnd) {
            response = readln();
            if (response.startsWith(".")) {
              processingXtnd = false;
            } else {
              StringTokenizer st = new StringTokenizer(response);
              int i = Integer.parseInt(st.nextToken());
              if (i != id) {
                // Huh?  xtnd skipped a message or something.  Fine, throw
                // away all the xtnd data, go use TOP instead.
                do {
                  response = readln();
                } while (!response.startsWith("."));
                processingXtnd = false;
              } else {
                st.nextToken();
                uidl = st.nextToken();
              }
            }
          }
          if (uidl == null) {
            writeln("TOP " + id + " 0");
            check(readln(), "The POP3 server (%s) does not\n" +
                  "support UIDL, which Netscape Mail needs to implement\n" +
                  "the ``Leave on Server'' and ``Maximum Message Size''\n" +
                  "options.\n\n" +
                  "To download your mail, turn off these options in the\n" +
                  "Servers panel of the ``Mail and News'' Preferences.");

            do {
              response = readln();
              if (uidl == null &&
                  response.regionMatches(true, 0, "Message-Id:", 0, 11)) {
                uidl = response.substring(11).trim();
                Spew("Top found message-id: " + uidl);
              }
            } while (!response.startsWith("."));
            if (uidl == null) {
              // Gack.  No message-id in this message.  Um, well, choke.
              uidl = "unknown";        // ### Any better ideas???
              Spew("Top couldn't find message-id.");
            }
          }
          if (uidl.charAt(0) == '<' && uidl.endsWith(">")) {
            uidl = uidl.substring(1, uidl.length() - 1);
          }

          String code = null;
          if (leave != null) {
            code = (String) leave.get(uidl);
            if (code != null && !leaveMessagesOnServer && code.equals(KEEP)) {
              // ### This causes us to redownload anything that we were
              // keeping on the server if you turn off "keep messages on
              // server".  This is arguably wrong, because it floods peoples
              // mailboxes with messages they've probably already seen.  On
              // the other hand, just deleting it could potentially lose
              // mail for people.  Bleah!
              code = null;
            }
          }
          if (code == null) {
            getlist.addElement(new Integer(id));
            bytesToLoad += sizes[id];
            if (leaveMessagesOnServer) {
              code = KEEP;
            }
          }
          if (code != null) {
            if (newleave == null) {
              newleave = new Hashtable();
            }
            newleave.put(uidl, code);
          }
        }
        if (processingXtnd) {
          do {
            response = readln();
          } while (!response.startsWith("."));
          processingXtnd = false;
        }
      }

      fNumWaiting = getlist.size();

//      if (dofetch && fNumWaiting > 0) {
//        for (Enumeration e = getlist.elements() ; e.hasMoreElements() ; ) {
//          int id = ((Integer) e.nextElement()).intValue();
//          // ### Insert code here to do partial messages...
//          writeln("RETR " + id);
//          check(readln(), "Requesting message from server");
//          InputStream in = new DotTerminatedInputStream(fInput);
//          Message m = new MessageFromStream(in);
//          Message[] stupidlist = { m };
//          try {
//            fFolder.appendMessages(stupidlist);
//          } catch (MessagingException err) {
//            throw new IOException(err.toString());
//          }
//
//          // Do filters, something like:
//          FilterMaster.Get().applyFilters(m);
//          // though it probably makes more sense to do this before the
//          // fFolder.appendMessages() and not do that if the filter
//          // has already saved the message...djw
//
//          if (!leaveMessagesOnServer) {
//            writeln("DELE " + id);
//            check(readln(), "Deleting retrieved message from server");
//          }
//        }
//        fNumWaiting = 0;
//        if (leave != null || newleave != null) {
//          saveLeaveOnServerList(newleave);
//        }
//      }
      logout();
    } finally {
      cleanup();
    }
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
    synchronized(popstatefile) {
      // This is all done synchronized via the popstatefile, so that if we
      // have multiple pop servers they will not attempt to write to the file
      // at the same time.  This assumes that every pop server was given the
      // same File object to represent the popstate file.

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


  synchronized public void fetchNewMail() throws IOException {
    fetchOrCheck(true);
  }

  synchronized public void doBiff() throws IOException {
    fetchOrCheck(false);
  }

  protected void login() throws IOException {
    // ### I18N -- all these exceptions need to be internationalized.
    if (fHost == null) {
      throw new MailDropException("No host specified for POP3 maildrop");
    }
    if (fUser == null) {
      throw new MailDropException("No user specified for POP3 maildrop");
    }
    try {
      fSocket = new Socket(fHost, fPort);
    } catch (UnknownHostException e) {
      throw new MailDropException("Unknown host: " + e.getMessage());
    }

    fInput = fSocket.getInputStream();
    fOutput = new DataOutputStream(fSocket.getOutputStream());

    check(readln(), "Connecting to server");
    writeln("USER " + fUser);
    check(readln(), "USER command not accepted");
    writeln("PASS " + fPass);
    check(readln(), "Login failed");
    // ### Should probably do GURL at this point, if necessary, to remember
    // the URL used to manage this account.
  }

  protected void cleanup() {
    if (fSocket != null) {
      try {
        fSocket.close();
      } catch (IOException e) {
      }
    }
    fSocket = null;
    fInput = null;
    fOutput = null;
  }

  protected void logout() throws IOException {
    writeln("QUIT");
    check(readln(), "Logout failed");
  }

  protected void stat() throws IOException {
    writeln("STAT");
    String line = readln();
    check(line, "Stat failed");
    StringTokenizer st = new StringTokenizer(line);
    st.nextToken();                                     //skip +OK
    stat_msgcount = Integer.parseInt(st.nextToken());
    stat_octetcount = Integer.parseInt(st.nextToken());
  }

  protected void writeln(String line) throws IOException {
    Spew(">> " + line);
    fOutput.writeBytes(line + "\n");
  }

  StringBuffer inbuffer = new StringBuffer();
  protected String readln() throws IOException {
    inbuffer.setLength(0);
    int c;
    do {
      c = fInput.read();
      if (c >= 0) inbuffer.append((char) c);
    } while (c != '\n');
    Spew("<< " + inbuffer);
    return inbuffer.toString();
  }

  protected void check(String line, String err) throws MailDropException {
    if (!resultOK(line)) {
      throw new MailDropException(err + " (Server response: " + line + ")");
                                // ### I18N
    }
  }

  protected boolean resultOK(String line) {
    return line.startsWith("+OK");
  }



  public int getBiffState() {
    if (fNumWaiting < 0) return UNKNOWN;
    if (fNumWaiting == 0) return NONE;
    return NEW;
  }

  public int getNumMessagesWaiting() {
    return fNumWaiting;
  }

//  protected void check(POP3Status status) throws MailDropException {
//    check(status.getStatus(), status.getServerResponse());
//  }

  protected void check(boolean status, String response)
    throws MailDropException
  {
    if (!status) {
      throw new MailDropException("Pop server reports: " + response);
    }
  }


}
