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
 */

package org.mozilla.webclient;

// BrowserControlExtended.java

import java.awt.Rectangle;

/**
 *

 *  <B>BrowserControlExtended</B> defines the interface for extended browser
 *  functionality

 *
 * @version $Id: BrowserControlExtended.java,v 1.2 1999/11/06 02:24:17 dmose%mozilla.org Exp $
 * 
 * @see	org.mozilla.webclient.BrowserControlCore

 * */

public interface BrowserControlExtended
{

public void show () throws Exception;

public void hide () throws Exception;

public void setBounds (Rectangle bounds) throws Exception;

public void moveTo (int x, int y) throws Exception;

public void setFocus () throws Exception;
	
public void removeFocus () throws Exception;

public void repaint (boolean forceRepaint) throws Exception;

public boolean goTo (int historyIndex) throws Exception;

public int getHistoryLength () throws Exception;

public int getHistoryIndex () throws Exception;

public String getURL (int historyIndex) throws Exception;

} // end of interface BrowserControlExtended
