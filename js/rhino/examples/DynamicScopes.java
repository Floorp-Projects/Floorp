/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Rhino code, released
 * May 6, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Norris Boyd
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

import org.mozilla.javascript.*;

/**
 * Example of controlling the JavaScript with multiple scopes and threads.
 */
public class DynamicScopes {

    /**
     * Main entry point.
     *
     * Set up the shared scope and then spawn new threads that execute
     * relative to that shared scope. Try compiling functions with and
     * without dynamic scope to see the effect.
     *
     * The expected output is
     * <pre>
     * sharedScope
     * sharedScope
     * sharedScope
     * thread0
     * thread1
     * thread2
     * </pre>
     * The final three lines may be permuted in any order depending on
     * thread scheduling.
     */
    public static void main(String[] args)
        throws JavaScriptException
    {
        Context cx = Context.enter();
        try {
            cx.setCompileFunctionsWithDynamicScope(false);
            runScripts(cx);
            cx.setCompileFunctionsWithDynamicScope(true);
            runScripts(cx);
        } finally {
            cx.exit();
        }
    }

    static void runScripts(Context cx)
        throws JavaScriptException
    {
        // Initialize the standard objects (Object, Function, etc.)
        // This must be done before scripts can be executed. The call
        // returns a new scope that we will share.
        Scriptable scope = cx.initStandardObjects(null);

        // Now we can evaluate a script and functions will be compiled to
        // use dynamic scope if the Context is so initialized.
        String source = "var x = 'sharedScope';" +
                        "function f() { return x; }";
        cx.evaluateString(scope, source, "MySource", 1, null);

        // Now we spawn some threads that execute a script that calls the
        // function 'f'. The scope chain looks like this:
        // <pre>
        //            ------------------
        //           |   shared scope   |
        //            ------------------
        //                    ^
        //                    |
        //            ------------------
        //           | per-thread scope |
        //            ------------------
        //                    ^
        //                    |
        //            ------------------
        //           | f's activation   |
        //            ------------------
        // </pre>
        // Both the shared scope and the per-thread scope have variables 'x'
        // defined in them. If 'f' is compiled with dynamic scope enabled,
        // the 'x' from the per-thread scope will be used. Otherwise, the 'x'
        // from the shared scope will be used. The 'x' defined in 'g' (which
        // calls 'f') should not be seen by 'f'.
        final int threadCount = 3;
        Thread[] t = new Thread[threadCount];
        for (int i=0; i < threadCount; i++) {
            String script = "function g() { var x = 'local'; return f(); }" +
                            "java.lang.System.out.println(g());";
            t[i] = new Thread(new PerThread(scope, script,
                                            "thread" + i));
        }
        for (int i=0; i < threadCount; i++)
            t[i].start();
        // Don't return in this thread until all the spawned threads have
        // completed.
        for (int i=0; i < threadCount; i++) {
            try {
                t[i].join();
            } catch (InterruptedException e) {
            }
        }
    }

    static class PerThread implements Runnable {

        PerThread(Scriptable scope, String script, String x) {
            this.scope = scope;
            this.script = script;
            this.x = x;
        }

        public void run() {
            // We need a new Context for this thread.
            Context cx = Context.enter();
            try {
                // We can share the scope.
                Scriptable threadScope = cx.newObject(scope);
                threadScope.setPrototype(scope);

                // We want "threadScope" to be a new top-level
                // scope, so set its parent scope to null. This
                // means that any variables created by assignments
                // will be properties of "threadScope".
                threadScope.setParentScope(null);

                // Create a JavaScript property of the thread scope named
                // 'x' and save a value for it.
                threadScope.put("x", threadScope, x);
                cx.evaluateString(threadScope, script, "threadScript", 1, null);
            }
            catch (NotAFunctionException jse) {
                // ignore
            }
            catch (PropertyException jse) {
                // ignore
            }
            catch (JavaScriptException jse) {
                // ignore
            }
            finally {
                Context.exit();
            }
        }
        private Scriptable scope;
        private String script;
        private String x;
    }

}

