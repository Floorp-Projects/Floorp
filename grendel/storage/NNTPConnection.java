/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 *
 * Created: Jamie Zawinski <jwz@netscape.com>, 20 Nov 1997.
 */

package grendel.storage;

import java.io.BufferedReader;
import java.io.File;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.IOException;

import java.net.Socket;
import java.net.UnknownHostException;

import java.text.DecimalFormat;

import java.util.Enumeration;
import java.util.StringTokenizer;
import java.util.Vector;
import java.util.Date;
import java.util.Calendar;
import java.util.GregorianCalendar;
import java.util.TimeZone;

import java.util.NoSuchElementException;

import grendel.storage.DotTerminatedInputStream;
import calypso.util.LineBufferingInputStream;

import calypso.util.Assert;
import calypso.util.ByteBuf;

import javax.mail.internet.InternetHeaders;


class NNTPConnection {

  static final boolean DEBUG = false;

  Socket socket = null;
  LineBufferingInputStream input;
  OutputStream output;
  String selected_group = null;
  String host = null;
  int    port = -1;
  String user = null;
  String pass = null;

  /* If we have returned to a caller a stream which points to the NNTP
     connection (for example, for reading BODY or XOVER data) we hold on
     to it here.  If a caller tries to interact with the NNTP connection
     before this stream has been drained, we fully drain it first.  This
     means two things:

       1: if the caller gets a stream, then tries to do some other NNTP
          thing while the stream is pending, all streaming-ness is shot,
          since the second NNTP request will block waiting for all of
          the stream data to come in and be buffered first.

       2: if thread A is reading from a stream, and thread B tries to
          do some short-running NNTP action, thread A will block, and
          thread B will fill up the whole buffer A is waiting for
          before doing its own action.

     There's no way around #1; within a thread, that's how things need
     to work.  But, in the #2 case, it might be nicer to have thread B
     block until thread A had fully drained its stream.  But not only
     do I not know how to detect that situation, it's rife with other
     problems too; what if thread A is dead, or reading really slowly,
     or...
   */
  protected NNTPDotTerminatedInputStream pending_dot_stream = null;

  static public final int DEFAULT_NNTP_PORT = 119;

  protected boolean hasSetGetExtension = false;
  protected boolean hasOverExtension = false;
  protected boolean hasXpatTextExtension = false;
  protected boolean hasListSubscrExtension = false;

  NNTPConnection() {
  }

  synchronized boolean connect(String host, int port,
                               String user, String password)
    throws UnknownHostException, IOException {

    if (socket != null) {
      close();
    }

    this.host = host;
    this.port = port;

    if (port == -1) port = DEFAULT_NNTP_PORT;

    if (DEBUG) {
      System.err.println("NNTP: connect(\"" + host + "\", " + port + ")");
    }

    socket = new Socket(host, port);
    input = new LineBufferingInputStream(socket.getInputStream());
    output = socket.getOutputStream();

    // This would be technically legal; by why tempt fate.
    // input.setInputEOL(new ByteBuf("\r\n"));

    if (DEBUG) {
      System.err.println("NNTP: connected.");
    }

    int code = readResponse(null);       // 200 foo NNRP ready (posting ok).
    // #### check "posting allowed" code


    // #### do async auth -- return false if it fails
    // #### or throw AuthenticationFailedException?
    this.user = user;
    this.pass = password;

    // Do some initial setup.
    MODE_READER();
    LIST_EXTENSIONS();
    return true;
  }

  synchronized void close() {
    if (DEBUG) {
      System.err.println("NNTP: close()");
    }

    if (output != null) {       // try to shut down cleanly; ignore errors.
      try {
        write("QUIT\r\n");
      } catch (IOException e) {
      }
    }

    user = null;
    pass = null;
    selected_group = null;
    try { input.close();  } catch (IOException e) {}
    try { output.close(); } catch (IOException e) {}
    try { socket.close(); } catch (IOException e) {}
    input = null;
    output = null;
    socket = null;
  }

  protected synchronized void write(byte b[], int start, int length)
    throws IOException {

    if (DEBUG) {
      String s = "NNTP: ==> ";
      for (int i = start; i < start+length; i++)
        s += (b[i] == (byte)'\r' ? "\\r" :
              b[i] == (byte)'\n' ? "\\n" :
              b[i] == (byte)'\t' ? "\\t" :
              b[i] == (byte)'\\' ? "\\\\" :
              new String(b, i, 1));
      System.err.print(s + "\n");
    }

    output.write(b, start, length);
  }

