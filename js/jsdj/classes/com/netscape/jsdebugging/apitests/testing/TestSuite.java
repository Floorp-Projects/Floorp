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
   
package com.netscape.jsdebugging.apitests.testing;
 
import com.netscape.jsdebugging.apitests.xml.*;
import com.netscape.jsdebugging.apitests.testing.desc.*;
import com.netscape.jsdebugging.apitests.testing.tests.*;
import com.netscape.jsdebugging.api.*;
import java.util.*;

/**
 * Test Suite is a vector of all tests. The hooks call to the appropriate method of
 * the TestSuite and it executes all appropriate methods of the tests.
 *
 * @author Alex Rakhlin
 */

public class TestSuite {

    public TestSuite (XMLWriter xmlw, DebugController controller){
        _xmlw = xmlw;
        _tests = new Vector ();
        _dmgr = new DescriptorManager();
        _dmgr.addDescriptors(xmlw);
        _controller = controller;
    }

    /**
     * Add a test to the test suite.
     */
    public void addTest (Test t) {
        t.setDescriptorManager (_dmgr);
        t.setTestSuite (this);
        _tests.addElement (t);
    }
    
    /* FUNCTIONS */
    
    /**
     * Calls justLoadedScript method of all the tests.
     */
    public void justLoadedScript(Script script){
        for (int i = 0; i<_tests.size(); i++) 
            ((Test) _tests.elementAt (i)).justLoadedScript(script);            
    }
    
    /**
     * Calls aboutToUnloadScript method of all the tests.
     */
    public void aboutToUnloadScript(Script script){
        for (int i = 0; i<_tests.size(); i++) 
            ((Test) _tests.elementAt (i)).aboutToUnloadScript(script);
    }
    
    /**
     * Calls aboutToExecuteInstructionHook method of all the tests.
     */
    public void aboutToExecuteInstructionHook (ThreadStateBase debug){
        for (int i = 0; i<_tests.size(); i++) 
            ((Test) _tests.elementAt (i)).aboutToExecuteInstructionHook (debug);
    }
    
    /**
     * Calls aboutToExecuteInterruptHook method of all the tests.
     */
    public void aboutToExecuteInterruptHook (ThreadStateBase debug, PC pc){
        for (int i = 0; i<_tests.size(); i++) 
            ((Test) _tests.elementAt (i)).aboutToExecuteInterruptHook (debug, pc);

        _controller.sendInterrupt();
    }
    
    /**
     * Calls reportError method of all the tests.
     * Returns the value returned by the last test...
     */
    public int reportError(String msg, String filename, int lineno, String linebuf, int tokenOffset){
        int ret = 0; 
        for (int i = 0; i < _tests.size(); i++)
            ret = ((Test) _tests.elementAt (i)).reportError (msg, filename, lineno, linebuf, tokenOffset);
        return ret; //just return the last value;
    }
    
    /* END FUNCTIONS */
    

    private Vector _tests;
    private DescriptorManager _dmgr; 
    private XMLWriter _xmlw;
    
    public DebugController getDebugController () { return _controller; }
    private DebugController _controller;
    
    
    /* Set of flags to share information between tests. */
    
    /* If doing eval (st), Rhino loads st as a script. So, if doing eval, don't load scripts. */
    public boolean _doing_eval = false;
}