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

/* JavaScript command line Debugger
 * @author Alex Rakhlin
 */

package com.netscape.jsdebugging.jsdb;

import com.netscape.jsdebugging.api.*;

class JSDBInstructionHook
    extends InstructionHook
{

    public JSDBInstructionHook(PC pc, DebugController controller, JSDBBreakpoint bp) {
        super (pc);
        _bp = bp;
        _controller = controller;
        System.out.println ("Set breakpoint at "+
                 ((JSPC)pc).getScript().getURL()+" "+
                 ((JSPC)pc).getScript().getFunction()+" "+
                 ((JSPC)pc).getSourceLocation().getLine());
    }

    public void aboutToExecute(ThreadStateBase debug) {
        if (_bp.isEnabled())
            _controller.getInterruptHook().aboutToExecute(debug, getPC());
    }

    private DebugController _controller;
    private JSDBBreakpoint _bp;
}
