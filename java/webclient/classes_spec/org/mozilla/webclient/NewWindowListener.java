/* -*- Mode: java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Contributor(s):  Kyle Yuan <kyle.yuan@sun.com>
 */

package org.mozilla.webclient;

/**
 * <p>This interface allows the implementer to be notified of {@link
 * NewWindowEvent}s that occur as a result of user browsing.  For
 * example, pop-ups.</p>
 *
 * <p>The constants defined in this interface should really be defined
 * on {@link NewWindowEvent}, but they remain here for backwards
 * compatability.  They are given to the user as the value of the
 * <code>type</code> property of the <code>NewWindowEvent</code>.  They
 * advise the user on the characteristics of the new window to be
 * created.  They should be anded with the argument eventType to
 * determine if they are indicated for this event.  Here is an example
 * of how to create a new window in response to a
 * <code>NewWindowEvent</code>:</p>
 *
 * <code><pre>
 *
 *
public void eventDispatched(WebclientEvent newWindowEvent) {

  BrowserControl newBrowserControl = null;
  BrowserControlCanvas newCanvas = null;

  Frame newFrame = new Frame();

  long type = newWindowEvent.getType();
  if (type & NewWindowListener.CHROME_MENUBAR) {
    // create a menu bar for the new window
  }
  if (type & NewWindowListener.CHROME_WINDOW_CLOSE) {
    // Make it so this window cannot be closed
  }

  try {
    newBrowserControl = BrowserControlFactory.newBrowserControl();
    newCanvas = (BrowserControlCanvas)
      newBrowserControl.queryInterface(BrowserControl.BROWSER_CONTROL_CANVAS_NAME);
    // obtain any other interfaces you need.
  } catch (Throwable e) {
      System.out.println(e.getMessage());
  }

  newFrame.add(newCanvas, BorderLayout.CENTER);
  newFrame.setVisible(true);
  newCanvas.setVisible(true);
}

 * </pre></code>
 *
 * <p></p>
 */

public interface NewWindowListener extends WebclientEventListener  {

    public static final long CHROME_DEFAULT = 1;

    public static final long CHROME_WINDOW_BORDERS = 1 << 1;

    /* 
     * Specifies whether the window can be closed. 
     */

    public static final long CHROME_WINDOW_CLOSE = 1 << 2;

    /**
     * Specifies whether the window is resizable. 
     */

    public static final long CHROME_WINDOW_RESIZE = 1 << 3;

    /**
     * Specifies whether to display the menu bar. 
     */

    public static final long CHROME_MENUBAR = 1 << 4;

    /** 
     * Specifies whether to display the browser toolbar, making buttons
     * such as Back, Forward, and Stop available.
     */

    public static final long CHROME_TOOLBAR = 1 << 5;

    /** 
     * Specifies whether to display the input field for entering URLs
     * directly into the browser. 
     */

    public static final long CHROME_LOCATIONBAR = 1 << 6;

    /** 
     * Specifies whether to add a status bar at the bottom of the window. 
     */

    public static final long CHROME_STATUSBAR = 1 << 7;

    /** 
     * Specifies whether to display the personal bar. (Mozilla only) 
     */
    
    public static final long CHROME_PERSONAL_TOOLBAR = 1 << 8;

    /**
     * Specifies whether to display horizontal and vertical scroll bars.
     */

    public static final long CHROME_SCROLLBARS = 1 << 9;

    /**
     * Specifies whether to display a title bar for the window. 
     */

    public static final long CHROME_TITLEBAR = 1 << 10;

    public static final long CHROME_EXTRA = 1 << 11;
    
    public static final long CHROME_WITH_SIZE = 1 << 12;
    
    public static final long CHROME_WITH_POSITION = 1 << 13;

    /**
     * Specifies whether the window is minimizable. 
     */

    public static final long CHROME_WINDOW_MIN = 1 << 14;

    public static final long CHROME_WINDOW_POPUP = 1 << 15;
    
    public static final long CHROME_ALL = 4094;

}
