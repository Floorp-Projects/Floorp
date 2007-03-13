/* -*- Mode: java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *               Mark Lin <mark.lin@eng.sun.com>
 *               Ed Burns <edburns@acm.org>
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 */

package org.mozilla.webclient.impl;

import org.mozilla.webclient.BrowserControl;

/**
 * <p>Defines the interface for the real implementation of the Webclient
 * API.  The implementation of this interface must be a singleton and
 * must have the same lifetime as {@link WebclientFactory}.</p>
 *
 * <p>This interface is intended to be the anchor for all
 * per-application state.</p>
 *
 */

public interface WrapperFactory {
    
    public static String IMPL_NAME = "WrapperFactoryImpl";

    public BrowserControl newBrowserControl() throws InstantiationException, IllegalAccessException, IllegalStateException;
    public void deleteBrowserControl(BrowserControl toDelete);
    
    public Object newImpl(String interfaceName, 
                          BrowserControl browserControl) throws ClassNotFoundException;
    
    public String getProfile();
    public void setProfile(String profileName);

    /**
     *
     * <p>This method must not be called by the developer.  
     *
     * <p>Cause the native library to be loaded, if necessary.  This method
     * must be called from the native event thread.</p>
     *
     * <p>POSTCONDITION: Native library for webclient has been loaded.</p>
     *
     * @return implmentation specific native int.
     *
     */
    public int loadNativeLibrariesIfNecessary();

    /**
     *
     * <p>Cause the native library to be loaded, if necessary.</p>
     *
     * <p>Cause the underlying browser embedding API to be ready to
     * use.</p>
     *
     * <p>POSTCONDITION: The API is ready to accomodate the creation of
     * a {@link BrowserControl} instance.</p>
     *
     */

    public void initialize(String verifiedBinDirAbsolutePath) throws SecurityException, UnsatisfiedLinkError;
    
    public void verifyInitialized() throws IllegalStateException;
    
    public void terminate() throws Exception;

    public int getNativeWrapperFactory();
    
    /**
     * <p>I would like this method to be on BrowserControl itself, but
     * that would mean exposing native elements on the public java API.
     * Therefore, the WrapperFactory needs to be able to return the
     * native counterpart given a java BrowserControl.</p>
     *
     */
    
    public int getNativeBrowserControl(BrowserControl bc);
} 
