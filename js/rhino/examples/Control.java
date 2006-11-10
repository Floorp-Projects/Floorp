/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0
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
 * The Original Code is Rhino code, released
 * May 6, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 or later (the "GPL"), in which
 * case the provisions of the GPL are applicable instead of those above. If
 * you wish to allow use of your version of this file only under the terms of
 * the GPL and not to allow others to use your version of this file under the
 * MPL, indicate your decision by deleting the provisions above and replacing
 * them with the notice and other provisions required by the GPL. If you do
 * not delete the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    public static void main(String[] args)
    {
        Context cx = Context.enter();
        try {
            // Set version to JavaScript1.2 so that we get object-literal style
            // printing instead of "[object Object]"
            cx.setLanguageVersion(Context.VERSION_1_2);

            // Initialize the standard objects (Object, Function, etc.)
            // This must be done before scripts can be executed.
            Scriptable scope = cx.initStandardObjects();

            // Now we can evaluate a script. Let's create a new object
            // using the object literal notation.
            Object result = cx.evaluateString(scope, "obj = {a:1, b:['x','y']}",
                                              "MySource", 1, null);

            Scriptable obj = (Scriptable) scope.get("obj", scope);

            // Should print "obj == result" (Since the result of an assignment
            // expression is the value that was assigned)
            System.out.println("obj " + (obj == result ? "==" : "!=") +
                               " result");

            // Should print "obj.a == 1"
            System.out.println("obj.a == " + obj.get("a", obj));

            Scriptable b = (Scriptable) obj.get("b", obj);

            // Should print "obj.b[0] == x"
            System.out.println("obj.b[0] == " + b.get(0, b));

            // Should print "obj.b[1] == y"
            System.out.println("obj.b[1] == " + b.get(1, b));

            // Should print {a:1, b:["x", "y"]}
            Function fn = (Function) ScriptableObject.getProperty(obj, "toString");
            System.out.println(fn.call(cx, scope, obj, new Object[0]));
        } finally {
            Context.exit();
        }
    }

}

