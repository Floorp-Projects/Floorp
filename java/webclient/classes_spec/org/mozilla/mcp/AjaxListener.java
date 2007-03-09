/*
 * $Id: AjaxListener.java,v 1.1 2007/03/09 04:34:24 edburns%acm.org Exp $
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

package org.mozilla.mcp;

import java.util.Map;
import org.mozilla.webclient.DocumentLoadEvent;
import org.mozilla.webclient.PageInfoListener;
import org.mozilla.webclient.WebclientEvent;

/**
 *
 * @author edburns
 */
public class AjaxListener implements PageInfoListener {
    public void eventDispatched(WebclientEvent event) {
        Map map = (Map) event.getEventData();
        if (!(event instanceof DocumentLoadEvent)) {
            return;
        }
        DocumentLoadEvent dle = (DocumentLoadEvent) event;
        int eventType = (int) dle.getType();
        switch (eventType) {
            case (int) DocumentLoadEvent.START_AJAX_EVENT_MASK:
                startAjax(map);
                break;
            case (int) DocumentLoadEvent.ERROR_AJAX_EVENT_MASK:
                errorAjax(map);
                break;
            case (int) DocumentLoadEvent.END_AJAX_EVENT_MASK:
                endAjax(map);
                break;
        }
    }
    
    public void startAjax(Map eventMap) {
    }
    
    public void errorAjax(Map eventMap) {
    }
    
    public void endAjax(Map eventMap) {
        
    }
    
    
    
}
