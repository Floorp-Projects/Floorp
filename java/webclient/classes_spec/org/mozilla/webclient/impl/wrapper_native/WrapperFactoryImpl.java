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
import org.mozilla.webclient.impl.BrowserControlImpl;
import org.mozilla.webclient.Bookmarks;
import org.mozilla.webclient.Preferences;
import org.mozilla.webclient.ProfileManager;
import org.mozilla.webclient.ImplObject;

import org.mozilla.webclient.impl.WrapperFactory;
import org.mozilla.webclient.impl.Service;

import java.util.HashMap;
import java.util.Map;

public class WrapperFactoryImpl extends Object implements WrapperFactory
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

protected String platformCanvasClassName = null;
protected boolean initialized = false;
protected int nativeContext = -1;
    /**
     * <p>keys: BrowserControl instances</p>
     *
     * <p>values: NativeEventThread instances that correspond to this
     * window.</p>
     *
     */
protected Map nativeEventThreads = null;

// Relationship Instance Variables

/**
 * <p>App singleton.  WrapperFactoryImpl is the owner of this reference.
 * We maintain a reference so we can remove it at shutdown time.</p>
 */

protected Bookmarks bookmarks = null;

/**
 * <p>App singleton.  WrapperFactoryImpl is the owner of this reference.
 * We maintain a reference so we can remove it at shutdown time.</p>
 */
protected Preferences prefs = null;

/**
 * <p>App singleton.  WrapperFactoryImpl is the owner of this reference.
 * We maintain a reference so we can remove it at shutdown time.</p>
 */

protected ProfileManager profileManager = null;

//
// Constructors and Initializers    
//

public WrapperFactoryImpl()
{
    super();
    nativeEventThreads = new HashMap();
}


//
// Class methods
//

//
// Methods from webclient.WrapperFactory
//

/**

 * @param interfaceName IMPORTANT!!!! This method assumes that
 * interfaceName is one of the actual strings from the interface
 * definition for BrowserControl.  That is, this method is only called
 * from BrowserControlImpl, and is only invoked like this

<CODE><PRE>

// BrowserControlImpl code...

    if (WINDOW_CONTROL_NAME.equals(interfaceName)) {
        if (null == windowControl) {
            windowControl = 
                (WindowControl) wrapperFactory.newImpl(WINDOW_CONTROL_NAME,
                                                       this);
        }
        return windowControl;
    }

</PRE></CODE>

 * <P>
 * This is done to avoid a costly string compare.  This shortcut is
 * justified since WrapperFactoryImpl is only called from
 * BrowserControlImpl <B>and</B> the only values for interfaceName that
 * make sense are those defined in BrowserControl class variables ending
 * with _NAME.
 * </P>

 * @see org.mozilla.webclient.BrowserControl.BROWSER_CONTROL_CANVAS_NAME

 */

public Object newImpl(String interfaceName,
                      BrowserControl browserControl) throws ClassNotFoundException
{
    Object result = null;

    synchronized(this) {
        if (!nativeDoesImplement(interfaceName)) {
            throw new ClassNotFoundException("Can't instantiate " + 
                                             interfaceName + 
                                             ": not implemented.");
        }
        if (BrowserControl.WINDOW_CONTROL_NAME == interfaceName) {
            result = new WindowControlImpl(this, browserControl);
        }
        if (BrowserControl.NAVIGATION_NAME == interfaceName) {
            result = new NavigationImpl(this, browserControl);
        }
        if (BrowserControl.HISTORY_NAME == interfaceName) {
            result = new HistoryImpl(this, browserControl);
        }
        if (BrowserControl.CURRENT_PAGE_NAME == interfaceName) {
            result = new CurrentPageImpl(this, browserControl);
        }
        if (BrowserControl.EVENT_REGISTRATION_NAME == interfaceName) {
            result = new EventRegistrationImpl(this, browserControl);
        }
        if (BrowserControl.BOOKMARKS_NAME == interfaceName) {
            Assert.assert_it(null != bookmarks);
            result = bookmarks;
        }
        if (BrowserControl.PREFERENCES_NAME == interfaceName) {
            Assert.assert_it(null != prefs);
            result = prefs;
        }
        if (BrowserControl.PROFILE_MANAGER_NAME == interfaceName) {
            Assert.assert_it(null != profileManager);
            result = profileManager;
        }
    }

    return result;
}

public void initialize(String verifiedBinDirAbsolutePath) throws SecurityException, UnsatisfiedLinkError
{
    if (initialized) {
        return;
    }

    System.loadLibrary("webclient");

    try {
        nativeContext = nativeAppInitialize(verifiedBinDirAbsolutePath);
    }
    catch (Throwable e) {
        throw new UnsatisfiedLinkError(e.getMessage());
    }
    Assert.assert_it(-1 != nativeContext);

    //
    // create app singletons
    //
    profileManager = new ProfileManagerImpl(this);
    Assert.assert_it(null != profileManager);
    ((Service)profileManager).startup();

    prefs = new PreferencesImpl(this);
    Assert.assert_it(null != prefs);
    ((Service)prefs).startup();

    bookmarks = new BookmarksImpl(this);
    Assert.assert_it(null != bookmarks);
    ((Service)bookmarks).startup();

    try {
        nativeAppSetup(nativeContext);
    }
    catch (Throwable e) {
        throw new UnsatisfiedLinkError(e.getMessage());
    }

    
    initialized = true;
}

