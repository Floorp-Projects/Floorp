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

package org.mozilla.webclient.wrapper_native;

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

import org.mozilla.webclient.BrowserControl;
import org.mozilla.webclient.EventRegistration;
import org.mozilla.webclient.WindowControl;
import org.mozilla.webclient.WrapperFactory;
import org.mozilla.webclient.DocumentLoadEvent;
import org.mozilla.webclient.DocumentLoadListener;
import org.mozilla.webclient.WebclientEvent;
import org.mozilla.webclient.WebclientEventListener;

public class EventRegistrationImpl extends ImplObjectNative implements EventRegistration
{
//
// Constants
//

//
// Class Variables
//

//
// Instance Variables
//

// Attribute Instance Variables

// Relationship Instance Variables

/**

 * the Java thread for processing events, owned by WindowControlImpl

 */

private NativeEventThread nativeEventThread = null;

//
// Constructors and Initializers    
//

public EventRegistrationImpl(WrapperFactory yourFactory, 
			     BrowserControl yourBrowserControl)
{
    super(yourFactory, yourBrowserControl);

        // pull out the NativeEventThread from the WindowControl
    try {
        WindowControl windowControl = (WindowControl)
            myBrowserControl.queryInterface(BrowserControl.WINDOW_CONTROL_NAME);
        
        if (windowControl instanceof WindowControlImpl) {
            nativeEventThread = 
                ((WindowControlImpl)windowControl).getNativeEventThread();
        }
    }
    catch (Exception e) {
      System.out.println(e.getMessage());
    }
}

public void delete()
{
    // PENDING(ashuk): remove all listeners, making sure to set
    // references to null

    nativeEventThread = null;
    
    super.delete();
}

//
// Class methods
//

//
// General Methods
//

//
// Methods from EventRegistration    
//

public void addDocumentLoadListener(DocumentLoadListener listener)
{
    myFactory.throwExceptionIfNotInitialized();
    Assert.assert(-1 != nativeWebShell);
    Assert.assert(null != nativeEventThread);
    
    synchronized(myBrowserControl) {
        nativeEventThread.addListener(listener);
    }
}

public void removeDocumentLoadListener(DocumentLoadListener listener)
{
    myFactory.throwExceptionIfNotInitialized();
    Assert.assert(-1 != nativeWebShell);
    
    synchronized(myBrowserControl) {
    }
}

// ----VERTIGO_TEST_START

//
// Test methods
//

public static void main(String [] args)
{
    Assert.setEnabled(true);

    Log.setApplicationName("EventRegistrationImpl");
    Log.setApplicationVersion("0.0");
    Log.setApplicationVersionDate("$Id: EventRegistrationImpl.java,v 1.3 2000/03/13 18:42:23 edburns%acm.org Exp $");

    try {
        org.mozilla.webclient.BrowserControlFactory.setAppData(args[0]);
	org.mozilla.webclient.BrowserControl control = 
	    org.mozilla.webclient.BrowserControlFactory.newBrowserControl();
        Assert.assert(control != null);
	
	EventRegistration wc = (EventRegistration)
	    control.queryInterface(org.mozilla.webclient.BrowserControl.WINDOW_CONTROL_NAME);
	Assert.assert(wc != null);
    }
    catch (Exception e) {
	System.out.println("got exception: " + e.getMessage());
    }
}

// ----VERTIGO_TEST_END

} // end of class EventRegistrationImpl
