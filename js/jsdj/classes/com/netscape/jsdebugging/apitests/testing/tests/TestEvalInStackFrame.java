/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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