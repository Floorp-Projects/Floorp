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

    static boolean useDynamicScope;

    static class MyFactory extends ContextFactory
    {
        protected boolean hasFeature(Context cx, int featureIndex)
        {
            if (featureIndex == Context.FEATURE_DYNAMIC_SCOPE) {
                return useDynamicScope;
            }
            return super.hasFeature(cx, featureIndex);
        }
    }

    static {
        ContextFactory.initGlobal(new MyFactory());
    }


    /**
     * Main entry point.
     *
     * Set up the shared scope and then spawn new threads that execute
     * relative to that shared scope. Try to run functions with and
     * without dynamic scope to see the effect.
     *
     * The expected output is
     * <pre>
     * sharedScope
     * nested:sharedScope
     * sharedScope
     * nested:sharedScope
     * sharedScope
     * nested:sharedScope
     * thread0
     * nested:thread0
     * thread1
     * nested:thread1
     * thread2
     * nested:thread2
     * </pre>
     * The final three lines may be permuted in any order depending on
     * thread scheduling.
     */
    public static void main(String[] args)
    {
        Context cx = Context.enter();
        try {
            // Precompile source only once
            String source = ""
                            +"var x = 'sharedScope';\n"
                            +"function f() { return x; }\n"
                            // Dynamic scope works with nested function too
                            +"function initClosure(prefix) {\n"
                            +"    return function test() { return prefix+x; }\n"
                            +"}\n"
                            +"var closure = initClosure('nested:');\n"
                            +"";
            Script script = cx.compileString(source, "sharedScript", 1, null);

            useDynamicScope = false;
            runScripts(cx, script);
            useDynamicScope = true;
            runScripts(cx, script);
        } finally {
            cx.exit();
        }
    }

    static void runScripts(Context cx, Script script)
    {
        // Initialize the standard objects (Object, Function, etc.)
        // This must be done before scripts can be executed. The call
        // returns a new scope that we will share.
        ScriptableObject sharedScope = cx.initStandardObjects(null, true);

        // Now we can execute the precompiled script against the scope
        // to define x variable and f function in the shared scope.
        script.exec(cx, sharedScope);

        // Now we spawn some threads that execute a script that calls the
        // function 'f'. The scope chain looks like this:
        // <pre>
        //            ------------------                ------------------
        //           | per-thread scope | -prototype-> |   shared scope   |
        //            ------------------                ------------------
        //                    ^
        //                    |
        //               parentScope
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
            String source2 = ""
                +"function g() { var x = 'local'; return f(); }\n"
                +"java.lang.System.out.println(g());\n"
                +"function g2() { var x = 'local'; return closure(); }\n"
                +"java.lang.System.out.println(g2());\n"
                +"";
            t[i] = new Thread(new PerThread(sharedScope, source2,
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

        PerThread(Scriptable sharedScope, String source, String x) {
            this.sharedScope = sharedScope;
            this.source = source;
            this.x = x;
        }

        public void run() {
            // We need a new Context for this thread.
            Context cx = Context.enter();
            try {
                // We can share the scope.
                Scriptable threadScope = cx.newObject(sharedScope);
                threadScope.setPrototype(sharedScope);

                // We want "threadScope" to be a new top-level
                // scope, so set its parent scope to null. This
                // means that any variables created by assignments
                // will be properties of "threadScope".
                threadScope.setParentScope(null);

                // Create a JavaScript property of the thread scope named
                // 'x' and save a value for it.
                threadScope.put("x", threadScope, x);
                cx.evaluateString(threadScope, source, "threadScript", 1, null);
            }
            catch (JavaScriptException jse) {
                // ignore
            }
            finally {
                Context.exit();
            }
        }
        private Scriptable sharedScope;
        private String source;
        private String x;
    }

}

