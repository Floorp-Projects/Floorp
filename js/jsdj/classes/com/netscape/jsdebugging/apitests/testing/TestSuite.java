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