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

package org.mozilla.webclient;

// ImplObject.java

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

/**

 * This is the base class for all implementations of the BrowserControl
 * interfaces.  It simply defines the common attributes for all
 * webclient implementation classes.

 */

public abstract class ImplObject extends Object
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

/**

 * My ivars are public for fast access from subclasses in the wrapper_*
 * packages.

 */

/**

 * The factory that created me.  It's true that the WrapperFactory
 * instance is stored as a static member of BrowserControlImpl, and thus
 * you might say we don't need to keep it as an ivar here.  BUT, we
 * don't want to introduce the notion of WrapperFactory to the webclient
 * user, AND we don't want to have webclient interface implementation
 * classes depend on BrowserControlImpl, SO we keep it as ivar.  That
 * is, given that we don't want to talk to myBrowserControl as anything
 * but a BrowserControl (and not a BrowserControlImpl), we would have to
 * add an accessor to the public BrowserControl interface in order to
 * avoid having this myFactory ivar.  But we can't do that, because we
 * don't want to expose the WrapperFactory concept to the webclient API
 * end user.  Got it?

 */

public WrapperFactory myFactory = null;

  
/**

 * The BrowserControl to which I'm attached, used for locking and communication.

 */

public BrowserControl myBrowserControl = null;


//
// Constructors and Initializers    
//

public ImplObject(WrapperFactory yourFactory, 
		  BrowserControl yourBrowserControl)
{
    super();
    ParameterCheck.nonNull(yourFactory);
    ParameterCheck.nonNull(yourBrowserControl);

    myFactory = yourFactory;
    myBrowserControl = yourBrowserControl;
}

/**

 * I know Java has automatic garbage collection and all, but explicitly
 * adding a delete method helps the gc algorithm out. <P>

 * Subclasses should override this and call super.delete() at the end of
 * their overridden delete() method.

 * @see org.mozilla.webclient.wrapper_native.ImplObjectNative#delete

 */

public void delete()
{
    myFactory = null;
    myBrowserControl = null;
}

} // end of class ImplObject
