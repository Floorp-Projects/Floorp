/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil -*-
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
 */

package calypso.util;

import java.net.*;
import java.util.*;
import java.util.zip.*;
import java.io.*;

// Simple class loader:
// - no signatures
// - no security
// - no mayscript

/**
 * A URL based class loader. This class knows how to load classes from
 * a given base URL
 */
public class URLClassLoader extends ClassLoader {
  private Hashtable classes;
  private URL codeBaseURL;
  private URL archiveURL;
  private ZipFile fZipFile;
  private TempFile fTempFile;

  static final boolean XXXnoisy = false;

  /**
   * Create a new URL based class loader. aBaseURL specifies the URL
   * to load classes relative to. If aArchiveURL is not null then the
   * archive will be searched first (if it fails then the load will be
   * attempted from aBaseURL).
   */
  public URLClassLoader(URL aBaseURL, URL aArchiveURL) {
    codeBaseURL = aBaseURL;
    archiveURL = aArchiveURL;
    classes = new Hashtable();
    if (XXXnoisy) {
      System.out.println("####### Creating TagClassLoader");
      System.out.println("### CodeBaseURL=" + codeBaseURL);
      System.out.println("### ArchiveURL=" + archiveURL);
    }
  }

  /**
   * Load a class from a URL. This does the actual work of loading the
   * class and then defining it.
   */
  private Class loadClass(String name, URL url, String pathname)
    throws IOException
  {
    byte[] data = readURL(url);
    if (XXXnoisy) {
      System.out.println("# loadClass: url="+url+" bytes="+data.length);
    }
    // XXX update for netscape security model
    return defineClass(name, data, 0, data.length);
  }

  private Class loadClassFromArchive(String name, URL url, String pathname)
    throws IOException
  {
    if (fZipFile == null) {
      // First time in we copy over the archive URL
      fTempFile = TempFile.TempName(".zip");
      copyURL(fTempFile.create(), url);
      if (XXXnoisy) {
        System.out.println("### Copying zip file: tempName=" +
                           fTempFile.getName() + " url=" + url);
      }
      fZipFile = new ZipFile(fTempFile.getName());
    }

    // Dig up the class's bits using the zip loader
    byte[] data = readZipFile(pathname);
    if (XXXnoisy) {
      System.out.println("# loadClass: url="+url+"(" +
                         pathname + ") bytes=" + data.length);
    }
    // XXX update for netscape security model
    return defineClass(name, data, 0, data.length);
  }

  /**
   * Load a class from this class loader. This method is used by applets
   * that want to explicitly load a class.
   */
  public Class loadClass(String name) throws ClassNotFoundException {
    return loadClass(name, true);
  }

  /**
   * Load and resolve a class. This method is called by the java runtime
   * to get a class that another class needs (e.g. a superclass).
   */
  protected Class loadClass(String name, boolean resolve)
    throws ClassNotFoundException
  {
    Class cl = (Class)classes.get(name);
    if (cl == null) {
      // XXX: We should call a Security.checksPackageAccess() native
      // method, and pass name as arg
      SecurityManager security = System.getSecurityManager();
      if (security != null) {
        int i = name.lastIndexOf('.');
        if (i >= 0) {
          security.checkPackageAccess(name.substring(0, i));
        }
      }
      try {
        return findSystemClass(name);
      } catch (ClassNotFoundException e) {
      }
      cl = findClass(name);
    }
    if (cl == null) {
      throw new ClassNotFoundException(name);
    }
    if (resolve) {
      resolveClass(cl);
    }
    return cl;
  }

