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
 *               Mark Lin <mark.lin@eng.sun.com>
 *               Ed Burns <edburns@acm.org>
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 */

package org.mozilla.webclient;

import java.io.File;
import java.io.FileNotFoundException;

/**
 * <p>Do per-app one-time initialization and shutdown, as well as being
 * the factory for per-window {@link BrowserControl} instances.  This
 * interface allows a pluggable webclient API implementation.  The
 * static methods in {@link BrowserControlFactory} call through to
 * methods on this interface.</p>
 */

public interface WebclientFactory {

    //
    // Public API
    // 

    /**
     *
     * <p>See {@link
     * BrowserControlFactory#setAppData(java.lang.String)}.</p>
     * 
     */

    public void setAppData(String absolutePathToNativeBrowserBinDir) 
	throws FileNotFoundException, ClassNotFoundException;

    public void appTerminate() throws Exception;
    
    public BrowserControl newBrowserControl() 
	throws InstantiationException, IllegalAccessException, 
	       IllegalStateException;

    /**
       
    * BrowserControlFactory.deleteBrowserControl is called with a
    * BrowserControl instance obtained from
    * BrowserControlFactory.newBrowserControl.  This method renders the
    * argument instance completely un-usable.  It should be called when the
    * BrowserControl instance is no longer needed.  This method simply
    * calls through to the non-public BrowserControlImpl.delete() method.
    
    * @see org.mozilla.webclient.ImplObject#delete
    
    */

    public void deleteBrowserControl(BrowserControl toDelete);

} 
