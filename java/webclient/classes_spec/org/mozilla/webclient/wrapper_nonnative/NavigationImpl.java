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
 * Contributor(s):  Ashutosh Kulkarni <ashuk@eng.sun.com>
 *                  Ed Burns <edburns@acm.org>
 *
 */



package org.mozilla.webclient.wrapper_nonnative;

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;
import org.mozilla.util.RangeException;

import org.mozilla.webclient.BrowserControl;
import org.mozilla.webclient.BrowserType;
import org.mozilla.webclient.Navigation;
import org.mozilla.webclient.WindowControl;
import org.mozilla.webclient.WrapperFactory;
import org.mozilla.webclient.Prompt;

import org.mozilla.webclient.UnimplementedException; 

import java.io.InputStream;
import java.util.Properties;

import ice.storm.*;
import java.beans.*;

public class NavigationImpl extends ImplObjectNonnative implements Navigation
{
private StormBase ICEbase;
private Viewport viewPort;

public NavigationImpl(WrapperFactory yourFactory, 
		      BrowserControl yourBrowserControl)
{
    super(yourFactory, yourBrowserControl);
    BrowserType browserType = null;
    try {
        browserType = (BrowserType) 
	    yourBrowserControl.queryInterface(BrowserControl.BROWSER_TYPE_NAME);
    }
    catch (Exception e) {
	System.out.println(e.toString());
	System.out.println("\n\n Something went wrong when getting the Interfaces \n");
    }
    ICEbase = (StormBase) browserType.getStormBase();
    viewPort = (Viewport) browserType.getViewPort();
}


public void loadURL(String absoluteURL)
{
    synchronized(myBrowserControl) {
        // Here make the call to the browserControl.StormBase
        //object
	ICEbase.renderContent(absoluteURL, null, viewPort.getName());
    }
}

public void loadFromStream(InputStream stream, String uri,
                           String contentType, int contentLength,
                           Properties loadInfo)
{
    //cant do anything here
    throw new UnimplementedException("\nUnimplementedException -----\n API Function Navigation::loadFromStream has not yet been implemented.\n");
}


public void refresh(long loadFlags)
{
    synchronized(myBrowserControl) {
        // Reload method call here
	String url = ICEbase.getHistoryManager().getCurrentLocation(viewPort.getId());
	ICEbase.renderContent(url, null, viewPort.getName());
    }
}

public void stop()
{ 
    synchronized(myBrowserControl) {
        // Stop call here
	ICEbase.stopLoading(viewPort.getName());
    }
}

public void setPrompt(Prompt yourPrompt)
{
    //Nothing here
    throw new UnimplementedException("\nUnimplementedException -----\n API Function Navigation::setPrompt has not yet been implemented.\n");	
}


// ----VERTIGO_TEST_START

//
// Test methods
//

public static void main(String [] args)
{
    Assert.setEnabled(true);

    Log.setApplicationName("NavigationImpl");
    Log.setApplicationVersion("0.0");
    Log.setApplicationVersionDate("$Id: NavigationImpl.java,v 1.1 2001/07/27 21:01:12 ashuk%eng.sun.com Exp $");

    try {
        org.mozilla.webclient.BrowserControlFactory.setAppData("nonnative", args[0]);
	org.mozilla.webclient.BrowserControl control = 
	    org.mozilla.webclient.BrowserControlFactory.newBrowserControl();
        Assert.assert_it(control != null);
	
	Navigation wc = (Navigation)
	    control.queryInterface(org.mozilla.webclient.BrowserControl.WINDOW_CONTROL_NAME);
	Assert.assert_it(wc != null);
    }
    catch (Exception e) {
	System.out.println("got exception: " + e.getMessage());
    }
}

// ----VERTIGO_TEST_END

} // end of class NavigationImpl
