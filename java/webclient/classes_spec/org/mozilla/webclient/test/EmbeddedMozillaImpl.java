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

package org.mozilla.webclient.test;

/*
 * EmbeddedMozillaImpl.java
 */

import org.mozilla.webclient.*;
import org.mozilla.util.Assert;

/**
 *

 * This is a test application for using the BrowserControl.

 *
 * @version $Id: EmbeddedMozillaImpl.java,v 1.3 2003/04/24 05:55:09 kyle.yuan%sun.com Exp $
 *
 * @see org.mozilla.webclient.BrowserControlFactory

 */

public class EmbeddedMozillaImpl implements EmbeddedMozilla
{

public static String binDir;
public static String url;
public static int count = 0;
public EMWindow[] EMWindow_Arr;

public static void printUsage()
{
    System.out.println("usage: java org.mozilla.webclient.test.EmbeddedMozillaImpl <path> [url]");
    System.out.println("       <path> is the absolute path to the native browser bin directory, ");
    System.out.println("       including the bin.");
}

public EmbeddedMozillaImpl()
{
    CreateEMWindow(url, NewWindowListener.CHROME_ALL);
}

public void CreateEMWindow(String newurl, long chromeFlags)
{
    System.out.println("Creating new EmbeddedMozillaImpl window");
    EMWindow aEMWindow ;
    aEMWindow = new EMWindow("EmbeddedMozilla#" + (int)(count+1),
                                 binDir, newurl, count, this, chromeFlags);
    count++;
}

public void DestroyEMWindow(int winNumber) 
{
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
    String urlArg =(2 == arg.length) ? arg[1] : "http://www.mozilla.org/projects/blackwood/webclient/";

    // set class vars used in EmbeddedMozillaImpl ctor
    binDir = arg[0];
    url = urlArg;

    EmbeddedMozillaImpl em = new EmbeddedMozillaImpl();
}

}

// EOF
