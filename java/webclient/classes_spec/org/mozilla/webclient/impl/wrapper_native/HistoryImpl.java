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
 * Contributor(s):  Ed Burns <edburns@acm.org>
 */

package org.mozilla.webclient.impl.wrapper_native;

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

import org.mozilla.webclient.BrowserControl;
import org.mozilla.webclient.History;
import org.mozilla.webclient.HistoryEntry;
import org.mozilla.webclient.WindowControl;
import org.mozilla.webclient.impl.WrapperFactory;

import org.mozilla.webclient.UnimplementedException; 

public class HistoryImpl extends ImplObjectNative implements History
{
//
// Protected Constants
//

//
// Class Variables
//

//
// Instance Variables
//

// Attribute Instance Variables

// Relationship Instance Variables

//
// Constructors and Initializers    
//

public HistoryImpl(WrapperFactory yourFactory, 
                       BrowserControl yourBrowserControl)
{
    super(yourFactory, yourBrowserControl);
}

//
// Class methods
//

//
// General Methods
//

//
// Methods from History    
//

public void back()
{
    myFactory.verifyInitialized();
    Assert.assert_it(-1 != nativeWebShell);

    synchronized(myBrowserControl) {
        nativeBack(nativeWebShell);
    }
}
            
public boolean canBack()
{
    myFactory.verifyInitialized();
    Assert.assert_it(-1 != nativeWebShell);
    boolean result = false;

    synchronized(myBrowserControl) {
        result = nativeCanBack(nativeWebShell);
    }
    return result;
}
            
public HistoryEntry [] getBackList()
{
    myFactory.verifyInitialized();
    Assert.assert_it(-1 != nativeWebShell);
    HistoryEntry [] result = null;
    
    /* synchronized(myBrowserControl) {
        result = nativeGetBackList(nativeWebShell);
    }
    return result;
    */
    
    throw new UnimplementedException("\nUnimplementedException -----\n API Function History::getBackList has not yet been implemented.\n");
}
            
public void clearHistory()
{
    myFactory.verifyInitialized();
    Assert.assert_it(-1 != nativeWebShell);
    
    /* synchronized(myBrowserControl) {
        nativeClearHistory(nativeWebShell);
    }
    */
    
    throw new UnimplementedException("\nUnimplementedException -----\n API Function History::clearHistory has not yet been implemented.\n");
}
            

            
public void forward()
{
    myFactory.verifyInitialized();
    Assert.assert_it(-1 != nativeWebShell);

    synchronized(myBrowserControl) {
        nativeForward(nativeWebShell);
    }
}
 
public boolean canForward()
{
    myFactory.verifyInitialized();
    Assert.assert_it(-1 != nativeWebShell);
    boolean result = false;

    synchronized(myBrowserControl) {
        result = nativeCanForward(nativeWebShell);
    }
    return result;
}

public HistoryEntry [] getForwardList()
{
    HistoryEntry [] result = null;
    myFactory.verifyInitialized();
    Assert.assert_it(-1 != nativeWebShell);
    
    /* synchronized(myBrowserControl) {
        result = nativeGetForwardList(nativeWebShell);
    }
    return result;
    */

    throw new UnimplementedException("\nUnimplementedException -----\n API Function History::getForwardList has not yet been implemented.\n");
}
            
public HistoryEntry [] getHistory()
{
    HistoryEntry [] result = null;
    myFactory.verifyInitialized();
    Assert.assert_it(-1 != nativeWebShell);
    
    /* synchronized(myBrowserControl) {
        result = nativeGetHistory(nativeWebShell);
    }
    return result;
    */

    throw new UnimplementedException("\nUnimplementedException -----\n API Function History::getHistory has not yet been implemented.\n");
}
            
public HistoryEntry getHistoryEntry(int historyIndex)
{
    ParameterCheck.noLessThan(historyIndex, 0);
    myFactory.verifyInitialized();
    Assert.assert_it(-1 != nativeWebShell);
    HistoryEntry result = null;
    
    /* synchronized(myBrowserControl) {
        result = nativeGetHistoryEntry(nativeWebShell, historyIndex);
    }
    return result;
    */

    throw new UnimplementedException("\nUnimplementedException -----\n API Function History::getHistoryEntry has not yet been implemented.\n");
}
            
public int getCurrentHistoryIndex()
{
    myFactory.verifyInitialized();
    Assert.assert_it(-1 != nativeWebShell);
    int result = -1;

    synchronized(myBrowserControl) {
        result = nativeGetCurrentHistoryIndex(nativeWebShell);
    }
    return result;
}

public void setCurrentHistoryIndex(int historyIndex)
{
    ParameterCheck.noLessThan(historyIndex, 0);
    myFactory.verifyInitialized();
    Assert.assert_it(-1 != nativeWebShell);
    
    synchronized(myBrowserControl) {
	nativeSetCurrentHistoryIndex(nativeWebShell, historyIndex);
    }
}

public int getHistoryLength()
{
    myFactory.verifyInitialized();
    Assert.assert_it(-1 != nativeWebShell);
    int result = -1;

    synchronized(myBrowserControl) {
        result = nativeGetHistoryLength(nativeWebShell);
    }
    return result;
}

public String getURLForIndex(int historyIndex)
{
    ParameterCheck.noLessThan(historyIndex, 0);
    myFactory.verifyInitialized();
    Assert.assert_it(-1 != nativeWebShell);
    String result = null;
    
    synchronized(myBrowserControl) {
        result = nativeGetURLForIndex(nativeWebShell, historyIndex);
    }
    return result;
}

// 
// Native methods
//

public native void nativeBack(int nativeWebShell);

public native boolean nativeCanBack(int webShellPtr);

public native HistoryEntry [] nativeGetBackList(int webShellPtr);

public native void nativeClearHistory(int webShellPtr);

public native void nativeForward(int webShellPtr);

public native boolean nativeCanForward(int webShellPtr);

public native HistoryEntry [] nativeGetForwardList(int webShellPtr);

public native HistoryEntry [] nativeGetHistory(int webShellPtr);

public native HistoryEntry nativeGetHistoryEntry(int webShellPtr, int historyIndex);

public native int nativeGetCurrentHistoryIndex(int webShellPtr);

public native void nativeSetCurrentHistoryIndex(int webShellPtr, int historyIndex);

public native int nativeGetHistoryLength(int webShellPtr);

public native String nativeGetURLForIndex(int webShellPtr, int historyIndex);

// ----VERTIGO_TEST_START

//
// Test methods
//

public static void main(String [] args)
{
    Assert.setEnabled(true);
    Log.setApplicationName("HistoryImpl");
    Log.setApplicationVersion("0.0");
    Log.setApplicationVersionDate("$Id: HistoryImpl.java,v 1.1 2003/09/28 06:29:06 edburns%acm.org Exp $");
    
}

// ----VERTIGO_TEST_END

} // end of class HistoryImpl
