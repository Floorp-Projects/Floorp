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

import org.mozilla.util.ParameterCheck;

import org.mozilla.webclient.WebclientEventListener;

/**

 * This class allows the custom app to have one instance that implements
 * multiple Webclient event listener types. <P>

 * This is simply a "struct" type class that encapsulates a listener
 * instance with its class name.  This is necessary because the class
 * name is lost when we deal with the listener as a
 * WebclientEventListener, and not a WebclientEventListener subclass. <P>

 * @see org.mozilla.webclient.wrapper_native.NativeEventThread#addListener
 * @see org.mozilla.webclient.wrapper_native.NativeEventThread#removeListener

 * note that we use "package protection" here since this class isn't
 * public.

 */

class WCEventListenerWrapper extends Object
{
//
// Instance Variables
//

// Attribute Instance Variables

// Relationship Instance Variables

/**

 * These are public for easy access

 */

public WebclientEventListener listener;
public String listenerClassName;

//
// Constructors and Initializers    
//

public WCEventListenerWrapper(WebclientEventListener yourListener, 
                              String yourListenerClassName)
{
    ParameterCheck.nonNull(yourListener);
    ParameterCheck.nonNull(yourListenerClassName);
    listener = yourListener;
    listenerClassName = yourListenerClassName;
}


} // end of class WCEventListenerWrapper