  /**
   * This method finds a class. The returned class
   * may be unresolved. This method has to be synchronized
   * to avoid two threads loading the same class at the same time.
   * Must be called with the actual class name.
   */
  protected synchronized Class findClass(String name)
    throws ClassNotFoundException
  {
    Class cl = (Class)classes.get(name);
    if (cl != null) {
      return cl;
    }

    // XXX: We should call a Security.checksPackageDefinition() native
    // method, and pass name as arg
    SecurityManager security = System.getSecurityManager();
    if (security != null) {
      int i = name.lastIndexOf('.');
      if (i >= 0) {
        security.checkPackageDefinition(name.substring(0, i));
      }
    }

    boolean system_class = true;
    String cname = name.replace('.', '/') + ".class";
    if (cl == null) {
      URL url;
      try {
        url = new URL(codeBaseURL, cname);
      } catch (MalformedURLException e) {
        throw new ClassNotFoundException(name);
      }
      if (XXXnoisy) {
        System.err.println("# Fetching " + url);
      }

      try {
        if (archiveURL != null) {
          cl = loadClassFromArchive(name, archiveURL, cname);
          // XXX try regular file if archive load fails?
        } else {
          cl = loadClass(name, url, cname);
        }
        system_class = false;
      } catch (IOException e) {
        if (XXXnoisy) {
          System.out.println("# IOException loading class: " + e);
          e.printStackTrace();
        }
        throw new ClassNotFoundException(name);
      }
    }
    if (!name.equals(cl.getName())) {
      Class oldcl = cl;
      cl = null;
      throw new ClassFormatError(name + " != " + oldcl.getName());
    }
    if (system_class == false) {
      // setPrincipalAry(cl, cname);
    }
    classes.put(name, cl);
    return cl;
  }

  // collapse the i/o loops between this code and readZipFile

  private byte[] readURL(URL url) throws IOException {
    byte[] data;
    InputStream in = null;
    try {
      URLConnection c = url.openConnection();
      c.setAllowUserInteraction(false);
      in = c.getInputStream();

      int len = c.getContentLength();
      data = new byte[(len == -1) ? 4096 : len];
      int total = 0, n;

      while ((n = in.read(data, total, data.length - total)) >= 0) {
        if ((total += n) == data.length) {
          if (len < 0) {
            byte newdata[] = new byte[total * 2];
            System.arraycopy(data, 0, newdata, 0, total);
            data = newdata;
          } else {
            break;
          }
        }
      }
    } finally {
      if (in != null) {
        in.close();
      }
    }
    return data;
  }

  /**
   * Load a given file from the underlying zip file named "aName". Return
   * an array of bytes which contain the decompressed contents of the
   * file in the zip file.
   */
  private byte[] readZipFile(String aName)
    throws IOException
  {
    ZipEntry entry = fZipFile.getEntry(aName);
    if (entry == null) {
      throw new IOException("file not found: " + aName);
    }

    InputStream in = null;
    int len = (int) entry.getSize();
    byte[] data = new byte[(len == -1) ? 4096 : len];
    try {
      in = fZipFile.getInputStream(entry);
      int total = 0, n;
      while ((n = in.read(data, total, data.length - total)) >= 0) {
        if ((total += n) == data.length) {
          if (len < 0) {
            byte newdata[] = new byte[total * 2];
            System.arraycopy(data, 0, newdata, 0, total);
            data = newdata;
          } else {
            break;
          }
        }
      }
    } finally {
      if (in != null) {
        in.close();
      }
    }
    return data;
  }

  private void copyURL(OutputStream aOut, URL aURLSource)
    throws IOException
  {
    InputStream in = null;
    try {
      byte[] inputBuf = new byte[4096];

      // open the destination file for writing
      //SecurityManager.enablePrivilege("UniversalFileAccess");
      //SecurityManager.enablePrivilege("UniversalConnect");

      URLConnection c = archiveURL.openConnection();
      c.setAllowUserInteraction(false);
      in = c.getInputStream();

      // Read all the bytes from the url into the temp file
      Thread thread = Thread.currentThread();
      int total = 0, n;
      while (((n = in.read(inputBuf)) >= 0) && !thread.isInterrupted()) {
        aOut.write(inputBuf, 0, n);
        total += n;
      }
      if (thread.isInterrupted()) {
        throw new IOException("interrupted: " + this);
      }
      if (XXXnoisy) {
        System.out.println("# Copying archive: url=" + archiveURL +
                           " tempFile=" + fTempFile.getName() +
                           " copiedBytes=" + total);
      }
    } finally {
      if (in != null) {
        in.close();
      }
      if (aOut != null) {
        aOut.close();
      }
    }
    // SecurityManager.revertPrivilege();
  }

  public URL getCodeBaseURL() {
    return codeBaseURL;
  }

  protected void finalize() {
    if (fZipFile != null) {
      try {
        // First close the zip file
        fZipFile.close();
      } catch (Exception e) {
      }
      fZipFile = null;
    }
    if (fTempFile != null) {
      try {
        // Then remove the temporary file
        fTempFile.delete();
      } finally {
        fTempFile = null;
      }
    }
  }
}
