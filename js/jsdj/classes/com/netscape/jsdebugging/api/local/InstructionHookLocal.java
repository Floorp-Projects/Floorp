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

package com.netscape.jsdebugging.api.local;

import com.netscape.jsdebugging.api.*;

/**
 * InstructionHook must be subinterfaceed to respond to hooks
 * at a particular program instruction.
 */
public class InstructionHookLocal extends netscape.jsdebug.InstructionHook
{
    InstructionHookLocal( InstructionHook hook, 
                          netscape.jsdebug.JSPC pc )
    {
        super(pc);
        _hook = hook;
    }

    public void aboutToExecute(netscape.jsdebug.ThreadStateBase debug)
    {
        JSThreadStateLocal ts = new JSThreadStateLocal(debug);
        ts.aboutToCallHook();
        _hook.aboutToExecute(ts);
        ts.returnedFromCallHook();
    }

    public InstructionHook getWrappedHook() {return _hook;}

    private InstructionHook _hook;
}
