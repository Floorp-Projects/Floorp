/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package com.netscape.jsdebugging.engine.local;

import com.netscape.jsdebugging.engine.*;

public class RuntimeLocal implements IRuntime
{
    private RuntimeLocal() {}

    public static final String LOCAL_LOADER_NAME = 
                            "com.netscape.jsdebugging.api.local.AdapterLoaderLocal";

    RuntimeLocal(boolean enableDebugging, String[] args) {
        _runtime = com.netscape.nativejsengine.JSRuntime.newRuntime(
                        com.netscape.nativejsengine.JSRuntime.ENABLE_DEBUGGING);
        if(null == _runtime) {
            _destroyed = true;
            return;
        }

        if(enableDebugging) {

            try {
                Class clazz = Class.forName(LOCAL_LOADER_NAME);
                com.netscape.jsdebugging.api.AdapterLoader loader = 
                    (com.netscape.jsdebugging.api.AdapterLoader) clazz.newInstance();

                _controller = loader.getDebugController();
                _stp = loader.getSourceTextProvider();
            }
            catch(Throwable e) {
                System.out.println(e);
            }
        }
    }

    public IContext newContext(String[] args) {
        ContextLocal context = new ContextLocal(this, args);
        if(! context.isValid())
            return null;
        return context;
    }

    public boolean isValid() {return ! _destroyed;}

    // finalization not guaranteed to happen in right order - trust programmer
    public void destroy() {
        _destroyed = true;
        _controller = null;
        _stp        = null;
        _runtime.destroy();
    }

    com.netscape.nativejsengine.JSRuntime getNativeRuntime() {return _runtime;}

    public com.netscape.jsdebugging.api.DebugController 
                                getDebugController()    {return _controller;}

    public com.netscape.jsdebugging.api.SourceTextProvider 
                                getSourceTextProvider() {return _stp;}

    private com.netscape.nativejsengine.JSRuntime _runtime;
    private boolean _destroyed = false;
    private com.netscape.jsdebugging.api.DebugController _controller;
    private com.netscape.jsdebugging.api.SourceTextProvider _stp;
}    