  protected synchronized void write(String buf) throws IOException {
    write(buf.getBytes(), 0, buf.length());
  }

  protected synchronized void readLine(ByteBuf into_buf) throws IOException {

    flushDotStream();

    // Note that `input' is a LineBufferingInputStream, which means that
    // `read' will only return chunks consisting of a single line (or
    // max_bytes, whichever is smaller.)
    //
    into_buf.setLength(0);
    into_buf.read(input, 10240);  // NNTP spec limits lines to 1000 characters

    if (DEBUG) {
      String s = "NNTP: <== ";
      byte b[] = into_buf.toBytes();
      for (int i = 0; i < into_buf.length(); i++)
        s += (b[i] == (byte)'\r' ? "\\r" :
              b[i] == (byte)'\n' ? "\\n" :
              b[i] == (byte)(byte)'\t' ? "\\t" :
              b[i] == '\\' ? "\\\\" :
              new String(b, i, 1));
      System.err.print(s + "\n");
    }

    // strip off trailing newlines (really, all ctl chars)
    int i = into_buf.length();
    int j = i;
    while (j > 0 &&
           (into_buf.byteAt(j-1) == (byte)'\r' ||
            into_buf.byteAt(j-1) == (byte)'\n'))
      j--;
    if (i != j)
      into_buf.setLength(j);
  }

  protected synchronized String readLine() throws IOException {
    ByteBuf b = new ByteBuf();
    readLine(b);
    return b.toString();
  }

  protected void flushDotStream() {
    if (pending_dot_stream != null) {
      synchronized (this) {
        if (pending_dot_stream != null) {
          if (DEBUG)
            System.err.println("NNTP: Yikes!  Reentrant call!\n" +
                               "NNTP: Buffering content of outstanding " +
                                "dot-terminated input stream...");
          // new Exception().printStackTrace(System.err);
          pending_dot_stream.bufferUpEverything();
          // dotStreamFinished should now have been called.
          Assert.Assertion(pending_dot_stream == null);
        }
      }
    }
  }

  protected synchronized NNTPDotTerminatedInputStream newDotStream() {
    flushDotStream();
    Assert.Assertion(pending_dot_stream == null);
    if (DEBUG) {
      System.err.println("NNTP: returning dot-terminated input stream.");
      // new Exception().printStackTrace(System.err);
    }
    pending_dot_stream = new NNTPDotTerminatedInputStream(this, input);
    return pending_dot_stream;
  }

  protected void checkString(String s) throws NNTPException {
    int L = s.length();
    for (int i = 0; i < L; i++)
      if (s.charAt(i) <= ' ')
        throw new NNTPException("bad character in string: " + s);
  }

  protected synchronized int readResponse(ByteBuf into_buf) throws IOException{
    ByteBuf b = (into_buf != null ? into_buf : new ByteBuf());
    readLine(b);
    if (b.length() < 4)
      throw new NNTPException("improper NNTP response: \"" + b + "\"");

    byte b0 = b.byteAt(0);
    byte b1 = b.byteAt(1);
    byte b2 = b.byteAt(2);
    if (b0 != (byte)'1' && b0 != (byte)'2' && b0 != (byte)'3')
      throw new NNTPException("NNTP error: \"" + b + "\"");

    return (((b0-(byte)'0') * 100) + ((b1-(byte)'0') * 10) + (b2-(byte)'0'));
  }


  protected synchronized void MODE_READER() throws IOException {
    write("MODE READER\r\n");
    try {
      readResponse(null);
    } catch (NNTPException e) {
      // ignore errors (probably meaning "mode reader not supported.")
    }
  }


  /** Returns an array of Strings, the names of extensions supported by
      this server.  If no extensions are supported, returns null.
    */
  protected synchronized String[] LIST_EXTENSIONS() throws IOException {
    write("LIST EXTENSIONS\r\n");
    try {
      readResponse(null);       // 215 Extensions supported by server.
      Vector v = new Vector();
      ByteBuf b = new ByteBuf();

      hasSetGetExtension = false;
      hasOverExtension = false;
      hasXpatTextExtension = false;
      hasListSubscrExtension = false;

      while (true) {            // dot-terminated list of extensions
        readLine(b);
        if (b.equals("."))
          break;
        else if (b.byteAt(0) == (byte)'.')
          b.remove(0, 1);
        if      (b.equals("SETGET"))     hasSetGetExtension = true;
        else if (b.equals("OVER"))       hasOverExtension = true;
        else if (b.equals("XPATTEXT"))   hasXpatTextExtension = true;
        else if (b.equals("LISTSUBSCR")) hasListSubscrExtension = true;
        // #### etc
      }

      String ss[] = new String[v.size()];
      Enumeration e = v.elements();
      for (int i = 0; e.hasMoreElements(); i++)
        ss[i] = (String) e.nextElement();
      return ss;

    } catch (NNTPException e) {
      // ignore errors (probably meaning "list extensions not supported".)
      return new String[0];
    }
  }

