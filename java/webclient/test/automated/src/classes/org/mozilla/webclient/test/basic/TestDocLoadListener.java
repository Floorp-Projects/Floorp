/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Client QA Team, St. Petersburg, Russia
 */



package org.mozilla.webclient.test.basic;


/*
 * TestLoader.java
 */

import org.mozilla.webclient.DocumentLoadEvent;

public interface TestDocLoadListener {

    /**
     * TestDocLoadListener isn't needed for most of tests.
     * Please use it very carefully and only if really needed.
     *
     * Important: If your test register own TestDocLoadListener then execute()
     * will be never called. If neede, call execute() manually when END_DOCUMENT_LOAD_EVENT_MASK 
     * ocurred. And also please do not use executionCount or increase it manualy.
     *
     */
    
public void eventDispatched(DocumentLoadEvent event);
 
} // end of interface WebclientEventListener
