/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Kirk Baker <kbaker@eb.com>
 *               Ian Wilkinson <iw@ennoble.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 */

import org.mozilla.webclient.*;
import org.mozilla.webclient.test.EMWindow;
import org.mozilla.webclient.test.EmbeddedMozilla;
import org.mozilla.util.Assert;

import java.util.Calendar;
import java.util.Date;

/**
 *

 * This is a test application for using the BrowserControl.

 *
 * @version $Id: WCRandom.java,v 1.2 2001/07/19 19:02:16 edburns%acm.org Exp $
 * 
 * @see	org.mozilla.webclient.BrowserControlFactory

 */

public class WCRandom implements Runnable, EmbeddedMozilla
{

public static String binDir;
public static String url;
public static int count = 0;
public EMWindow[] EMWindow_Arr;

private Navigation navigation;

public static void printUsage()
{
    System.out.println("usage: java org.mozilla.webclient.test.WCRandom <path> [url]");
    System.out.println("       <path> is the absolute path to the native browser bin directory, ");
    System.out.println("       including the bin.");
}
	
public WCRandom() 
{
  CreateEMWindow();
}
 
public void CreateEMWindow()
{
  System.out.println("Creating new WCRandom window");
  EMWindow aEMWindow ;
  aEMWindow = new EMWindow("EmbeddedMozila#" + (int)(count+1),
                                 binDir, url, count, this);
  BrowserControl b = aEMWindow.getBrowserControl();
  try {
    navigation = (Navigation) b.queryInterface(BrowserControl.NAVIGATION_NAME);
  }
  catch (Exception e) {
    System.out.println("Can't get Navigation from BrowserControl");
  }
  Thread t = new Thread(this, "WCRandom#" + (int)(count+1));
  t.start();
  count++;
}

public void DestroyEMWindow(int winNumber) {
  count--;
  if (count == 0) {
    System.out.println("closing application");
    try {
        BrowserControlFactory.appTerminate();
    }
    catch(Exception e) {
        System.out.println("got Exception on exit: " + e.getMessage());
     }
    System.exit(0);
  }
}

public static void main(String [] arg)
{
    if (1 > arg.length) {
        printUsage();
        System.exit(-1);
    }
    String urlArg =(2 == arg.length) ? arg[1] : "http://random.yahoo.com/bin/ryl";

    // set class vars used in WCRandom ctor 
    binDir = arg[0];
    url = urlArg;

    WCRandom em = new WCRandom();
}

//
// Methods from Runnable
//

public void run()
{
    Calendar calendar;
    Date date;
    for (;;) {
        try {
            Thread.sleep(10000);
        }
        catch (Exception e) {
        }
        calendar = Calendar.getInstance();
        date = calendar.getTime();
        System.out.println("Loading " + url + " at " + date.toString());
        navigation.stop();
        navigation.loadURL(url);
        
    }
}


}

// EOF
