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

public class DocumentLoadEvent extends WebclientEvent
{

public static final long START_DOCUMENT_LOAD_EVENT_MASK = 1;
public static final long END_DOCUMENT_LOAD_EVENT_MASK = 1 << 2;
public static final long START_URL_LOAD_EVENT_MASK = 1 << 3;
public static final long END_URL_LOAD_EVENT_MASK = 1 << 4;
public static final long PROGRESS_URL_LOAD_EVENT_MASK = 1 << 5;
public static final long STATUS_URL_LOAD_EVENT_MASK = 1 << 6;
public static final long UNKNOWN_CONTENT_EVENT_MASK = 1 << 7;
public static final long FETCH_INTERRUPT_EVENT_MASK = 1 << 8;

//
// Constructors
//

public DocumentLoadEvent(Object source, long newType, 
                         Object newEventData)
{
    super(source, newType, newEventData);
}

} // end of class DocumentLoadEvent
