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
 * Contributor(s):  Ashutosh Kulkarni <ashuk@eng.sun.com>
 *                  Ed Burns <edburns@acm.org>
 *
 */



package org.mozilla.webclient.wrapper_nonnative;

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

import org.mozilla.webclient.BrowserControl;
import org.mozilla.webclient.WrapperFactory;

public class WrapperFactoryImpl extends WrapperFactory
{
//
// Protected Constants
//

    final String [] implementedInterfaces = 
        {
            BrowserControl.NAVIGATION_NAME,
            BrowserControl.HISTORY_NAME,
            BrowserControl.CURRENT_PAGE_NAME,
            BrowserControl.BROWSER_TYPE_NAME
        };


boolean initialized = false;

public WrapperFactoryImpl()
{
    super();
}

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
        if (!doesImplement(interfaceName)) {
            throw new ClassNotFoundException("Can't instantiate " + 
                                             interfaceName + 
                                             ": not implemented.");
        }
        System.out.println("non-native classes do implement " +
                           interfaceName);
        if (BrowserControl.NAVIGATION_NAME == interfaceName) {
            result = new NavigationImpl(this, browserControl);
            return result;
        }
        if (BrowserControl.HISTORY_NAME == interfaceName) {
            result = new HistoryImpl(this, browserControl);
            return result;
        }
        if (BrowserControl.CURRENT_PAGE_NAME == interfaceName) {
            result = new CurrentPageImpl(this, browserControl);
            return result;
        }
        if (BrowserControl.BROWSER_TYPE_NAME == interfaceName) {
            result = new BrowserTypeImpl(this, browserControl);
            return result;
        }
    }

    return result;
}



private boolean doesImplement(String interfaceName) 
{
    boolean foundInterface = false;
    for (int i=0; i<implementedInterfaces.length; i++) {
        if (interfaceName.equals(implementedInterfaces[i])) {
            foundInterface = true;
        }
    }
    return foundInterface;
}


public boolean hasBeenInitialized()
{
System.out.println("\n\n\n +++++ You should Never have reached Here - in hasBeenInitialized +++++ \n\n\n");
return false;
}


public void initialize(String verifiedBinDirAbsolutePath) throws Exception
{
}

public void terminate() throws Exception
{
System.out.println("\n\n\n +++++ You should Never have reached Here - in terminate +++++ \n\n\n");
}

} // end of class WrapperFactoryImpl
