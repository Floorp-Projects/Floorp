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
import org.mozilla.webclient.CurrentPage;
import org.mozilla.webclient.WindowControl;
import org.mozilla.webclient.WrapperFactory;

import java.util.Properties;
import java.io.*;
import java.net.*;

import org.w3c.dom.Document;

import org.mozilla.webclient.UnimplementedException; 

import org.mozilla.dom.DOMAccessor;

public class CurrentPageImpl extends ImplObjectNative implements CurrentPage
{
//
// Protected Constants
//

//
// Class Variables
//

private static boolean domInitialized = false;

//
// Instance Variables
//

// Attribute Instance Variables


// Relationship Instance Variables

//
// Constructors and Initializers    
//

public CurrentPageImpl(WrapperFactory yourFactory, 
                       BrowserControl yourBrowserControl)
{
    super(yourFactory, yourBrowserControl);
    // force the class to be loaded, thus loading the JNI library
    if (!domInitialized) {
        DOMAccessor.initialize();
    }
}

//
// Class methods
//

//
// General Methods
//

//
// Methods from CurrentPage    
//

public void copyCurrentSelectionToSystemClipboard()
{
    myFactory.throwExceptionIfNotInitialized();
    Assert.assert(-1 != nativeWebShell);

    synchronized(myBrowserControl) {
        nativeCopyCurrentSelectionToSystemClipboard(nativeWebShell);
    }
}
            
public void findInPage(String stringToFind, boolean forward, boolean matchCase)
{
    myFactory.throwExceptionIfNotInitialized();

    synchronized(myBrowserControl) {
        nativeFindInPage(nativeWebShell, stringToFind, forward, matchCase);
    }
}
            
public void findNextInPage(boolean forward)
{
    myFactory.throwExceptionIfNotInitialized();
    
    synchronized(myBrowserControl) {
        nativeFindNextInPage(nativeWebShell, forward);
    }
}
            
public String getCurrentURL()
{
    String result = null;
    myFactory.throwExceptionIfNotInitialized();
    
    synchronized(myBrowserControl) {
        result = nativeGetCurrentURL(nativeWebShell);
    }
    return result;
}
            
public Document getDOM()
{
    Document result = nativeGetDOM(nativeWebShell);
    return result;
}

public Properties getPageInfo()
{
  Properties result = null;

  /* synchronized(myBrowserControl) {
        result = nativeGetPageInfo(nativeWebShell);
    }
    return result;
    */
  
  throw new UnimplementedException("\nUnimplementedException -----\n API Function CurrentPage::getPageInfo has not yet been implemented.\n");
}  
  

            
public String getSource()
{
    String result = null;
    myFactory.throwExceptionIfNotInitialized();

    /*    synchronized(myBrowserControl) {
        result = nativeGetSource();
    }
    */
    throw new UnimplementedException("\nUnimplementedException -----\n API Function CurrentPage::getSource has not yet been implemented.\n");

    //    return result;
}
 
public byte [] getSourceBytes(boolean viewMode)
{
    byte [] result = null;
    myFactory.throwExceptionIfNotInitialized();
    
        synchronized(myBrowserControl) {
        result = nativeGetSourceBytes(nativeWebShell, viewMode);
    }
    

    //throw new UnimplementedException("\nUnimplementedException -----\n API Function CurrentPage::getSourceBytes has not yet been implemented.\n Will be available after Webclient M3 Release\n");
    
    // PENDING (Ashu) - This should work - but it does not get anything from URl
    // and hangs up from time to time. Have to Debug. In M15, other native solution
    // will also be available using DocShell::viewMode

    /*
    String HTMLContent = null;
    String URL = getCurrentURL();
    System.out.println("\nThe Current URL is -- " + URL);
    try {
      Socket s = new Socket("sunweb.central",80);
      BufferedReader in = new BufferedReader(new InputStreamReader(s.getInputStream()));
      boolean more = true;
      while (more)
	{
	  String line = in.readLine();
	  if (line == null) more = false;
	  else
	    {
	      HTMLContent = HTMLContent + line;
	      System.out.println(line);
	    }
	}
    }
      catch (Throwable e)
	{
	  System.out.println("Error occurred while establishing connection -- \n ERROR - " + e);
	}
    */
    return result;
    
}
            
public void resetFind()
{
    myFactory.throwExceptionIfNotInitialized();
    
    synchronized(myBrowserControl) {
        nativeResetFind(nativeWebShell);
    }
}
            
public void selectAll()
{
    myFactory.throwExceptionIfNotInitialized();
    
    synchronized(myBrowserControl) {
        nativeSelectAll(nativeWebShell);
    }
}

// 
// Native methods
//

native public void nativeCopyCurrentSelectionToSystemClipboard(int webShellPtr);
            
native public void nativeFindInPage(int webShellPtr, String stringToFind, boolean forward, boolean matchCase);
            
native public void nativeFindNextInPage(int webShellPtr, boolean forward);
            
native public String nativeGetCurrentURL(int webShellPtr);
            
native public Document nativeGetDOM(int webShellPtr);

// webclient.PageInfo getPageInfo();
            
native public String nativeGetSource();
 
native public byte [] nativeGetSourceBytes(int webShellPtr, boolean viewMode);
            
native public void nativeResetFind(int webShellPtr);
            
native public void nativeSelectAll(int webShellPtr);


// ----VERTIGO_TEST_START

//
// Test methods
//

public static void main(String [] args)
{
    Assert.setEnabled(true);
    Log.setApplicationName("CurrentPageImpl");
    Log.setApplicationVersion("0.0");
    Log.setApplicationVersionDate("$Id: CurrentPageImpl.java,v 1.7 2000/06/30 00:01:35 ashuk%eng.sun.com Exp $");
    
}

// ----VERTIGO_TEST_END

} // end of class CurrentPageImpl
