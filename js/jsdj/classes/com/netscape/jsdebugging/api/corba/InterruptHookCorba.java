/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

package com.netscape.jsdebugging.api.corba;

import com.netscape.jsdebugging.api.*;
import com.netscape.jsdebugging.ifcui.Log;
import com.netscape.jsdebugging.ifcui.palomar.util.ER;

/**
 * InterruptHook must be subinterfaceed to respond to hooks
 * at a particular program instruction.
 */
public class InterruptHookCorba 
    extends com.netscape.jsdebugging.remote.corba._sk_IJSExecutionHook
{
    private static int _objectCounter = 0;
    public InterruptHookCorba(DebugControllerCorba controller,
                              InterruptHook hook)
    {
        super( "InterruptHookCorba_"+_objectCounter++);
        _controller = controller;
        _hook = hook;
    }

    public void aboutToExecute(com.netscape.jsdebugging.remote.corba.IJSThreadState debug, 
                               com.netscape.jsdebugging.remote.corba.IJSPC pc)
    {
        if(ASS)Log.trace("com.netscape.jsdebugging.api.corba.InterruptHookCorba.aboutToExecute", "in ->  , JSThreadID: "+debug.id);
//        System.out.println( "InterruptHookCorba called..." );
        try
        {
            JSThreadStateCorba ts = new JSThreadStateCorba(_controller,debug);
            JSPCCorba jspc = (JSPCCorba) ts.getCurrentFrame().getPC();
            _hook.aboutToExecute( ts, jspc );
        }
        catch( Exception e )
        {
            e.printStackTrace();
            // eat it.
        }
        if(ASS)Log.trace("com.netscape.jsdebugging.api.corba.InterruptHookCorba.aboutToExecute", "out <- , JSThreadID: "+debug.id);
    }

    public InterruptHook getWrappedHook() {return _hook;}

    private DebugControllerCorba _controller;
    private InterruptHook _hook;

    private static final boolean ASS = true; // enable ASSERT support?
}
