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
 */

package calypso.util;

import java.io.*;
import java.util.*;

/**
 * Utility class for generating temporary file names.
 *
 * @author Kipp E.B. Hickman
 */
public class TempFile {
  static private boolean gHaveSetFinalizersOnExit;
  static private int gNextID;
  static private Random gGenerator = new Random();

  String fTempFileName;

  /**
   * Create a new temporary file name that the calling
   * thread can use that is unique and has the given extension.
   */
  // XXX need security access check
  static public synchronized TempFile TempName(String aExtension) {
    /* XXX seems to cause jdk1.1.3b to hang...
    if (!gHaveSetFinalizersOnExit) {
      // Make sure temp files get removed on exit
      System.runFinalizersOnExit(true);
      gHaveSetFinalizersOnExit = true;
    }*/

    // Fix up temp dir to have a trailing separator
    String tmpDir = GetTempDir();
    int tmpDirLen = tmpDir.length();
    if ((tmpDirLen > 0) &&
        (tmpDir.charAt(tmpDirLen - 1) != File.separatorChar)) {
      tmpDir = tmpDir + File.separatorChar;
      SetTempDir(tmpDir);
    }

    for (;;) {
      int value = gGenerator.nextInt() & 0xffffff;
      String name = tmpDir + "ns" + StringUtils.ToHex(value, 6) + aExtension;
      File file = new File(name);
      if (file.exists()) {
        continue;
      }
      return new TempFile(name);
    }
  }

  /**
   * Return the native path name for the system's temporary directory.
   */
  static final String propertyName = "system.tempdir";
  static public synchronized String GetTempDir() {
    Properties p = System.getProperties();
    String dir = p.getProperty(propertyName);
    if (dir == null) {
      String osname = p.getProperty("os.name");
      if (osname.startsWith("Windows")) {
        dir = "c:\\windows\temp";
        if (TryDirectory(dir)) {
          return dir;
        }
        dir = "c:\\temp";
        if (TryDirectory(dir)) {
          return dir;
        }
        dir = "\temp";
        if (TryDirectory(dir)) {
          return dir;
        }
        dir = "\tmp";
        if (TryDirectory(dir)) {
          return dir;
        }
        // Oh well, fall back to the current directory wherever that is!
        dir = ".";
        if (TryDirectory(dir)) {
          return dir;
        }
        throw new Error("whoops: no windoze tempdir!");
      } else {
        // XXX for now, assume it's unix
        dir = "/tmp";
        if (TryDirectory(dir)) {
          return dir;
        }
        throw new Error("whoops: no unix tempdir!");
      }
    }
    return dir;
  }

  static private boolean TryDirectory(String aDir) {
    File f = new File(aDir);
    if (f.exists() && f.isDirectory()) {
      SetTempDir(aDir);
      return true;
    }
    return false;
  }

  /**
   * Set the native path name for the system's temporary directory.
   */
  // XXX need security access check
  static public synchronized void SetTempDir(String aDir) {
    Properties p = System.getProperties();
    p.put(propertyName, aDir);
  }

  protected TempFile(String aName) {
    fTempFileName = aName;
  }

  public OutputStream create() throws IOException {
    return new BufferedOutputStream(new FileOutputStream(fTempFileName));
  }

  public OutputStream append() throws IOException {
    return new BufferedOutputStream(new FileOutputStream(fTempFileName, true));
  }

  public String getName() {
    return fTempFileName;
  }

  public void delete() {
    if (fTempFileName != null) {
      new File(fTempFileName).delete();
      fTempFileName = null;
    }
  }

  protected void finalize() {
    delete();
  }
}
