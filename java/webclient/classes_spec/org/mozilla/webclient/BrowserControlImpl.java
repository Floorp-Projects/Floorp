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

package org.mozilla.webclient;

// BrowserControlImpl

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;
import org.mozilla.util.Utilities;

class BrowserControlImpl extends Object implements BrowserControl
{
//
// Protected Constants
//

//
// Class Variables
//

/**

 * Strings for the names of the interfaces supported by QI.

 */

/**

 * the factory for creating instances of webclient interfaces

 */

private static WrapperFactory wrapperFactory = null;


//
// Instance Variables
//

// Attribute Instance Variables

// Relationship Instance Variables


private BrowserControlCanvas myCanvas = null;
private CurrentPage currentPage = null;
private EventRegistration eventRegistration = null;
private WindowControl windowControl = null;
private Navigation navigation = null;
private History history = null;
private static Bookmarks bookmarks = null;

//
// Constructors and Initializers    
//

public BrowserControlImpl(BrowserControlCanvas yourCanvas)
{
    super();
    ParameterCheck.nonNull(yourCanvas);
    myCanvas = yourCanvas;
}

/**

 * Called from BrowserControlFactory.deleteBrowserControl() <P>

 * The order of deletion of objects is very important! <P>

 * We don't allow deletion if the Canvas is showing. <P>

 */ 

void delete()
{
    Assert.assert(null != myCanvas);
    
    // Make sure we're not showing.
    if (myCanvas.isShowing()) {
        throw new IllegalStateException("Can't delete a browser control while its canvas is showing");
    }
    myCanvas = null;

    if (null != eventRegistration) {
        ((ImplObject)eventRegistration).delete();
        eventRegistration = null;
    }

    if (null != navigation) {
        ((ImplObject)navigation).delete();
        navigation = null;
    }

    if (null != history) {
        ((ImplObject)history).delete();
        history = null;
    }

    if (null != currentPage) {
        ((ImplObject)currentPage).delete();
        currentPage = null;
    }

    Assert.assert(null != windowControl);
    ((ImplObject)windowControl).delete();
    windowControl = null;

    // since bookmarks is static, we must not deallocate it here.  That
    // is done in the static method appTerminate

}

//
// Class methods
//

static void appInitialize(String verifiedBinDirAbsolutePath) throws Exception 
{
    ParameterCheck.nonNull(verifiedBinDirAbsolutePath);
    if (null == wrapperFactory) {
        wrapperFactory = createWrapperFactory();
    }
    wrapperFactory.initialize(verifiedBinDirAbsolutePath);
}

static void appTerminate() throws Exception
{
    Assert.assert(null != wrapperFactory);

    if (null != bookmarks) {
        ((ImplObject)bookmarks).delete();
        bookmarks = null;
    }

    wrapperFactory.terminate();
}


/**

 * Creates an instance of the appropriate wrapper factory, based on the
 * current package name and a parameterized string value.  This method
 * uses reflection to obtain the right WrapperFactory concrete subclass,
 * either native or nonnative.  This method should only be called once.

 */

private static WrapperFactory createWrapperFactory() throws ClassNotFoundException
{
    String wrapperFactoryClassName = null;
    Class wrapperFactoryClass = null;
    WrapperFactory result = null;

    // get the current package name
    if (null == (wrapperFactoryClassName = 
                 Utilities.getPackageName(BrowserControlImpl.class))){
        throw new ClassNotFoundException("Can't determine current class name");
    }


    // PENDING(edburns): when we have a java implementation, this is
    // where you'll replace the string "native" with either "native" or
    // "nonnative".

    String PARAMETERIZED_VALUE = "native";

    
    // tack on the appropriate stuff
    wrapperFactoryClassName = wrapperFactoryClassName + "wrapper_" + 
        PARAMETERIZED_VALUE + "." + WrapperFactory.IMPL_NAME;

    wrapperFactoryClass = Class.forName(wrapperFactoryClassName);

    try {
        result = (WrapperFactory) wrapperFactoryClass.newInstance();
    }
    catch (Exception e) {
        throw new ClassNotFoundException(e.getMessage());
    }
    return result;
}

//
// General Methods
//

//
// Methods from BrowserControl
//

public Object queryInterface(String interfaceName) throws ClassNotFoundException
{
    ParameterCheck.nonNull(interfaceName);
    
    Assert.assert(null != wrapperFactory);
    wrapperFactory.throwExceptionIfNotInitialized();

    // At some point, it has to come down to hard coded string names,
    // right?  Well, that point is here.  There is an extensibility
    // mechanism that doesn't require recompilation.  See the last bit
    // of this big-ass if statement.
    
    if (NAVIGATION_NAME.equals(interfaceName)) {
        if (null == navigation) {
            navigation = (Navigation) wrapperFactory.newImpl(NAVIGATION_NAME,
                                                             this);
        }
        return navigation;
    }
    if (HISTORY_NAME.equals(interfaceName)) {
        if (null == history) {
            history = (History) wrapperFactory.newImpl(HISTORY_NAME, this);
        }
        return history;
    }
    if (BROWSER_CONTROL_CANVAS_NAME.equals(interfaceName)) {
        Assert.assert(null != myCanvas);
        return myCanvas;
    }
    if (CURRENT_PAGE_NAME.equals(interfaceName)) {
        if (null == currentPage) {
            currentPage = 
                (CurrentPage) wrapperFactory.newImpl(CURRENT_PAGE_NAME,
                                                     this);
        }
        return currentPage;
    }
    if (WINDOW_CONTROL_NAME.equals(interfaceName)) {
        if (null == windowControl) {
            windowControl = 
                (WindowControl) wrapperFactory.newImpl(WINDOW_CONTROL_NAME,
                                                       this);
        }
        return windowControl;
    }
    if (EVENT_REGISTRATION_NAME.equals(interfaceName)) {
        if (null == eventRegistration) {
            eventRegistration = (EventRegistration) 
                wrapperFactory.newImpl(EVENT_REGISTRATION_NAME,
                                       this);
        }
        return eventRegistration;
    }
    if (BOOKMARKS_NAME.equals(interfaceName)) {
        if (null == bookmarks) {
            bookmarks = (Bookmarks) 
                wrapperFactory.newImpl(BOOKMARKS_NAME, this);
        }
        return bookmarks;
    }
    // extensibility mechanism: just see if wrapperFactory can make one!
    return wrapperFactory.newImpl(interfaceName, this);
}

// ----VERTIGO_TEST_START

//
// Test methods
//

public static void main(String [] args)
{
    Assert.setEnabled(true);
    Log.setApplicationName("BrowserControlImpl");
    Log.setApplicationVersion("0.0");
    Log.setApplicationVersionDate("$Id: BrowserControlImpl.java,v 1.3 2000/07/22 02:48:23 edburns%acm.org Exp $");
    
}

// ----VERTIGO_TEST_END

} // end of class BrowserControlImpl


