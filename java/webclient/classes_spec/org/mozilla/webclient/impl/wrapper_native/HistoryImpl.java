/* 
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
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());

    NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable() {
	    public Object run() {
		nativeBack(getNativeBrowserControl());
		return null;
	    }
	});
}
            
public boolean canBack()
{
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());

    Boolean result = (Boolean)
	NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable() {
		public Object run() {
		    boolean canBack = nativeCanBack(getNativeBrowserControl());
		    return new Boolean(canBack);
		}
	    });
    
    return result.booleanValue();
}
            
public HistoryEntry [] getBackList()
{
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());
    HistoryEntry [] result = null;
    
    /* synchronized(getBrowserControl()) {
        result = nativeGetBackList(getNativeBrowserControl());
    }
    return result;
    */
    
    throw new UnimplementedException("\nUnimplementedException -----\n API Function History::getBackList has not yet been implemented.\n");
}
            
public void clearHistory()
{
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());
    
    /* synchronized(getBrowserControl()) {
        nativeClearHistory(getNativeBrowserControl());
    }
    */
    
    throw new UnimplementedException("\nUnimplementedException -----\n API Function History::clearHistory has not yet been implemented.\n");
}
            
public void forward()
{
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());

    NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable() {
	    public Object run() {
		nativeForward(getNativeBrowserControl());
		return null;
	    }
	});
}
            
public boolean canForward()
{
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());

    Boolean result = (Boolean)
	NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable() {
		public Object run() {
		    boolean canForward = nativeCanForward(getNativeBrowserControl());
		    return new Boolean(canForward);
		}
	    });
    
    return result.booleanValue();
}

            

            
public HistoryEntry [] getForwardList()
{
    HistoryEntry [] result = null;
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());
    
    /* synchronized(getBrowserControl()) {
        result = nativeGetForwardList(getNativeBrowserControl());
    }
    return result;
    */

    throw new UnimplementedException("\nUnimplementedException -----\n API Function History::getForwardList has not yet been implemented.\n");
}
            
public HistoryEntry [] getHistory()
{
    HistoryEntry [] result = null;
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());
    
    /* synchronized(getBrowserControl()) {
        result = nativeGetHistory(getNativeBrowserControl());
    }
    return result;
    */

    throw new UnimplementedException("\nUnimplementedException -----\n API Function History::getHistory has not yet been implemented.\n");
}
            
public HistoryEntry getHistoryEntry(int historyIndex)
{
    ParameterCheck.noLessThan(historyIndex, 0);
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());
    HistoryEntry result = null;
    
    /* synchronized(getBrowserControl()) {
        result = nativeGetHistoryEntry(getNativeBrowserControl(), historyIndex);
    }
    return result;
    */

    throw new UnimplementedException("\nUnimplementedException -----\n API Function History::getHistoryEntry has not yet been implemented.\n");
}
            
public int getCurrentHistoryIndex()
{
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());
    int result = -1;

    synchronized(getBrowserControl()) {
        result = nativeGetCurrentHistoryIndex(getNativeBrowserControl());
    }
    return result;
}

public void setCurrentHistoryIndex(int historyIndex)
{
    ParameterCheck.noLessThan(historyIndex, 0);
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());
    
    synchronized(getBrowserControl()) {
	nativeSetCurrentHistoryIndex(getNativeBrowserControl(), historyIndex);
    }
}

public int getHistoryLength()
{
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());
    int result = -1;

    synchronized(getBrowserControl()) {
        result = nativeGetHistoryLength(getNativeBrowserControl());
    }
    return result;
}

public String getURLForIndex(int historyIndex)
{
    ParameterCheck.noLessThan(historyIndex, 0);
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());
    String result = null;
    
    synchronized(getBrowserControl()) {
        result = nativeGetURLForIndex(getNativeBrowserControl(), historyIndex);
    }
    return result;
}

// 
// Native methods
//

public native void nativeBack(int webShellPtr);

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

} // end of class HistoryImpl
