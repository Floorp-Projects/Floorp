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

package com.netscape.jsdebugging.apitests.testing.tests;

import com.netscape.jsdebugging.apitests.testing.*;
import com.netscape.jsdebugging.apitests.testing.desc.*;
import com.netscape.jsdebugging.api.*;
import java.util.*;

/**
 * Calls interrupt descriptor when an interrupt occurs on a new line (stepping).
 *
 * @author Alex Rakhlin
 */

public class TestStepping extends Test {

    public TestStepping (){
        _old_pc = null;
    }

    public void justLoadedScript(Script script){
    }

    public void aboutToUnloadScript(Script script){
    }

    public void aboutToExecuteInstructionHook (ThreadStateBase debug){
    }

    public void aboutToExecuteInterruptHook (ThreadStateBase debug, PC pc){
        if (_should_stop ((JSPC) pc)) {
            ((DescStep) getDmgr().getDescriptor (DescStep.class)).describe (debug);
            _old_pc = (JSPC) pc;
        }
    }
    
    public int reportError(String msg, String filename, int lineno, String linebuf, int tokenOffset){
        return 0;
    }
    
    private boolean _should_stop (JSPC pc){
        if (_old_pc == null) return true;
        if (!pc.getScript().equals(_old_pc.getScript())) return true;
        if (pc.getSourceLocation().getLine() != _old_pc.getSourceLocation().getLine()) return true;
        return false;
    }
    
    
    private JSPC _old_pc;


}