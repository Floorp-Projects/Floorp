/* 
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
 *               Brian Satterfield <bsatterf@atl.lmco.com>
 *               Anthony Sizer <sizera@yahoo.com>
 */

package org.mozilla.webclient.impl.wrapper_native;

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;
import org.mozilla.util.RangeException;

import org.mozilla.webclient.BrowserControl;
import org.mozilla.webclient.Navigation;
import org.mozilla.webclient.Navigation2;
import org.mozilla.webclient.WindowControl;
import org.mozilla.webclient.impl.WrapperFactory;
import org.mozilla.webclient.Prompt;

import java.io.InputStream;
import java.util.Properties;

public class NavigationImpl extends ImplObjectNative implements Navigation2
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

//
// Constructors and Initializers    
//

public NavigationImpl(WrapperFactory yourFactory, 
                      BrowserControl yourBrowserControl) {
    super(yourFactory, yourBrowserControl);
}

//
// Class methods
//

//
// General Methods
//

//
// Methods from Navigation    
//

public void loadURL(String absoluteURL)
{
    ParameterCheck.nonNull(absoluteURL);
    getWrapperFactory().verifyInitialized();
    final int bc = getNativeBrowserControl();
    final String url = new String(absoluteURL);
    Assert.assert_it(-1 != bc);
    
    NativeEventThread.instance.pushRunnable(new Runnable() {
	    public void run() {
		NavigationImpl.this.nativeLoadURL(bc, url);
	    }
	});
}

    public void loadFromStream(InputStream stream, String uri,
			       String contentType, int contentLength,
			       Properties loadInfo) {
	ParameterCheck.nonNull(stream);
	ParameterCheck.nonNull(uri);
	ParameterCheck.nonNull(contentType);
	if (contentLength < -1 || contentLength == 0) {
	    throw new RangeException("contentLength value " + contentLength +
				     " is out of range.  It is should be either -1 or greater than 0.");
	}
	
	final InputStream finalStream = stream;
	final String finalUri = uri;
	final String finalContentType = contentType;
	final int finalContentLength = contentLength;
	final Properties finalLoadInfo = loadInfo;
	
	NativeEventThread.instance.pushRunnable(new Runnable() {
		public void run() {
		    nativeLoadFromStream(NavigationImpl.this.getNativeBrowserControl(), 
					 finalStream, finalUri, 
					 finalContentType, 
					 finalContentLength, finalLoadInfo);
		}
	    });
    }

public void refresh(long loadFlags)
{
    ParameterCheck.noLessThan(loadFlags, 0);
    getWrapperFactory().verifyInitialized();

    final long finalLoadFlags = loadFlags;
    Assert.assert_it(-1 != getNativeBrowserControl());
    
    NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable() {
	    public Object run() {
		nativeRefresh(NavigationImpl.this.getNativeBrowserControl(), 
			      finalLoadFlags);
		return null;
	    }
	});
}

public void stop()
{
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());
    
    NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable() {
	    public Object run() {
		nativeStop(getNativeBrowserControl());
		return null;
	    }
	});
    
}

public void setPrompt(Prompt yourPrompt)
{
    ParameterCheck.nonNull(yourPrompt);
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());
    
    synchronized(getBrowserControl()) {
        nativeSetPrompt(getNativeBrowserControl(), yourPrompt);
    }

}

//
// Methods from Navigation2
//

public void post(String  argUrl,
                 String  argTarget, 
                 String  argPostData,
                 String  argPostHeaders) 
{
    ParameterCheck.nonNull(argUrl);
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());

    final String url = new String(argUrl);
    final String target = argTarget;
    final String postData = argPostData;
    final String postHeaders = argPostHeaders;
    final int postDataLength;
    final int postHeadersLength;

    // PENDING(edburns): make it so the url doesn't have to be absolute.

    // PENDING(edburns): check postData and postHeaders for correct
    // "\r\n" as specified in Navigation.java

    if (postData != null){
	postDataLength = postData.length();
    }
    else {
	postDataLength = 0;
    }
    
    if (postHeaders != null){
	postHeadersLength = postHeaders.length();
    }
    else {
	postHeadersLength = 0;
    }

    NativeEventThread.instance.pushRunnable(new Runnable() {
	    public void run() {
		nativePost(NavigationImpl.this.getNativeBrowserControl(), 
			   url, 
			   target,
			   postDataLength, 
			   postData, 
			   postHeadersLength, 
			   postHeaders);
	    }
	});
}

// 
// Native methods
//

public native void nativeLoadURL(int webShellPtr, String absoluteURL);

public native void nativeLoadFromStream(int webShellPtr, InputStream stream,
                                        String uri, 
                                        String contentType, 
                                        int contentLength,
                                        Properties loadInfo);

public native void nativePost(int     webShellPtr, 
                              String  absoluteUrl,
                              String  target, 
                              int     postDataLength, 
                              String  postData, 
                              int     PostHeadersLength, 
                              String  postHeaders);


public native void nativeRefresh(int webShellPtr, long loadFlags);

public native void nativeStop(int webShellPtr);

public native void nativeSetPrompt(int webShellPtr, Prompt yourPrompt);

// ----VERTIGO_TEST_START

//
// Test methods
//

public static void main(String [] args)
{
    Assert.setEnabled(true);

    Log.setApplicationName("NavigationImpl");
    Log.setApplicationVersion("0.0");
    Log.setApplicationVersionDate("$Id: NavigationImpl.java,v 1.11 2004/06/24 16:23:42 edburns%acm.org Exp $");

    try {
        org.mozilla.webclient.BrowserControlFactory.setAppData(args[0]);
	org.mozilla.webclient.BrowserControl control = 
	    org.mozilla.webclient.BrowserControlFactory.newBrowserControl();
        Assert.assert_it(control != null);
	
	Navigation wc = (Navigation)
	    control.queryInterface(org.mozilla.webclient.BrowserControl.WINDOW_CONTROL_NAME);
	Assert.assert_it(wc != null);
    }
    catch (Exception e) {
	System.out.println("got exception: " + e.getMessage());
    }
}

// ----VERTIGO_TEST_END

} // end of class NavigationImpl
