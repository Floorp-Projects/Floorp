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
 * Contributor(s):  Ed Burns <edburns@acm.org>
 */

package org.mozilla.webclient;

/**
 * <p>The {@link WebclientEventListener#eventDispatched} method is
 * passed a {@link DocumentLoadEvent} instance.  The <code>type</code>
 * property of the event will be one of the types defined as a
 * <code>public static final int</code> constant in
 * <code>DocumentLoadEvent</code>.</p>
 *
 * <p>The <code>eventData</code> property of the
 * <code>DocumentLoadEvent</code> instance will be a
 * <code>java.util.Map</code>.  For all <code>type</code>s in
 * <code>DocumentLoadEvent</code> the map will contain an entry under
 * the key "<code>URI</code>" without the quotes.  This will be the
 * fully qualified URI for the event.</p>
 *
 * <p>For the <code>PROGRESS_URL_LOAD_EVENT_MASK</code> there will be an
 * entry in the map for the key "<code>message</code>" without the
 * quotes.  This will be the progress message from the browser.</p>
 *
 * <p>For extended information about the event, implement {@link
 * PageInfoListener}.</p>
 *
 */

public interface DocumentLoadListener extends WebclientEventListener  {
    
}
