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

/**
 * InstructionHook must be subinterfaceed to respond to hooks
 * at a particular program instruction.
 */
public class InstructionHookCorba 
    extends com.netscape.jsdebugging.remote.corba._sk_IJSExecutionHook
{
    private static int _objectCounter = 0;

    InstructionHookCorba(DebugControllerCorba controller,
                         InstructionHook hook)
    {
        super( "InstructionHookCorba_"+_objectCounter++);
        _controller = controller;
        _hook = hook;
    }

    public void aboutToExecute(com.netscape.jsdebugging.remote.corba.IJSThreadState debug, 
                               com.netscape.jsdebugging.remote.corba.IJSPC pc)
    {
        _hook.aboutToExecute( new JSThreadStateCorba(_controller, debug) );
    }

    public InstructionHook getWrappedHook() {return _hook;}

    private DebugControllerCorba _controller;
    private InstructionHook _hook;
}