  synchronized void LIST_SEARCHES() {
    // #### if SEARCH
  }

  synchronized void LIST_SRCHFIELDS() {
    // #### if SEARCH
  }

  synchronized String GET(String prop) throws IOException {
    checkString(prop);
    if (!hasSetGetExtension)
      return null;
    else {
      write("GET " + prop + "\r\n");
      readResponse(null);       // 209 values follow
      ByteBuf b = new ByteBuf();
      String target = prop + " ";
      int tl = target.length();
      String result = null;

      while (true) {            // dot-terminated list of "KEY value"
        readLine(b);
        if (b.equals("."))
          break;
        else if (b.byteAt(0) == (byte)'.')
          b.remove(0, 1);

        if (b.regionMatches(true, 0, target, 0, tl)) {
          String s = b.toString().substring(tl);
          if (result == null)
            result = s;
          else
            result = result + "\n" + s;
        }
      }
      return result;
    }
  }


  /** Returns an array of strings, the names of newsgroups to which new users
      of this server should be subscribed by default.
   */
  synchronized String[] LIST_SUBSCRIPTIONS() throws IOException {
    if (!hasListSubscrExtension)
      return new String[0];
    else {
      Vector v = new Vector();
      write("LIST SUBSCRIPTIONS\r\n");
      readResponse(null);       // 215 default newsgroups.
      ByteBuf b = new ByteBuf();
      while (true) {            // dot-terminated list of group names
        readLine(b);
        if (b.equals("."))
          break;
        else if (b.byteAt(0) == (byte)'.')
          b.remove(0, 1);

        v.addElement(b.toString());
      }

      String ss[] = new String[v.size()];
      Enumeration e = v.elements();
      for (int i = 0; e.hasMoreElements(); i++)
        ss[i] = (String) e.nextElement();
      return ss;
    }
  }


  /** Returns array of int: [ nmessages low hi ]
    */
  synchronized int[] GROUP(String group_name) throws IOException {
    checkString(group_name);
    write("GROUP " + group_name + "\r\n");
    ByteBuf b = new ByteBuf();

    try {
      readResponse(b);

    } catch (NNTPException e) {
      // If we got an error selecting this group, reconnect and try again,
      // just once more.  If we get an error this time, give up.
      if (DEBUG)
        System.err.println("NNTP: rebooting connection...");
      connect(host, port, user, pass);
      write("GROUP " + group_name + "\r\n");
      readResponse(b);
    }

    // parse response: 211 2553 609025 611724 alt.test
    StringTokenizer st = new StringTokenizer(b.toString(), " \t", false);
    int result[] = new int[3];
    for (int i = 0; i < (1+result.length); i++) {
      int x;
      try {
        x = Integer.parseInt((String) st.nextToken());
      } catch (NoSuchElementException e) {
        x = -1;
      }
      if (i != 0)
        result[i-1] = x;
    }
    selected_group = group_name;
    return result;
  }

  private synchronized InternetHeaders readHeaders(String terminator)
    throws IOException {
    ByteBuf b = new ByteBuf();
    InternetHeaders h = new InternetHeaders();
    while (true) {
      readLine(b);
      if (b.equals(terminator))
        break;
      else if (b.byteAt(0) == (byte)'.')
        b.remove(0, 1);
      h.addHeaderLine(b.toString());
    }
    return h;
  }

  synchronized NewsMessage HEAD(NewsFolder folder, String id)
    throws IOException {
    if (folder == null)
      throw new NullPointerException();
    checkString(id);
    write("HEAD " + id + "\r\n");
    readResponse(null);            // 221 0 head <346D5A48.399F@host>
    InternetHeaders h = readHeaders(".");
    return new NewsMessage(folder, h);
  }

