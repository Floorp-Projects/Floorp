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

 * The BrowserControl to which I'm attached, used for locking and communication.

 */

private BrowserControl myBrowserControl = null;


//
// Constructors and Initializers    
//

public ImplObject(BrowserControl yourBrowserControl)
{
    super();

    myBrowserControl = yourBrowserControl;
}

public BrowserControl getBrowserControl() {
    return myBrowserControl;
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
    myBrowserControl = null;
}

} // end of class ImplObject
