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

public interface History
{
public void back();

public boolean canBack();

public HistoryEntry [] getBackList(); 

public void clearHistory();

public void forward();

public boolean canForward();

public HistoryEntry [] getForwardList();

public HistoryEntry [] getHistory();

public HistoryEntry getHistoryEntry(int historyIndex);

public int getCurrentHistoryIndex();

public void setCurrentHistoryIndex(int historyIndex);

public int getHistoryLength();

public String getURLForIndex(int historyIndex);
} // end of interface History
