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

import java.util.*;
import java.net.*;

public class ConfigUtils {

  private static String gFileSeparator = System.getProperty("file.separator");
  private static String gUserDir = System.getProperty("user.dir");

  /**
   * This is the standardized place for getting a list of URLs which
   * you should search, in order, when trying to locate a resource
   * which may be overridden by external files.  This is used, at time
   * of writing, by the Configuration system, which looks for things
   * within packages, but first looks for external files which will
   * effectively replace the package resources.
   *   aBaseClass is necessary if aLocation refers to a resource
   * (which it normally will).  In this case, we will need a class loader.
   * supplied by aBaseClass.
   * @param aLocation The resource path for locating the resource in
   *                  its default location, a package.  Use a full
   *                  name, like "/netscape/shell/imp/ShellConfig.xml".
   *                  Use a slash for the path separator character.
   * @param aBaseClass A base class using the same classloader as
   *                   aLocation.  That is, a class from the same .jar file.
   *                   Can be null if aLocation is a simple local system file.
   * @return an enumeration of URLs to try, in order
   */
  public static final Enumeration fileSearchURLs(
    String aLocation,
    Class  aBaseClass) {

    URL     url;
    int     nameIndex = aLocation.lastIndexOf('/');
    String  fileName = nameIndex >= 0 && nameIndex < aLocation.length()-1 ?
                       aLocation.substring(nameIndex+1) : aLocation;
    Vector  urls = new Vector(2,2);

    // first, try to find a file with the given name in the user directory.
    try {
      // can user.dir be off the net (not a file URL) on a Network Computer?
      urls.addElement(new URL("file", "", gUserDir + gFileSeparator + fileName));
    } catch (MalformedURLException mue) {
    }
    url = aBaseClass != null ? aBaseClass.getResource(aLocation) :
                               ClassLoader.getSystemResource(aLocation);
    if (url != null)
      urls.addElement(url);

    return urls.elements();
  }
}
