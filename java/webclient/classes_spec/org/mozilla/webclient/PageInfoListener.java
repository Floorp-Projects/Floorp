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
 * <p>This {@link DocumentLoadListener} subclass adds the ability to get
 * detailed information on each event. </p>
 *
 * <p>The <code>eventDispatched()</code> method is passed the same thing
 * as in the {@link DocumentLoadListener}.</p>
 *
 * <p>The <code>eventData</code> property of the
 * <code>DocumentLoadEvent</code> instance will be a
 * <code>java.util.Map</code>.  For the
 * <code>END_URL_LOAD_EVENT_MASK</code> type in
 * <code>DocumentLoadEvent</code> the map will contain an entry under
 * the key "<code>URI</code>" without the quotes.  This will be the
 * fully qualified URI for the event.  The map will also contain an
 * entry under the key "<code>headers</code>".  This entry will be a
 * <code>Map</code> of all the response headers.</p>
 *
 */

public interface PageInfoListener extends DocumentLoadListener  {
    
}
