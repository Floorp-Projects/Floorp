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
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 *               Ann Sunhachawee
 */

package org.mozilla.webclient.impl.wrapper_native;

// ImplObjectNative.java

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

import org.mozilla.webclient.ImplObject;
import org.mozilla.webclient.impl.WrapperFactory;
import org.mozilla.webclient.BrowserControl;

/**

 * This is the base class for all implementations of the BrowserControl
 * interfaces in the native browser wrapping code.  It simply defines
 * the common attributes for all native browser wrapping webclient
 * implementation classes.  It extends the ImplObject, which is the base
 * class for all implementations of BrowserControl interfaces in either
 * native or non-native code.

 */

public abstract class ImplObjectNative extends ImplObject
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

private WrapperFactory myFactory = null;

private int nativeWebShell = -1;

//
// Constructors and Initializers    
//

public ImplObjectNative(WrapperFactory yourFactory, 
                        BrowserControl yourBrowserControl) {
    super(yourBrowserControl);
    myFactory = yourFactory;
}

protected WrapperFactory getWrapperFactory() {
    return myFactory;
}

    /**

    * <p>We do this lazily to allow for applications that don't use any
    * per-window features.  </p>

    */

protected int getNativeBrowserControl() {
    if (-1 == nativeWebShell) {
        nativeWebShell = 
            getWrapperFactory().getNativeBrowserControl(getBrowserControl());
    }
    return nativeWebShell;
}

protected NativeEventThread getNativeEventThread() {
    return (NativeEventThread) 
        getWrapperFactory().getNativeEventThread(getBrowserControl());
}

} // end of class ImplObject
