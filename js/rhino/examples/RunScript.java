/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

import org.mozilla.javascript.*;

/**
 * RunScript: simplest example of controlling execution of Rhino.
 * 
 * Collects its arguments from the command line, executes the 
 * script, and prints the result.
 * 
 * @author Norris Boyd
 */
public class RunScript {
    public static void main(String args[]) 
        throws JavaScriptException 
    {
        // Creates and enters a Context. The Context stores information
        // about the execution environment of a script.
        Context cx = Context.enter();
        
        // Initialize the standard objects (Object, Function, etc.)
        // This must be done before scripts can be executed. Returns
        // a scope object that we use in later calls.
        Scriptable scope = cx.initStandardObjects(null);
        
        // Collect the arguments into a single string.
        String s = "";
        for (int i=0; i < args.length; i++)
            s += args[i];
        
        // Now evaluate the string we've colected.
        Object result = cx.evaluateString(scope, s, "<cmd>", 1, null);
        
        // Convert the result to a string and print it.
        System.err.println(cx.toString(result));
        
        // Exit from the context.
        Context.exit();
    }
}

