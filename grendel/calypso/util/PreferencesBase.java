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

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.util.Properties;
import java.util.MissingResourceException;
import java.util.Enumeration;

class PreferencesBase extends Properties implements Preferences {

  static final String gPrefsPath = System.getProperties().getProperty("user.home");
  static final String gPrefsFile = "xena.pref";

  PreferencesBase() {
    super();

    File infile = new File(new File(gPrefsPath), gPrefsFile);
    InputStream in = null;
    try {
      in = new FileInputStream(infile);
      load(in);
      in.close();
    } catch (IOException e) {
    }
  }

  void writePrefs() {
    File outfile = new File(new File(gPrefsPath), gPrefsFile);
    OutputStream out = null;
    try {
      out = new FileOutputStream(outfile);
      save(out, "Xena User Preferences.  Do not directly modify this file!");
      out.close();
    } catch (IOException e) {
    }
  }

  public void addDefaults(Properties defs) {
    if(defs == null) {
      return;
    }

    if(defaults == null) {
      defaults = defs;
      return;
    }

    for(Enumeration e = defs.keys(); e.hasMoreElements();) {
      Object key = e.nextElement();
      defaults.put(key, defs.get(key));
    }
  }

  public String getString(String name, String defaultValue) {
    return getProperty(name, defaultValue);
  }

  public String getString(String name)
  throws MissingResourceException {
    String result = getProperty(name);
    if(result == null) {
      throw new MissingResourceException(name + " is missing!", "", name);
    }
    return result;
  }

  public int getInt(String name, int defaultValue) {
    String str = getString(name, null);
    int result = defaultValue;
    if (str != null) {
      try {
        result = Integer.parseInt(str);
      } catch (NumberFormatException e) {
      }
    }
    return result;
  }

  public int getInt(String name)
  throws MissingResourceException, NumberFormatException {
    String str;
    try {
      str = getString(name);
    } catch(MissingResourceException e) {
      throw e;
    }
    int result;
    try {
        result = Integer.parseInt(str);
    } catch (NumberFormatException e) {
      throw e;
    }
    return result;
  }

  public boolean getBoolean(String name, boolean defaultValue) {
    String str = getString(name, null);
    boolean result = defaultValue;
    if (str != null) {
      result = Boolean.valueOf(str).booleanValue();
    }
    return result;
  }

  public boolean getBoolean(String name)
  throws MissingResourceException {
    String str;
    try {
      str = getString(name);
    } catch(MissingResourceException e) {
      throw e;
    }
    return Boolean.valueOf(str).booleanValue();
  }

  public File getFile(String name, File defaultValue) {
    String str = getString(name, null);
    File result = defaultValue;
    if (str != null) {
      result = new File(str);
    }
    return result;
  }

  /** Assign a String value to the given preference. */
  public void putString(String prefName, String value) {
    put(prefName, value);
    writePrefs();
  }

  /** Assign an int value to the given preference. */
  public void putInt(String prefName, int value) {
    put(prefName, (String)(""+value));
    writePrefs();
  }

  /** Assign a boolean value to the given preference. */
  public void putBoolean(String prefName, boolean value) {
    put(prefName, (String)(""+value));
    writePrefs();
  }

  /** Return the preferences as a Properties object */
  public Properties getAsProperties() {
    return this;
  }
}
