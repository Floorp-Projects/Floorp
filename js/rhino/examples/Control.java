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
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

import org.mozilla.javascript.*;

/**
 * Example of controlling the JavaScript execution engine.
 *
 * We evaluate a script and then manipulate the result.
 * 
 */
public class Control {

    /**
     * Main entry point.
     *
     * Process arguments as would a normal Java program. Also
     * create a new Context and associate it with the current thread.
     * Then set up the execution environment and begin to
     * execute scripts.
     */
    public static void main(String[] args) {
        Context cx = Context.enter();

        // Set version to JavaScript1.2 so that we get object-literal style
        // printing instead of "[object Object]"
        cx.setLanguageVersion(Context.VERSION_1_2);

        // Initialize the standard objects (Object, Function, etc.)
        // This must be done before scripts can be executed.
        Scriptable scope = cx.initStandardObjects(null);

        // Now we can evaluate a script. Let's create a new object
        // using the object literal notation.
        Object result = null;
        try {
            result = cx.evaluateString(scope, "obj = {a:1, b:['x','y']}",
                                              "MySource", 1, null);
        }
        catch (JavaScriptException jse) {
            // ignore
        }

        FlattenedObject global = new FlattenedObject(scope);

        FlattenedObject f = (FlattenedObject) global.getProperty("obj");

        // Should print "obj == result" (Since the result of an assignment
        // expression is the value that was assigned)
        System.out.println("obj " + (f.getObject() == result ? "==" : "!=") +
                           " result");

        // Should print "f.a == 1"
        System.out.println("f.a == " + f.getProperty("a"));

        FlattenedObject b = (FlattenedObject) f.getProperty("b");

        // Should print "f.b[0] == x"
        System.out.println("f.b[0] == " + b.getProperty(new Integer(0)));

        // Should print "f.b[1] == y"
        System.out.println("f.b[1] == " + b.getProperty(new Integer(1)));

        try {
            // Should print {a:1, b:["x", "y"]}
            System.out.println(f.callMethod("toString", new Object[0]));
        } catch (PropertyException e) {
            // ignore
        } catch (NotAFunctionException e) {
            // ignore
        } catch (JavaScriptException e) {
            // ignore
        }

        cx.exit();
    }

}

