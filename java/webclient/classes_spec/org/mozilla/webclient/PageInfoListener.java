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
 * <p>The <code>eventData</code> property of the
 * <code>DocumentLoadEvent</code> instance will be a
 * <code>java.util.Map</code>.  The following entries may be present in
 * this map for the following <code>*_EVENT_MASK</code> types in
 * <code>DocumentLoadEvent</code>.</p>
 *
 * <dl>
 *
 * <dt>For all <code>*_EVENT_MASK</code> types</dt>
 *
 * <dd><p>the map will contain an entry under the key "<code>URI</code>"
 * without the quotes.  This will be the fully qualified URI for the
 * event. </p></dd>
 *
 * <dt>For <code>START_URL_LOAD</code> type</dt>
 *
 * <dd><p>The map will contain an entry under the key
 * "<code>method</code>" without the quotes.  This will be the request
 * method for this event.  The map will also contain an entry under the
 * key "<code>headers</code>".  This entry will be a
 * <code>java.util.Map</code> of all the request headers.</p></dd>
 *
 * <dt>For <code>END_URL_LOAD</code> type</dt>
 *
 * <dd><p>The map will contain an entry under the key
 * "<code>method</code>" without the quotes.  This will be the request
 * method for this event.  The map will contain an entry under the key
 * "<code>status</code>" without the quotes.  This will be the response
 * status string from the server, such as "<code>200 OK</code>".  The
 * map will also contain an entry under the key "<code>headers</code>".
 * This entry will be a <code>java.util.Map</code> of all the response
 * headers.</p></dd>
 *
 * <dt>For <code>START_AJAX_EVENT_MASK</code> type</dt>

 * <dd><p>The map will contain the following keys and values:</p>

 * <dl>

 * <dt><code>headers</code></dt>

 * <dd>a <code>java.util.Map</code> of all the request headers.</dd>

 * <dt><code>method</code></dt>
 
 * <dd>the request method for this event.</dd>

 * <dt><code>readyState</code></dt>

 * <dd>a String of the numerical value of the XMLHTTPRequest readyState</dd>

 * </dl>

 * </dd>

 * <dt>For <code>END_AJAX_EVENT_MASK</code> type</dt>
 *
 * <dd><p>The map will contain the following keys and values:</p>

 * <dl>

 * <dt><code>method</code></dt>
 
 * <dd>the request method for this event.</dd>

 * <dt>responseXML</dt>

 * <dd>a <code>org.w3c.dom.Document</code> instance of the response XML.</dd>

 * <dt>responseText</dt>

 * <dd>a String instance of the response Text.</dd>

 * <dt><code>status</code></dt>

 * <dd>the response status string from the server, such as "<code>200
 * OK</code>".</dd>

 * <dt><code>headers</code></dt>

 * <dd>a <code>java.util.Map</code> of all the response headers.</dd>

 * <dt><code>method</code></dt>
 
 * <dd>the request method for this event.</dd>

 * <dt><code>readyState</code></dt>

 * <dd>a String of the numerical value of the XMLHTTPRequest readyState</dd>

 * </dl>

 * </dl>
 *
 *
 */

public interface PageInfoListener extends DocumentLoadListener  {
    
}
