/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
  
package com.netscape.jsdebugging.apitests.testing;

import com.netscape.jsdebugging.apitests.xml.*;
import com.netscape.jsdebugging.apitests.testing.desc.*;
import com.netscape.jsdebugging.api.*;

/**
 * All Tests should extend this class.
 *
 * @author Alex Rakhlin
 */

public abstract class Test {

    public abstract void justLoadedScript(Script script);
    public abstract void aboutToUnloadScript(Script script);
    public abstract void aboutToExecuteInstructionHook (ThreadStateBase debug);
    public abstract void aboutToExecuteInterruptHook (ThreadStateBase debug, PC pc);
    public abstract int reportError(String msg, String filename, int lineno, String linebuf, int tokenOffset);
    
    
    public void setDescriptorManager (DescriptorManager dmgr) { _dmgr = dmgr; }
    public void setXMLWriter (XMLWriter xmlw) { _xmlw = xmlw; } //should be moved to _dmgr
    
    public DescriptorManager getDmgr () { return _dmgr; }
    
    private DescriptorManager _dmgr;
    
    protected XMLWriter _xmlw;
    
    public void setTestSuite (TestSuite ts) { _test_suite = ts; }
    public TestSuite getTestSuite () { return _test_suite; }
    protected TestSuite _test_suite;
}