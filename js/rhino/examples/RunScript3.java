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
 * RunScript3: Execute scripts in an environment that includes the
 *             example Counter class.
 * 
 * @author Norris Boyd
 */
public class RunScript3 {
    public static void main(String args[]) 
        throws Exception 
    {
        Context cx = Context.enter();
        Scriptable scope = cx.initStandardObjects(null);

        // Use the Counter class to define a Counter constructor
        // and prototype in JavaScript.
        ScriptableObject.defineClass(scope, Counter.class);
        
        String s = "";
        for (int i=0; i < args.length; i++)
            s += args[i];
        Object result = cx.evaluateString(scope, s, "<cmd>", 1, null);
        System.err.println(cx.toString(result));
        Context.exit();
    }

}