public void verifyInitialized() throws IllegalStateException
{
    if (!initialized) {
        throw new IllegalStateException("Webclient has not been initialized.");
    }
}

public void terminate() throws Exception
{
    Assert.assert_it(null != bookmarks);
    ((Service)bookmarks).shutdown();
    ((ImplObject)bookmarks).delete();
    bookmarks = null;

    Assert.assert_it(null != prefs);
    ((Service)prefs).shutdown();
    ((ImplObject)prefs).delete();
    prefs = null;

    Assert.assert_it(null != profileManager);
    ((Service)profileManager).shutdown();
    ((ImplObject)profileManager).delete();
    profileManager = null;
    
    nativeTerminate(nativeContext);
}

public int getNativeContext() {
    return nativeContext;
}

public Object getNativeEventThread(BrowserControl bc) {
    verifyInitialized();
    NativeEventThread eventThread = null;
    int nativeBrowserControl = -1;
    
    synchronized(bc) {
        // if this is the first time the nativeBrowserControl has been
        // requested for this Java BrowserControl instance:
        if (null == 
            (eventThread = (NativeEventThread) nativeEventThreads.get(bc))) {
            
            // spin up the NativeEventThread
            try {
                nativeBrowserControl = 
                    nativeCreateBrowserControl(nativeContext);
                eventThread = new NativeEventThread("EventThread-" + 
                                                    nativeBrowserControl,
                                                    nativeBrowserControl, bc);
                // IMPORTANT: the nativeEventThread initializes all the
                // native browser stuff, then sends us notify().
                eventThread.start();
                try {
                    bc.wait();
                }
                catch (Exception e) {
                    System.out.println("WindowControlImpl.createWindow: interrupted while waiting\n\t for NativeEventThread to notify(): " + e + 
                                       " " + e.getMessage());
                }
                
                nativeEventThreads.put(bc, eventThread);
            }
            catch (Exception e) {
                nativeBrowserControl = -1;
            }
        }
    }
    return eventThread;
}

public int getNativeBrowserControl(BrowserControl bc) {
    NativeEventThread eventThread = 
        (NativeEventThread) getNativeEventThread(bc);
    return eventThread.getNativeBrowserControl();
}

public void destroyNativeBrowserControl(BrowserControl bc) {
    verifyInitialized();
    NativeEventThread eventThread = null;
    int nativeBrowserControl = -1;

    synchronized(this) {
        if (null != 
            (eventThread = (NativeEventThread) nativeEventThreads.get(bc))){
            nativeBrowserControl = eventThread.getNativeBrowserControl();
            
            eventThread.delete();
            
            try {
                nativeDestroyBrowserControl(nativeContext, 
                                            nativeBrowserControl);
            }
            catch (Exception e) {
                System.out.println("Exception while destroying nativeBrowserControl:"
                                   + e.getMessage());
            }
        }
    }
}    

//
// helper methods
// 

/**
 *
 * <p>Called from {@link #loadNativeLibrary}.  This method simply
 * figures out the proper name for the class that is the
 * BrowserControlCanvas.  Sets the {@link #platformCanvasClassName} ivar
 * as a side effect.</p>
 */

protected String getPlatformCanvasClassName()
{
    if (null != platformCanvasClassName) {
        return platformCanvasClassName;
    }

    String osName = System.getProperty("os.name");
    
    if (null != osName) {
        if (-1 != osName.indexOf("indows")) {
            platformCanvasClassName = "org.mozilla.webclient.impl.wrapper_native.win32.Win32BrowserControlCanvas";
        }
        else {
            platformCanvasClassName = "org.mozilla.webclient.impl.wrapper_native.gtk.GtkBrowserControlCanvas";
        }
    }
    
    return platformCanvasClassName;
}

// 
// Native methods
//

/**

 * <p>This is called only once, at the very beginning of program execution.
 * This method allows the native code to handle ONE TIME initialization
 * tasks.  Per-window initialization tasks are handled in
 * {@link #nativeCreateBrowserControl}. </p>

 * <p>For mozilla, this means initializing XPCOM.</p>

 */

native int nativeAppInitialize (String verifiedBinDirAbsolutePath) throws Exception;

/**
 *
 * <p>Place to put any final app setup, after the app singletons have
 * been started.</p>
 */

native void nativeAppSetup(int nativeContext) throws Exception;

/**

 * Called only once, at program termination.  Results are undefined if
 * the number of browser windows currently open is greater than zero.

 */

native void nativeTerminate(int nativeContext) throws Exception;

/**

 * This is called whenever java calls newImpl on an instance of this
 * class.  This method allows the native code to declare which of the
 * "webclient.*" interfaces it supports.  See the comments for the
 * newImpl method for the format of valid values for interfaceName.

 * @see org.mozilla.webclient.wrapper_native.WrapperFactoryImpl.newImpl

 */

native boolean nativeDoesImplement(String interfaceName);

/**

 * <p>The one and only entry point to do per-window initialization.</p>

 */
native int nativeCreateBrowserControl(int nativeContext) throws Exception;

/*

 * <p>The one and only de-allocater for the nativeBrowserControl</p>

 */
native int nativeDestroyBrowserControl(int nativeContext, 
                                       int nativeBrowserControl) throws Exception;

} // end of class WrapperFactoryImpl
