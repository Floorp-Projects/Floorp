/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil -*-
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
 * Created: Terry Weissman <terry@netscape.com>,  3 Dec 1997.
 */

package grendel;


import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.StringWriter;
import java.io.PrintWriter;

import java.text.DateFormat;
import java.util.Date;
import java.util.Properties;

import javax.mail.Authenticator;
import javax.mail.PasswordAuthentication;
import javax.mail.Session;

import selftest.SelfTestRoot;

/**
 * This is the base grendel SelfTest.  Running this classes main() will
 * execute the main() of all the SelfTest classes within the Grendel
 * project.
 *
 * <p>
 *
 * This class also contains useful stuff for all of the Grendel SelfTest
 * classes to use.  It is expected that all of those classes will inherit
 * from this one.
 */

public class SelfTest extends SelfTestRoot {
  /** The Properties instance that all the javamail stuff will use.  We try not
    to use calypso.util.Preferences during SelfTest, because there's no real
    way to control the values to be found there.   Typically, your SelfTest
    code will stuff values into this Properties database so that the Store
    you're using will pull those values out. */
  static protected Properties props;

  /** A stupid authenticator that we use to stuff in name/password info into
    our tests. */
  static private StupidAuthenticator authenticator;

  /** The javax.mail.Session object.  This is created for you by startTests. */
  static protected Session session;

  /** The directory where you can store temporary stuff.  If you want to use
    this, be sure to call makePlayDir() at the beginning of your test; that
    will ensure that the directory exists and is empty. */
  static protected File playdir = new File("selftestdir");


  /** Run all the grendel selftests.  Every package within grendel ought to
    add itself here. */
  public static void main(String args[]) {
    grendel.storage.SelfTest.main(args);
  }


  /** Initialize things.  If a subclass overrides this method, it must call
   this superclass's method (via super.startTests())
   The framework requires args to be passed down to SelfTestRoot.startTests.*/
  public void startTests(String args[]) {
    super.startTests(args);

    // ###HACKHACKHACK  Remove me when javamail fixes their stuff.
    java.io.File mailcapfile = new java.io.File("mailcap");
    if (!mailcapfile.exists()) {
      try {
        (new java.io.RandomAccessFile(mailcapfile, "rw")).close();
        writeMessage(null, "setup", "*** Created empty mailcap file in current");
        writeMessage(null, "setup", "*** directory (to work around buggy javamail");
        writeMessage(null, "setup", "*** software from JavaSoft).");
      } catch (java.io.IOException e) {
        writeMessage(null, "setup", "*** Couldn't create empty mailcap file: " + e);
        writeMessage(null, "setup", "*** Immanent crash is likely due to buggy");
        writeMessage(null, "setup", "*** javamail software from JavaSoft.");
      }
    }


    if (session == null) {
      props = new Properties();
      authenticator = new StupidAuthenticator();
      session = Session.getDefaultInstance(props, authenticator);
    }
  }


  /** Clean up at the end.  If a subclass overrides this method, it must call
   this superclass's method (via super.endTests()) */

  public void endTests() {
    cleanPlayDirectory();
    super.endTests();
  }

  /** Stuff in the name and password we want to be used for the next test. */
  public void setUserAndPassword(String user, String password) {
    authenticator.set(user, password);
  }

  static private boolean firsttime = true;

  /** Creates an empty directory for your test to put stuff into.  If a
    previously running test made one, it gets blown away.
    <p>
    The very first time this is called, we make sure that the directory doesn't
    already exist.  We want to make sure not to blow away something that was
    already sitting on disk that doesn't belong to us. */
  public void makePlayDir() {
    if (firsttime) {
      if (playdir.exists()) {
        throw new
          Error("A directory or file named selftestdir already exists. " +
                "It must be moved or deleted before SelfTest can be run.");
      }
    }
    firsttime = false;
    cleanPlayDirectory();
    if (!playdir.mkdirs()) {
      throw new
        Error("Couldn't create a directory named selftestdir, so " +
              "SelfTest can't be run.");
    }
  }

  /** Recursively cleans out the contents of the given directory.  Potentially
    very dangerous! */
  public void cleanDirectory(File dir) {
    String [] list = dir.list();
    for (int i=0 ; i<list.length ; i++) {
      File f = new File(dir, list[i]);
      if (f.isDirectory()) {
        cleanDirectory(f);
      }
      f.delete();
    }
  }

  /** Blows away the play directory.  Since endTests() calls this, there
    generally won't be any reason for anyone else to. */
  public void cleanPlayDirectory() {
    if (playdir.exists()) {
      cleanDirectory(playdir);
      playdir.delete();
    }
  }

  /** Given a throwable, return as a string the stack trace from it. */
  public String getStackTrace(Throwable t) {
    StringWriter s = new StringWriter();
    PrintWriter p = new PrintWriter(s, true);
    t.printStackTrace(p);
    return s.toString();
  }

  static byte copybuf[] = new byte[4096];


  /** Takes a file from the jar file and stores it into the play directory.
    @param resname  The name of the resource to grab from the jar file.  This
    name is interpreted relative to the package that your SelfTest subclass
    is in.
    @param filename The name of the file to create.  This name is interpreted
    relative to the playdir. */
  public void installFile(String resname, String filename) {
    try {
      InputStream in = getClass().getResourceAsStream(resname);
      if (in == null) {
        throw new Error("Can't open resource as stream: " + resname);
      }
      FileOutputStream out = new FileOutputStream(new File(playdir, filename));
      int length;
      while ((length = in.read(copybuf)) > 0) {
        out.write(copybuf, 0, length);
      }
      in.close();
      out.close();
    } catch (IOException e) {
      throw new Error("IOException " + e + " while installing file " + filename);
    }
  }


  /** Given a long that represents a time, returns a String representation of
    it suitable for putting in a log message. */
  public String prettyTime(long time) {
    return
      DateFormat.getDateTimeInstance(DateFormat.FULL,
                                     DateFormat.FULL).format(new Date(time)) +
      " (" + time + ")";
  }


  /** Report a bug that we already know about and that has an outstanding bug
    report sitting in the bug database. */

  public void writeKnownBug(Object o, String methodName, int bugnum,
                            String message)
  {
    // Should this be a warning or fatal?  I dunno!
    writeWarning(o, methodName, "Known bug " + bugnum + ": " + message);
  }
}


class StupidAuthenticator extends Authenticator {
  String user;
  String password;

  StupidAuthenticator() {}

  void set(String u, String p) {
    user = u;
    password = p;
  }

  protected PasswordAuthentication getPasswordAuthentication() {
    return new PasswordAuthentication(user, password);
  }
}