  synchronized NewsMessage HEAD(NewsFolder folder, long article)
    throws IOException {
    String group = folder.getFullName();
    GROUP(group);
    NewsMessage m = HEAD(folder, Long.toString(article));
    m.setStorageFolderIndex((int) article);
    return m;
  }

  /** Returns a stream of the message's body.  This takes care of the
      dot termination for you.  You <I><B>must</B></I> drain this stream
      before issuing another NNTP command.
   */
  synchronized InputStream BODY(String id) throws IOException {
    checkString(id);
    write("BODY " + id + "\r\n");
    readResponse(null);          // 221 609025 <346D5A48.399F@grid.lock> head
    return newDotStream();
  }

  synchronized InputStream BODY(String group, long article)
    throws IOException {
    checkString(group);
    GROUP(group);
    return BODY(Long.toString(article));
  }

  /** Returns a stream of the full message, including headers and body.
      This takes care of the dot termination for you.  You <I><B>must</B></I>
      drain this stream before issuing another NNTP command.
   */
  synchronized InputStream ARTICLE(String id) throws IOException {
    checkString(id);
    write("ARTICLE " + id + "\r\n");
    readResponse(null);          // 221 609025 <346D5A48.399F@grid.lock> head
    return newDotStream();
  }

  synchronized InputStream ARTICLE(String group, long article)
    throws IOException {
    GROUP(group);
    return ARTICLE(Long.toString(article));
  }

  /** Returns a stream listing the new newsgroups added since the given date.
      If the date is null, lists all of them.  The stream lists the group
      names one per line, in no particular order.
    */
  synchronized InputStream NEWGROUPS(Date since) throws IOException {
    String cmd;
    if (since == null) {
      cmd = "NEWGROUPS\r\n";
    } else {
      DecimalFormat twod = new DecimalFormat("00");
      Calendar c = new GregorianCalendar();
      c.setTime(since);
      c.setTimeZone(TimeZone.getTimeZone("GMT"));
      cmd = ("NEWGROUPS " +
             twod.format(c.get(c.YEAR)) +
             twod.format(c.get(c.MONTH)) +
             twod.format(c.get(c.DAY_OF_MONTH)) +
             " " +
             twod.format(c.get(c.HOUR_OF_DAY)) +
             twod.format(c.get(c.MINUTE)) +
             twod.format(c.get(c.SECOND)) +
             " GMT" +
             "\r\n");
    }

    write(cmd);

    try {
      readResponse(null);          // 231 New newsgroups follow.
    } catch (NNTPException e) {
      // If we got an error, reconnect and try again, just once more.
      // If we get an error this time, give up.
      if (DEBUG)
        System.err.println("NNTP: rebooting connection...");
      connect(host, port, user, pass);
      write(cmd);
      readResponse(null);
    }

    return newDotStream();
  }


  /** Sends the OVER or XOVER command, as appropriate, and returns a stream
      of the overview data.  If neither OVER nor XOVER is supported,
      throws an NNTPException.
   */
  synchronized InputStream OVER(String group, long from, long to)
    throws IOException {
    if (to < from)
      throw new NNTPException("arguments out of order: " + from + ", " + to);
    GROUP(group);
//    String cmd = (hasOverExtension ? "OVER" : "XOVER");
    // #### wait, OVER is weird...
    String cmd = "XOVER";
    if (from <= 0)
      write(cmd + "\r\n");
    else if (from == to)
      write(cmd + " " + from + "\r\n");
    else
      write(cmd + " " + from + "-" + to + "\r\n");

    readResponse(null);            // 224 data follows
    return newDotStream();
  }


  synchronized void dotStreamFinished(NNTPDotTerminatedInputStream stream) {
    Assert.Assertion(pending_dot_stream == null ||
                     pending_dot_stream == stream);
    if (stream == pending_dot_stream)
      pending_dot_stream = null;
  }


