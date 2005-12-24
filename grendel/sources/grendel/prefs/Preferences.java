/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * XMLPreferences.java
 *
 * Created on 10 August 2005, 22:33
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is
 * Kieran Maclean.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
package grendel.prefs;

import grendel.messaging.ExceptionNotice;
import grendel.messaging.StringNotice;

import grendel.prefs.accounts.Accounts;

import grendel.prefs.addressbook.Addressbooks;

import grendel.prefs.xml.XMLPreferences;

import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;


/**
 *
 * @author hash9
 */
public final class Preferences extends XMLPreferences
{
  private static String base_path;
  private static String profile_path;
  private static File file_profiles;
  private static File file_preferences;
  private static Preferences preferences;
  private static XMLPreferences profiles = new XMLPreferences();

  /**
   * Creates a new instance of Preferences
   */
  private Preferences()
  {
    setupProfilePath();

    StringBuilder prefs_path = new StringBuilder(Preferences.profile_path);

    prefs_path.append("preferences.xml");

    file_preferences = new File(prefs_path.toString());

    try
    {
        InputStream is = getClass().getResourceAsStream("/" +
            getClass().getPackage().getName().replace('.', '/') +
            "/defaults.xml");
        if (is == null) {
            grendel.messaging.NoticeBoard.publish(new StringNotice("Unable to load default preferences!"));
        } else {
            loadDefaultsFromXML(new InputStreamReader(is));
        }
    }
    catch (Exception e)
    {
      grendel.messaging.NoticeBoard.publish(new ExceptionNotice(e));
    }

    try
    {
      loadFromXML(new FileReader(file_preferences));
    }
    catch (Exception e)
    {
      grendel.messaging.NoticeBoard.publish(new ExceptionNotice(e));
    }
  }

  public static StringBuilder getBasePath()
  {
    StringBuilder base_path = new StringBuilder();

    base_path.append(System.getProperty("user.home"));

    char[] c_a = System.getProperty("user.home").toCharArray();

    if (c_a[c_a.length - 1] != File.separatorChar)
    {
      base_path.append(File.separator);
    }

    base_path.append(".grendel");
    base_path.append(File.separator);

    Preferences.base_path = base_path.toString();

    return base_path;
  }

  public static Preferences getPreferances()
  {
    if (preferences == null)
    {
      preferences = new Preferences();
    }

    return preferences;
  }

  public static void save()
  {
    try
    {
      savePreferances();
      saveProfiles();
    }
    catch (IOException ioe)
    {
      ioe.printStackTrace();
    }
  }

  public static void savePreferances() throws IOException
  {
    preferences.storeToXML(new FileWriter(file_preferences));
  }

  public static void saveProfiles() throws IOException
  {
    profiles.storeToXML(new FileWriter(file_profiles));
  }

  public Accounts getAccounts()
  {
    XMLPreferences p = getPropertyPrefs("accounts");

    if (p instanceof Accounts)
    {
      return (Accounts) p;
    }
    else
    {
      Accounts a = new Accounts(p);
      setProperty("accounts", a);

      return a;
    }
  }

  public Addressbooks getAddressbooks()
  {
    XMLPreferences p = getPropertyPrefs("addressbooks");

    if (p instanceof Addressbooks)
    {
      return (Addressbooks) p;
    }
    else
    {
      Addressbooks a = new Addressbooks(p);
      setProperty("addressbooks", a);

      return a;
    }
  }

  public static XMLPreferences getProfiles()
  {
    setupProfilePath();
    return profiles;
  }

  public String getProfilePath()
  {
    return profile_path;
  }

  private static void createDefaultProfile()
  {
    File dir_base = new File(base_path);
    dir_base.mkdirs();
    profiles.setProperty("default_profile", "default");

    XMLPreferences profile = new XMLPreferences();
    profile.setProperty("name", "default");
    profiles.setProperty("default", profile);

    StringBuilder dir_path_profile = new StringBuilder(base_path);
    dir_path_profile.append("default");
    dir_path_profile.append(File.separator);

    File dir_profile = new File(dir_path_profile.toString());
    dir_profile.mkdirs();

    try
    {
      saveProfiles();
    }
    catch (IOException ioe)
    {;
    }
  }

  private static void setupProfilePath()
  {
    StringBuilder profile_path = new StringBuilder(getBasePath());
    StringBuilder prefs_path = new StringBuilder(profile_path);

    profile_path.append("profiles.xml");

    try
    {
      file_profiles = new File(profile_path.toString());
      profiles.loadFromXML(new FileReader(file_profiles));
    }
    catch (IOException ioe)
    {
      createDefaultProfile();
    }

    String default_profile = profiles.getPropertyString("default_profile");
    XMLPreferences profile = profiles.getPropertyPrefs(default_profile);

    prefs_path.append(profile.getPropertyString("name"));
    prefs_path.append(File.separator);

    Preferences.profile_path = prefs_path.toString();
  }
}
