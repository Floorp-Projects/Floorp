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
import org.mozilla.webclient.History;
import org.mozilla.webclient.HistoryEntry;
import org.mozilla.webclient.WindowControl;
import org.mozilla.webclient.WrapperFactory;
import org.mozilla.webclient.WindowControl;

import org.mozilla.webclient.UnimplementedException; 

import ice.storm.*;
import java.beans.*;

public class HistoryImpl extends ImplObjectNonnative implements History
{

private StormBase ICEbase;
private Viewport viewPort;


public HistoryImpl(WrapperFactory yourFactory, 
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


public void back()
{
    synchronized(myBrowserControl) {
        // Here make the call to the browserControl.StormBase.HistoryManager
        //object
	ICEbase.getHistoryManager().goBack(viewPort.getId());
    }
}
            
public boolean canBack()
{
    boolean result;
    synchronized(myBrowserControl) {
        //CanBack call here
	result = ICEbase.getHistoryManager().canGoBack(viewPort.getId());
    }
    return result;
}
            
public HistoryEntry [] getBackList()
{
    throw new UnimplementedException("\nUnimplementedException -----\n API Function History::getBackList has not yet been implemented.\n");
}
            
public void clearHistory()
{
    synchronized(myBrowserControl) {
        //clearHistory here
	ICEbase.getHistoryManager().clearHistoryForViewport(viewPort);
    }
}
            

            
public void forward()
{
    synchronized(myBrowserControl) {
        //forward here
	ICEbase.getHistoryManager().goForward(viewPort.getId());
    }
}
 
public boolean canForward()
{
    boolean result;
    synchronized(myBrowserControl) {
        //canForward here
		result = ICEbase.getHistoryManager().canGoForward(viewPort.getId());
    }
    return result;
}

public HistoryEntry [] getForwardList()
{
    throw new UnimplementedException("\nUnimplementedException -----\n API Function History::getForwardList has not yet been implemented.\n");
}
            
public HistoryEntry [] getHistory()
{
    throw new UnimplementedException("\nUnimplementedException -----\n API Function History::getForwardList has not yet been implemented.\n");
}
            
public HistoryEntry getHistoryEntry(int historyIndex)
{
    throw new UnimplementedException("\nUnimplementedException -----\n API Function History::getHistoryEntry has not yet been implemented.\n");
}
            
public int getCurrentHistoryIndex()
{
    throw new UnimplementedException("\nUnimplementedException -----\n API Function History::getCurrentHistoryIndex has not yet been implemented.\n");
}

public void setCurrentHistoryIndex(int historyIndex)
{
    throw new UnimplementedException("\nUnimplementedException -----\n API Function History::setCurrentHistoryIndex has not yet been implemented.\n");
}

public int getHistoryLength()
{
    throw new UnimplementedException("\nUnimplementedException -----\n API Function History::getHistoryLength has not yet been implemented.\n");
}

public String getURLForIndex(int historyIndex)
{
    throw new UnimplementedException("\nUnimplementedException -----\n API Function History::getURLForIndex has not yet been implemented.\n");
}


// ----VERTIGO_TEST_START

//
// Test methods
//

public static void main(String [] args)
{
    Assert.setEnabled(true);
    Log.setApplicationName("HistoryImpl");
    Log.setApplicationVersion("0.0");
    Log.setApplicationVersionDate("$Id: HistoryImpl.java,v 1.1 2001/07/27 21:01:09 ashuk%eng.sun.com Exp $");
    
}

// ----VERTIGO_TEST_END

} // end of class HistoryImpl
