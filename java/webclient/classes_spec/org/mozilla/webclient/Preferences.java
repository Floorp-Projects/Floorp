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

import java.util.Properties;

/**
 * <p>This interface provides a way to set browser specific preferences
 * on the underlying browser implementation.</p>
 *
 * <p>The following mozilla preferences are especially useful:</p>
 *
 * 	<ul>
 *
 *	  <li><p><code>network.cookie.cookieBehavior</code></p>
 *
 *    <table>
 *
 *    <tr><th>Pref Value</th> <th>Pref meaning</th></tr>
 *
 *    <tr><td>0</td> <td>Allow all cookies</td></tr>
 *
 *    <tr><td>1</td> <td>Allow cookies from the originating website
 *    only</td></tr>
 *
 *    <tr><td>2</td> <td>Block cookies</td></tr>
 *
 *    </table>
 *    </li>
 *
 *	</ul>
 */

public interface Preferences
{
  public void setPref(String prefName, String prefValue);
  public Properties getPrefs();
  public void registerPrefChangedCallback(PrefChangedCallback cb,
                                          String prefName, Object closure);
  public void unregisterPrefChangedCallback(PrefChangedCallback cb,
                                            String prefName, Object closure);
} 
// end of interface Preferences

