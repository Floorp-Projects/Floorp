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
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 *               Ann Sunhachawee
 */

package org.mozilla.webclient;

import java.io.InputStream;
import java.util.Properties;

public interface Navigation
{

/**

 * These flags can be or'd together for the argument to refresh().

 */

public static int LOAD_NORMAL = 0;
public static int LOAD_FORCE_RELOAD = 1 << 9;

public void loadURL(String absoluteURL);
public void loadFromStream(InputStream stream, String uri,
                           String contentType, int contentLength,
                           Properties loadInfo);
public void refresh(long loadFlags);
public void stop();

/**

 * Gives this Navigation instance the ability to call back the custom
 * app when a site with basic authentication, cookies, etc, is
 * encountered.  The custom app can choose to put up appropriate modal
 * UI.

 */

public void setPrompt(Prompt yourPrompt);

} 
// end of interface CurrentPage
