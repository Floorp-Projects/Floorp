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
 * RunScript2: Like RunScript, but reflects the System.out into JavaScript.
 * 
 * @author Norris Boyd
 */
public class RunScript2 {
    public static void main(String args[]) 
        throws JavaScriptException 
    {
        Context cx = Context.enter();
        Scriptable scope = cx.initStandardObjects(null);

        // Add a global variable "out" that is a JavaScript reflection
        // of System.out
        Scriptable jsArgs = Context.toObject(System.out, scope);
        scope.put("out", scope, jsArgs);
        
        String s = "";
        for (int i=0; i < args.length; i++)
            s += args[i];
        Object result = cx.evaluateString(scope, s, "<cmd>", 1, null);
        System.err.println(cx.toString(result));
        Context.exit();
    }

}

