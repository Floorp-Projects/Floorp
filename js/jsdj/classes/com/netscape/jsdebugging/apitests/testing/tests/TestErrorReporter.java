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