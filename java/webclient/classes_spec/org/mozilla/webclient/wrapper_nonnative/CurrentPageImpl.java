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

import org.mozilla.webclient.BrowserControl;
import org.mozilla.webclient.BrowserType;
import org.mozilla.webclient.CurrentPage2;
import org.mozilla.webclient.Selection;
import org.mozilla.webclient.WrapperFactory;

import java.util.Properties;
import java.io.*;
import java.net.*;

import org.w3c.dom.Document;
import org.w3c.dom.Node;

import ice.storm.*;

import org.mozilla.webclient.UnimplementedException; 

public class CurrentPageImpl extends ImplObjectNonnative implements CurrentPage
{

private StormBase ICEbase;
private Viewport viewPort;

public CurrentPageImpl(WrapperFactory yourFactory, 
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


public void copyCurrentSelectionToSystemClipboard()
{
    throw new UnimplementedException("\nUnimplementedException -----\n API Function CurrentPage::copyCurrentSelectionToSystemClipboard has not yet been implemented.\n");
}

public Selection getSelection() {
    throw new UnimplementedException("\nUnimplementedException -----\n API Function CurrentPage::getSelection has not yet been implemented.\n");
}           

public void highlightSelection(Selection selection) {
    throw new UnimplementedException("\nUnimplementedException -----\n API Function CurrentPage::highlightSelection has not yet been implemented.\n");
}

public void clearAllSelections() {
    throw new UnimplementedException("\nUnimplementedException -----\n API Function CurrentPage::clearAllSelections has not yet been implemented.\n");
}

 public void findInPage(String stringToFind, boolean forward, boolean matchCase)
 {
     throw new UnimplementedException("\nUnimplementedException -----\n API Function CurrentPage::findInPage has not yet been implemented.\n");
public void findInPage(String stringToFind, boolean forward, boolean matchCase)
{
    throw new UnimplementedException("\nUnimplementedException -----\n API Function CurrentPage::findInPage has not yet been implemented.\n");
}
            
public void findNextInPage()
{
    throw new UnimplementedException("\nUnimplementedException -----\n API Function CurrentPage::findNextInPage has not yet been implemented.\n");
}
            
public String getCurrentURL()
{
    String result = null;
    synchronized(myBrowserControl) {
        //getCurrentURL
	result = ICEbase.getHistoryManager().getCurrentLocation(viewPort.getId());
    }
    return result;
}
            
public Document getDOM()
{
    throw new UnimplementedException("\nUnimplementedException -----\n API Function CurrentPage::getDOM has not yet been implemented.\n");
}

public Properties getPageInfo()
{
    throw new UnimplementedException("\nUnimplementedException -----\n API Function CurrentPage::getPageInfo has not yet been implemented.\n");
}  
  

            
public String getSource()
{
    myFactory.throwExceptionIfNotInitialized();
    String HTMLContent = new String();
    String currURL = getCurrentURL();
    System.out.println("\nThe Current URL is -- " + currURL);
    try {
        URL aURL = new URL(currURL);
        URLConnection connection = aURL.openConnection();
        connection.connect();
        BufferedReader in = new BufferedReader(new InputStreamReader(connection.getInputStream()));
        boolean more = true;
        while (more)
            {
                String line = in.readLine();
                if (line == null) more = false;
                else
                    {
                        HTMLContent = HTMLContent + line;
                    }
            }
    }
    catch (Throwable e)
        {
            System.out.println("Error occurred while establishing connection -- \n ERROR - " + e);
        }
    
    return HTMLContent;
}
 
public byte [] getSourceBytes()
{
    byte [] result = null;
    myFactory.throwExceptionIfNotInitialized();
    
    
    String HTMLContent = new String();
    String currURL = getCurrentURL();
    System.out.println("\nThe Current URL is -- " + currURL);
    try {
        URL aURL = new URL(currURL);
        URLConnection connection = aURL.openConnection();
        connection.connect();
        BufferedReader in = new BufferedReader(new InputStreamReader(connection.getInputStream()));
        boolean more = true;
        while (more)
            {
                String line = in.readLine();
                if (line == null) more = false;
                else
                    {
                        HTMLContent = HTMLContent + line;
                    }
            }
    }
    catch (Throwable e)
 	{
        System.out.println("Error occurred while establishing connection -- \n ERROR - " + e);
 	}
    result = HTMLContent.getBytes();
    return result;
}

public void resetFind()
{
    throw new UnimplementedException("\nUnimplementedException -----\n API Function CurrentPage::resetFind has not yet been implemented.\n");
}
            
public void selectAll()
{
    throw new UnimplementedException("\nUnimplementedException -----\n API Function CurrentPage::selectAll has not yet been implemented.\n");
}

// ----VERTIGO_TEST_START

//
// Test methods
//

public static void main(String [] args)
{
    Assert.setEnabled(true);
    Log.setApplicationName("CurrentPageImpl");
    Log.setApplicationVersion("0.0");
    Log.setApplicationVersionDate("$Id: CurrentPageImpl.java,v 1.2 2003/04/09 17:42:38 edburns%acm.org Exp $");
    
}

// ----VERTIGO_TEST_END

} // end of class CurrentPageImpl
