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
 * Contributor(s):  Kyle Yuan <kyle.yuan@sun.com>
 */

package org.mozilla.webclient;

public interface NewWindowListener extends WebclientEventListener  {

public static final long CHROME_DEFAULT = 1;

public static final long CHROME_WINDOW_BORDERS = 1 << 1;

/* Specifies whether the window can be closed. */
public static final long CHROME_WINDOW_CLOSE = 1 << 2;

/* Specifies whether the window is resizable. */
public static final long CHROME_WINDOW_RESIZE = 1 << 3;

/* Specifies whether to display the menu bar. */
public static final long CHROME_MENUBAR = 1 << 4;

/* Specifies whether to display the browser toolbar, making buttons such as Back, Forward, and Stop available. */
public static final long CHROME_TOOLBAR = 1 << 5;

/* Specifies whether to display the input field for entering URLs directly into the browser. */
public static final long CHROME_LOCATIONBAR = 1 << 6;

/* Specifies whether to add a status bar at the bottom of the window. */
public static final long CHROME_STATUSBAR = 1 << 7;

/* Specifies whether to display the personal bar. (Mozilla only) */
public static final long CHROME_PERSONAL_TOOLBAR = 1 << 8;

/* Specifies whether to display horizontal and vertical scroll bars. */
public static final long CHROME_SCROLLBARS = 1 << 9;

/* Specifies whether to display a title bar for the window. */
public static final long CHROME_TITLEBAR = 1 << 10;

public static final long CHROME_EXTRA = 1 << 11;

public static final long CHROME_WITH_SIZE = 1 << 12;

public static final long CHROME_WITH_POSITION = 1 << 13;

/* Specifies whether the window is minimizable. */
public static final long CHROME_WINDOW_MIN = 1 << 14;

public static final long CHROME_WINDOW_POPUP = 1 << 15;

public static final long CHROME_ALL = 4094;

}
