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

import com.netscape.jsdebugging.api.*;

/**
 * DebugBreakHook must be subclassed to respond to hooks
 * at a particular program instruction.
 */
public class DebugBreakHookCorba 
    extends com.netscape.jsdebugging.remote.corba._sk_IJSExecutionHook
{
    private static int _objectCounter = 0;

    public DebugBreakHookCorba(DebugControllerCorba controller,
                               DebugBreakHook hook)
    {
        super( "DebugBreakHookCorba_"+_objectCounter++);
        _controller = controller;
        _hook = hook;
    }

    public void aboutToExecute(com.netscape.jsdebugging.remote.corba.IJSThreadState debug, 
                               com.netscape.jsdebugging.remote.corba.IJSPC pc)
    {
        try
        {
            JSThreadStateCorba ts = new JSThreadStateCorba(_controller, debug);
            JSPCCorba jspc = (JSPCCorba) ts.getCurrentFrame().getPC();
            _hook.aboutToExecute( ts, jspc );
        }
        catch( Exception e )
        {
            e.printStackTrace();
            // eat it.
        }
    }

    public DebugBreakHook getWrappedHook() {return _hook;}

    private DebugControllerCorba _controller;
    private DebugBreakHook _hook;
}
