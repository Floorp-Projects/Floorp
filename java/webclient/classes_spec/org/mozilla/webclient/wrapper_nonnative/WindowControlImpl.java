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
import org.mozilla.webclient.WindowControl;
import org.mozilla.webclient.WrapperFactory;


import org.mozilla.webclient.UnimplementedException;

import ice.storm.*;

import java.awt.Rectangle;

public class WindowControlImpl extends ImplObjectNonnative implements WindowControl
{

public WindowControlImpl(WrapperFactory yourFactory, 
			 BrowserControl yourBrowserControl)
{
    super(yourFactory, yourBrowserControl, false);
}

public void delete()
{

}


public void setBounds(Rectangle newBounds)
{
}

public void createWindow(int nativeWindow, Rectangle bounds)
{
}

public int getNativeWebShell()
{
return 0;
}

public void moveWindowTo(int x, int y)
{
}

public void removeFocus()
{
}
    
public void repaint(boolean forceRepaint)
{
}

public void setVisible(boolean newState)
{
}

public void setFocus()
{
}



// ----VERTIGO_TEST_START

//
// Test methods
//

public static void main(String [] args)
{
    Assert.setEnabled(true);

    Log.setApplicationName("WindowControlImpl");
    Log.setApplicationVersion("0.0");
    Log.setApplicationVersionDate("$Id: WindowControlImpl.java,v 1.1 2001/07/27 21:01:12 ashuk%eng.sun.com Exp $");

    try {
        org.mozilla.webclient.BrowserControlFactory.setAppData("nonnative",args[0]);
	org.mozilla.webclient.BrowserControl control = 
	    org.mozilla.webclient.BrowserControlFactory.newBrowserControl();
        Assert.assert_it(control != null);
	
	WindowControl wc = (WindowControl)
	    control.queryInterface(org.mozilla.webclient.BrowserControl.WINDOW_CONTROL_NAME);
	Assert.assert_it(wc != null);
    }
    catch (Exception e) {
	System.out.println("got exception: " + e.getMessage());
    }
}

// ----VERTIGO_TEST_END

} // end of class WindowControlImpl
