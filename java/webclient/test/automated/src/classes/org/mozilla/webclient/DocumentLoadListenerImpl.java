/*
 * $Id: DocumentLoadListenerImpl.java,v 1.1 2004/06/23 19:58:12 edburns%acm.org Exp $
 */

/* 
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun
 * Microsystems, Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): Ed Burns &lt;edburns@acm.org&gt;
 */

package org.mozilla.webclient;

public abstract class DocumentLoadListenerImpl implements DocumentLoadListener {

	public void eventDispatched(WebclientEvent event) {
	    if (event instanceof DocumentLoadEvent) {
		switch ((int) event.getType()) {
		case ((int) DocumentLoadEvent.END_DOCUMENT_LOAD_EVENT_MASK):
		    doEndCheck();
		    break;
		case ((int) DocumentLoadEvent.PROGRESS_URL_LOAD_EVENT_MASK):
		    doProgressCheck();
		    break;
		}
	    }
	}	
	
	public void doEndCheck() {}

	public void doProgressCheck() {}
    }

