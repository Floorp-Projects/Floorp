/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

package com.netscape.jsdebugging.api.corba;

import netscape.security.ForbiddenTargetException;
import com.netscape.jsdebugging.api.DebugController;
import com.netscape.jsdebugging.api.SourceTextProvider;

public class AdapterLoaderCorba
    implements com.netscape.jsdebugging.api.AdapterLoader
{
    public void setHost(String host) {_host = host;}
    public boolean isDebuggingSupported() throws ForbiddenTargetException
    {
        return null != getDebugController();
    }
    public com.netscape.jsdebugging.api.DebugController getDebugController() throws ForbiddenTargetException
    {
        if(null == _controller)
            _controller = DebugControllerCorba.getDebugController(_host);
        return _controller;
    }
    public com.netscape.jsdebugging.api.SourceTextProvider getSourceTextProvider()
    {
        try {
            DebugControllerCorba controller = 
                (DebugControllerCorba) getDebugController();
            return SourceTextProviderCorba.getSourceTextProvider(controller);
        }
        catch(ForbiddenTargetException e) {
            // eat it
        }
        return null;
    }
    
    private DebugControllerCorba _controller;
    private String _host;
}    
