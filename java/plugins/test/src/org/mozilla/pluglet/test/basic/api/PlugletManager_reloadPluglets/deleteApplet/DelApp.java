/* 
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


import java.applet.Applet;      //Applet
import java.io.*;
import java.net.URL;
import java.util.Properties;

public class DelApp extends Applet
{
  private File secondPluglet = null;
  private URL nextHTML = null;
  private String target = null;
  public void init()
  {
     String codeBase = getCodeBase().toString();
     String fileName = getParameter("PROPS_FILE_NAME");
     String htmlName = getParameter("NEXT_HTML");
     target = getParameter("NEXT_HTML_TARGET");
     if ((fileName == null)||(htmlName == null)||(target == null)) {
       printLog("PROPS_FILE_NAME or NEXT_HTML or NEXT_HTML_TARGET parameter not specified for APPLET!");
       return;
     }
     Properties pProps = new Properties();
     try {
       nextHTML = new URL(codeBase + "/" + htmlName);
       URL propsURL = new URL(codeBase + "/" + fileName);
       pProps.load(propsURL.openStream());
     } catch(Exception e) {
       printLog("Exception " + e + " ocurred during applet initialization");
       return;
     }
     String pDir = pProps.getProperty("PLUGLET_DIR",null);
     String pName = pProps.getProperty("SECOND_PLUGLET_FILE",null);
     String separator = System.getProperty("file.separator");
     if ((pDir == null)||(pName == null)) {
       printLog("PLUGLET_DIR or SECOND_PLUGLET_FILE properties not initialized");
       return;
     }
     secondPluglet = new File(pDir  + separator + pName);
  }
  public void start()
  {
    if (!secondPluglet.exists()) {
      printLog("File " + secondPluglet + " not exist, nothing to do.");
      nextURL();
      return;
    }
    if (!secondPluglet.delete()) {
      printLog("Can't delete " + secondPluglet);
      return;
    } else {
      printLog("File: " + secondPluglet + " succesfully deleted");
    }
    nextURL();
  }
  private void nextURL()
  {
    printLog("Going to the next URL: " + nextHTML);
    getAppletContext().showDocument(nextHTML,target);
  }
  private void printLog(String msg) 
  {
     org.mozilla.pluglet.test.basic.api.PlugletManager_reloadPluglets_2.msgVect.addElement(msg);  
  }
}




