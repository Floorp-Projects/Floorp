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
 * Created: Terry Weissman <terry@netscape.com>,  2 Sep 1997.
 *
 * Contributors: Edwin Woudt <edwin@woudt.nl>
 */

package grendel.util;

import calypso.util.ByteBuf;

import java.io.File;

/**
  This class contains various constants that are used in various places that
  we don't want to recompute all over the place.  Mostly, these are system-
  dependent things. */

public class Constants {
  /** The string used to separate lines in files on this system. */
  public final static String LINEBREAK =
  System.getProperties().getProperty("line.separator");

  /** The string used to separate lines in files on this system, stored as
    a ByteBuf. */
  public final static ByteBuf BYTEBUFLINEBREAK = new ByteBuf();
  static {
    BYTEBUFLINEBREAK.append(LINEBREAK);
  }
  private static boolean unix = false; // Need to use these silly three temp
  private static boolean mac = false;  // variables just so that I can declare
  private static boolean windows = false; // ISUNIX, etc, to be "final".
  private static boolean os2 = false;
  static {
    String osname = System.getProperties().getProperty("os.name");
    if (osname.startsWith("Windows") ||
        osname.startsWith("Win32") ||
        osname.startsWith("Win16") ||
        osname.startsWith("16-bit Windows")) {
      windows = true;
    } else if (osname.startsWith("Mac OS")) {
      mac = true;
    // (edwin) Moved from grendel.Main
    // ### According to talisman this string needs to be verified
    } else if (osname.startsWith("OS/2")) {
      os2 = true;
    } else {
      unix = true;
    }
  }

  /** Whether this is a Unix machine. */
  public final static boolean ISUNIX = unix;

  /** Whether this is a Macintosh machine. */
  public final static boolean ISMAC = mac;

  /** Whether this is a Windows machine. */
  public final static boolean ISWINDOWS = windows;

  /** Whether this is a OS/2 machine. */
  public final static boolean ISOS2 = os2;

/** File representing the user's home directory. */
  public final static File HOMEDIR = ISUNIX ?
  new File(System.getProperties().getProperty("user.home")+File.separator+ ".grendel") :
  new File(System.getProperties().getProperty("user.dir"));
 
};
