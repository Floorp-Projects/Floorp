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
 * Calls error descriptor when reportError is called.
 *
 * @author Alex Rakhlin
 */

public class TestErrorReporter extends Test {

    public TestErrorReporter (){
    }

    public void justLoadedScript(Script script){
    }

    public void aboutToUnloadScript(Script script){
    }

    public void aboutToExecuteInstructionHook (ThreadStateBase debug){
    }

    public void aboutToExecuteInterruptHook (ThreadStateBase debug, PC pc){
    }

    public int reportError( String msg,
                            String filename,
                            int    lineno,
                            String linebuf,
                            int    tokenOffset )
    {
        Vector error = new Vector ();
        /* package these five elements into a vector and pass it to the error descriptor.
         * note that even though we make DescError serializable, there will be no matches
         * since we create a new Vector each time */
        error.addElement (msg);
        error.addElement (filename);
        error.addElement (new Integer(lineno));
        error.addElement (linebuf);
        error.addElement (new Integer(tokenOffset));        
        ((DescError) getDmgr().getDescriptor (DescError.class)).describe (error);
        return 0;
    }
}