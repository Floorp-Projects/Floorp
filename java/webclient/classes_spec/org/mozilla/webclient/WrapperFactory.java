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

// WrapperFactory

/**

 * Webclient is intended to be able to wrap both java based and native
 * web browsers.  These two classes of browsers require different
 * wrapping styles.  This is accomodated by requiring each different
 * wrapping style to provide a subclass of this abstract base class.
 * This subclass implementation must be called WrapperFactory.IMPL_NAME
 * and must reside in a package with a name starting with "wrapper-",
 * which must be a sub-package of the package in which this class
 * resides.  For example, for native browsers, the package is
 * "wrapper-native". <P>


 * The subclass implementation MUST HAVE a public no-arg constructor.

 */

public abstract class WrapperFactory extends Object
{
//
// Protected Constants
//

//
// Class Variables
//

public static String IMPL_NAME = "WrapperFactoryImpl";

public WrapperFactory()
{
    super();
}

//
// General Methods
//

public abstract Object newImpl(String interfaceName, 
                               BrowserControl browserControl) throws ClassNotFoundException;

public abstract void initialize(String verifiedBinDirAbsolutePath) throws Exception;

public abstract void terminate() throws Exception;

public abstract boolean hasBeenInitialized();

public void throwExceptionIfNotInitialized() throws IllegalStateException

{ 
    if (!hasBeenInitialized()) {
        throw new IllegalStateException("webclient has not been properly initialized");
    }

}

} // end of class WrapperFactory