  NewsMessage parseOverviewLine(NewsFolder folder,
                                byte line[], int start, int length) {

    // We can't use StringTokenizer for this, because "a\t\tb" is supposed to
    // be interpreted as the three fields "a", "", and "b".  StringTokenizer
    // would interpret it as two fields, "a" and "b".

    // Field order is always:
    //    article subject from date id references bytes lines

    long article = 0;
    String subject;
    String from;
    String date;
    String id;
    String references;
    long bytes = 0;
    long lines = 0;

    int i = start;
    int j = i;

    if (length > 0 && line[length-1] == (byte)'\n')
      length--;
    if (length > 0 && line[length-1] == (byte)'\r')
      length--;

    while (j < length && line[j] != (byte)'\t')
      article = (article * 10) + line[j++] - (byte)'0';

    i = ++j;
    while (j < length && line[j] != (byte)'\t') j++;
    subject = new String(line, i, j-i);

    i = ++j;
    while (j < length && line[j] != (byte)'\t') j++;
    from = new String(line, i, j-i);

    i = ++j;
    while (j < length && line[j] != (byte)'\t') j++;
    date = new String(line, i, j-i);

    i = ++j;
    while (j < length && line[j] != (byte)'\t') j++;
    id = new String(line, i, j-i);

    i = ++j;
    while (j < length && line[j] != (byte)'\t') j++;
    references = new String(line, i, j-i);

    i = ++j;
    while (j < length && line[j] != (byte)'\t')
      bytes = (bytes * 10) + line[j++] - (byte)'0';

    i = ++j;
    while (j < length && line[j] != (byte)'\t')
      lines = (lines * 10) + line[j++] - (byte)'0';

    InternetHeaders h = new InternetHeaders();
    h.addHeaderLine("Subject: " + subject + "\r\n");
    h.addHeaderLine("From: " + from + "\r\n");
    h.addHeaderLine("Date: " + date + "\r\n");
    h.addHeaderLine("Message-ID: " + id + "\r\n");
    h.addHeaderLine("References: " + references + "\r\n");
    h.addHeaderLine("Content-Length: " + bytes + "\r\n");
    h.addHeaderLine("Lines: " + lines + "\r\n");

    NewsMessage m = new NewsMessage(folder, h);
    m.setStorageFolderIndex((int) article);
    return m;
  }

  Enumeration getMessages(NewsFolder folder, long from, long to)
    throws IOException {
    try {
      NNTPDotTerminatedInputStream s = (NNTPDotTerminatedInputStream)
        OVER(folder.getFullName(), from, to);
      return new XOVERMessagesEnumeration(folder, this, s);

    } catch (NNTPException e) {         // XOVER not supported; do HEAD.
      if (DEBUG)
        System.err.println("NNTP: XOVER didn't work: " + e + "; trying HEAD.");
      return new HEADEnumeration(folder, this, from, to);
    }
  }


// begin_authorize
// list
// FIGURE_NEXT_CHUNK
// profile add
// profile delete
// read group
// post
// check for message
// display newsrc
// cancel
// xpat
// search
// list pretty names
// list xactive


}


class NNTPDotTerminatedInputStream extends DotTerminatedInputStream {

  static private final boolean DEBUG = false;

  // debugging kludge: if the number printed in the stderr output doesn't
  // increase monotonically, there's a synchronization problem.
  private int debug_count = 0;

  private NNTPConnection nntp;

  NNTPDotTerminatedInputStream(NNTPConnection nntp, InputStream s) {
    super(s);
    this.nntp = nntp;
  }

  public int read(byte[] buf, int start, int length) throws IOException {
    Assert.Assertion(nntp != null);
    synchronized (nntp) {
      int i = super.read(buf, start, length);
      Assert.Assertion(i > 0 || i == -1);

      if (DEBUG && NNTPConnection.DEBUG) {
        String s = "NNTP/DOT #" + (debug_count++) + ": <== ";
        int k = (i > 50 ? 50 : i);
        for (int j = start; j < start+k; j++)
          s += (buf[j] == (byte)'\r' ? "\\r" :
                buf[j] == (byte)'\n' ? "\\n" :
                buf[j] == (byte)'\t' ? "\\t" :
                buf[j] == (byte)'\\' ? "\\\\" :
                new String(buf, j, 1));
        System.err.print(s + "\n");
      }

      if (i <= 0) {
        finished();
        Assert.Assertion(i < 0);
      }

      return i;
    }
  }

  public void close() {
    super.close();
    finished();
  }

  public void bufferUpEverything() {
    super.bufferUpEverything();
    finished();
  }

  private void finished() {
    NNTPConnection n2 = nntp;  // avoid double sync and possible deadlock
    nntp = null;
    if (n2 != null) {
      synchronized (n2) {
        n2.dotStreamFinished(this);
        if (NNTPConnection.DEBUG) {
          if (DEBUG)
            System.err.println("NNTP/DOT #" + debug_count +
                               ": dot-terminated input stream finished.");
          else
            System.err.println("NNTP: dot-terminated input stream finished.");
        }
      }
    }
  }

}



