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

// BrowserControlFactory.java

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

import java.io.File;
import java.io.FileNotFoundException;

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.util.HashMap;
import java.util.Properties;


/**
 *
 *  <p><B>BrowserControlFactory</B> uses a discovery algorithm to find
 *  an implementation of {@link BrowserControlFactoryInterface}.  All of
 *  the public static methods in this class simply call through to this
 *  implemenatation instance.</p>
 *
 * <p>The discovery mechanism used is to look try to load a resource
 * called
 * <code>META-INF/services/org.mozilla.webclient.BrowserControlFactoryInterface</code>.
 * If the resource is found, interpret it as a <code>Properties</code>
 * file and read out its first line.  Interpret the first line as the
 * fully qualified class name of a class that implements {@link
 * BrowserControlFactoryInterface}.  The named class must have a public
 * no-arg constructor.</p>
 *
 *
 * @version $Id: BrowserControlFactory.java,v 1.8 2003/09/06 06:26:45 edburns%acm.org Exp $
 * 
 *
 */

public class BrowserControlFactory extends Object 
{
//
// Protected Constants
//

//
// Class Variables
//

private static BrowserControlFactoryInterface instance = null;

//
// Constructors and Initializers    
//

private BrowserControlFactory()
{
    Assert.assert_it(false, "This class shouldn't be constructed.");
}

//
// Class methods
//

public static void setAppData(String absolutePathToNativeBrowserBinDir) throws FileNotFoundException, ClassNotFoundException
{
    getInstance().setAppData(BrowserControl.BROWSER_TYPE_NATIVE, absolutePathToNativeBrowserBinDir);
}

public static void setAppData(String myBrowserType, String absolutePathToNativeBrowserBinDir) throws FileNotFoundException, ClassNotFoundException
{
    getInstance().setAppData(myBrowserType, absolutePathToNativeBrowserBinDir);
}

public static void appTerminate() throws Exception
{
    getInstance().appTerminate();
}

public static BrowserControl newBrowserControl() throws InstantiationException, IllegalAccessException, IllegalStateException
{
    BrowserControl result = null;
    result = getInstance().newBrowserControl();
    return result;
}

public static void deleteBrowserControl(BrowserControl toDelete)
{
    getInstance().deleteBrowserControl(toDelete);
}

//
// helper methods
// 

protected static BrowserControlFactoryInterface getInstance() 
{
    if (null != instance) {
        return instance;
    }

    ClassLoader classLoader = Thread.currentThread().getContextClassLoader();
    if (classLoader == null) {
        throw new RuntimeException("Context ClassLoader");
    }
    
    BufferedReader reader = null;
    InputStream stream = null;
    String 
        className = null,
        resourceName = "META-INF/services/org.mozilla.webclient.BrowserControlFactoryInterface";
    try {
        stream = classLoader.getResourceAsStream(resourceName);
        if (stream != null) {
            // Deal with systems whose native encoding is possibly
            // different from the way that the services entry was created
            try {
                reader =
                    new BufferedReader(new InputStreamReader(stream,
                                                             "UTF-8"));
            } catch (UnsupportedEncodingException e) {
                reader = new BufferedReader(new InputStreamReader(stream));
            }
            className = reader.readLine();
            reader.close();
            reader = null;
            stream = null;
        }
    } catch (IOException e) {
    } catch (SecurityException e) {
    } finally {
        if (reader != null) {
            try {
                reader.close();
            } catch (Throwable t) {
                ;
            }
            reader = null;
            stream = null;
        }
        if (stream != null) {
            try {
                stream.close();
            } catch (Throwable t) {
                ;
            }
            stream = null;
        }
    }
    if (null != className) {
        try {
            Class clazz = classLoader.loadClass(className);
            instance = (BrowserControlFactoryInterface) (clazz.newInstance());
        } catch (Exception e) {
        }
    }
    return instance;
}

} // end of class BrowserControlFactory
