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
 * Contributor(s): Edwin Woudt <edwin@woudt.nl>
 */

package calypso.util;

import java.io.File;
import java.util.Properties;

/** This is an interface to get things from the Preferences database.
    @see PreferencesFactory */

public interface Preferences {
  /** Given a name of a preference, return its value as a String.  If it's not
      defined, return the given default. */
  String getString(String prefname, String defaultValue);

  /** Given a name of a preference, return its value as a String. */
  String getString(String prefname);

  /** Given a name of a preference, return its value as an int.  If it's not
      defined, return the given default. */
  int getInt(String prefname, int defaultValue);

  /** Given a name of a preference, return its value as an int. */
  int getInt(String prefname);

  /** Given a name of a preference, return its value as a boolean.  If it's not
      defined, return the given default. */
  boolean getBoolean(String prefname, boolean defaultValue);

  /** Given a name of a preference, return its value as a boolean. */
  boolean getBoolean(String prefname);

  /** Given a name of a preference, return its value as an File.  If it's not
      defined, return the given default. */
  File getFile(String prefname, File defaultValue);

  /** Assign a String value to the given preference. */
  void putString(String prefname, String value);

  /** Assign an int value to the given preference. */
  void putInt(String prefname, int value);

  /** Assign a boolean value to the given preference. */
  void putBoolean(String prefname, boolean value);

  /** Return the preferences as a Properties object */
  Properties getAsProperties();
};