/** An enumeration object that reads XOVER data from a stream, and returns
    successive NewsMessage objects.  It streams; the enumeration will block
    when there is no data ready on the stream.
 */
class XOVERMessagesEnumeration implements Enumeration {

  static private final boolean DEBUG = false;

  // debugging kludge: if the number printed in the stderr output doesn't
  // increase monotonically, there's a synchronization problem.
  private int debug_count = 0;

  private NNTPDotTerminatedInputStream s;
  private NewsFolder folder;
  private NNTPConnection nntp;

  private int max_bytes = 10240;
  private byte buf[] = new byte[max_bytes];
  private NewsMessage next_value = null;

  XOVERMessagesEnumeration(NewsFolder folder,
                           NNTPConnection nntp,
                           NNTPDotTerminatedInputStream s) {
    this.folder = folder;
    this.nntp = nntp;
    this.s = s;
    this.next_value = read_one();
  }

  private NewsMessage read_one() {
    try {
      // Note that `s' is a DotTerminatedInputStream, which means that
      // `read' will only return chunks consisting of a single line (or
      // max_bytes, whichever is smaller.)
      int i = s.read(buf, 0, max_bytes);

      if (DEBUG && NNTPConnection.DEBUG) {
        String s = "NNTP/XOVER #" + (debug_count++) + ": <== ";
        int k = (i > 50 ? 50 : i);
        for (int j = 0; j < k; j++)
          s += (buf[j] == (byte)'\r' ? "\\r" :
                buf[j] == (byte)'\n' ? "\\n" :
                buf[j] == (byte)'\t' ? "\\t" :
                buf[j] == (byte)'\\' ? "\\\\" :
                new String(buf, j, 1));
        System.err.print(s + "\n");
      }

      if (i <= 0) {
        return null;
      } else {
        return nntp.parseOverviewLine(folder, buf, 0, i);
      }
    } catch (IOException e) {
      return null;
    }
  }

  public Object nextElement() {
    if (next_value == null) {
      throw new NoSuchElementException();
    } else {
      NewsMessage m = next_value;
      next_value = read_one();
      return m;
    }
  }

  public boolean hasMoreElements() {
    return (next_value != null);
  }

}


/** An enumeration object that sends and interprets successive HEAD commands
    to the NNTP server, and returns successive NewsMessage objects.
 */
class HEADEnumeration implements Enumeration {

  static private final boolean DEBUG = false;

  // debugging kludge: if the number printed in the stderr output doesn't
  // increase monotonically, there's a synchronization problem.
  private int debug_count = 0;

  private NewsFolder folder;
  private NNTPConnection nntp;
  private long from;
  private long to;
  private NewsMessage next_value = null;

  /* #### Possible optimization:
     GNUS used to send many HEAD requests all at once, then parse the
     responses as they came in, rather than doing a round-trip /
     synchronization between each message.  The trick here is, you have
     to be careful not to blow the network buffer (pipe) or you'll
     deadlock.  I think GNUS only sent 50 or 100 at a time, under the
     assumption that the returned data would tend to be of some size
     that made that safe.

     This probably isn't that big a deal in the case of HEAD (since almost
     nobody has servers that don't support XOVER these days) but it would
     be a huge win to be able to do this with GROUP (for listing the message
     counts at startup.)
   */

  HEADEnumeration(NewsFolder folder, NNTPConnection nntp, long from, long to) {
    this.folder = folder;
    this.nntp = nntp;
    this.from = from;
    this.to = to;
    this.next_value = read_one();
  }

  private NewsMessage read_one() {
    try {
      if (from > to) {
        return null;
      } else {
        NewsMessage m = nntp.HEAD(folder, from++);
        if (NNTPConnection.DEBUG && DEBUG) {
          System.err.println("NNTP/HEAD #" + (debug_count++) + ": " + m);
        }
        return m;
      }
    } catch (IOException e) {
      if (NNTPConnection.DEBUG && DEBUG) {
        System.err.println("NNTP/HEAD: caught I/O error: " + e);
      }
      return null;
    }
  }

  public Object nextElement() {
    if (next_value == null) {
      throw new NoSuchElementException();
    } else {
      NewsMessage m = next_value;
      next_value = read_one();
      return m;
    }
  }

  public boolean hasMoreElements() {
    return (next_value != null);
  }

}
