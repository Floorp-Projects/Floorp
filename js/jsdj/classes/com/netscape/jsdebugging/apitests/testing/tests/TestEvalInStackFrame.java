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
 * Evaluates an expression and calls eval descriptor when an interrupt occurs.
 *
 * @author Alex Rakhlin
 */

public class TestEvalInStackFrame extends Test {

    public TestEvalInStackFrame (){
    }

    public void justLoadedScript(Script script){
    }

    public void aboutToUnloadScript(Script script){
    }

    public void aboutToExecuteInstructionHook (ThreadStateBase debug){
    }

    public void aboutToExecuteInterruptHook (ThreadStateBase debug, PC pc){
        String what_to_evaluate = "a";
        //((DescEval) getDmgr().getDescriptor (DescInterrupt.class)).describe (debug);
        ExecResult result;
        JSThreadState state = (JSThreadState) debug;
        JSStackFrameInfo frame;
        try {
            frame = (JSStackFrameInfo) state.getCurrentFrame();            
            DebugController controller = getTestSuite().getDebugController();
            getTestSuite()._doing_eval = true;
            result = controller.executeScriptInStackFrame (frame, what_to_evaluate, "EVAL", 1);
            getTestSuite()._doing_eval = false;
            
            Vector pack = new Vector (); 
            pack.addElement (frame);
            pack.addElement (what_to_evaluate);
            
            if(! result.getErrorOccured()) pack.addElement (result.getResult());
            else pack.addElement (result.getErrorMessage());

            ((DescEval) getDmgr().getDescriptor (DescEval.class)).describe (pack);
        }
        catch (InvalidInfoException e){ }
        /*
        try {
            _length_stack = state.getStack().length;
        }
        catch (InvalidInfoException e){
        }*/
    }

    public int reportError(String msg, String filename, int lineno, String linebuf, int tokenOffset){
        return 0;
    }
}