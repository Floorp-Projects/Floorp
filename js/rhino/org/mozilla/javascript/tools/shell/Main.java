/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Patrick Beard
 * Norris Boyd
 * Kurt Westerfeld
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

package org.mozilla.javascript.tools.shell;

import java.io.*;
import java.util.*;
import java.lang.reflect.*;
import org.mozilla.javascript.*;
import org.mozilla.javascript.tools.ToolErrorReporter;

/*
TODO: integrate debugger; make DebugProxy class so that debugger is
      not required to run the shell.
import org.mozilla.javascript.debug.ILaunchableDebugger;
import org.mozilla.javascript.debug.DebugManager;
import org.mozilla.javascript.debug.SourceTextManagerImpl;
*/

/**
 * The shell program.
 *
 * Can execute scripts interactively or in batch mode at the command line.
 * An example of controlling the JavaScript engine.
 *
 * @author Norris Boyd
 */
public class Main {

    /**
     * Main entry point.
     *
     * Process arguments as would a normal Java program. Also
     * create a new Context and associate it with the current thread.
     * Then set up the execution environment and begin to
     * execute scripts.
     */
    public static void main(String args[]) {
        Context cx = Context.enter();

        errorReporter = new ToolErrorReporter(false, err);
        cx.setErrorReporter(errorReporter);

        // Create the "global" object where top-level variables will live.
        global = new Global();

        args = processOptions(cx, args);

        // Set up "arguments" in the global scope to contain the command
        // line arguments after the name of the script to execute
        Object[] array = args;
        if (args.length > 0) {
            int length = args.length - 1;
            array = new Object[length];
            System.arraycopy(args, 1, array, 0, length);
        }
        Scriptable argsObj = cx.newArray(global, array);
        global.defineProperty("arguments", argsObj,
                              ScriptableObject.DONTENUM);
        
        // Set up "environment" in the global scope to provide access to the
        // System environment variables.
        Environment.defineClass(global);
        Environment environment = new Environment(global);
        global.defineProperty("environment", environment,
                              ScriptableObject.DONTENUM);
        
        global.history = (NativeArray) cx.newArray(global, 0);
        global.defineProperty("history", global.history,
                              ScriptableObject.DONTENUM);
        
        /*
        TODO: enable debugger
        if (global.debug) {

            global.debug_dm = new DebugManager();
            global.debug_stm = new SourceTextManagerImpl();
            global.debug_dm.setSourceTextManager(global.debug_stm);
            cx.setSourceTextManager(global.debug_stm);
            global.debug_dm.createdContext(cx);

            if (global.showDebuggerUI) {
                out.println("Launching JSDebugger...");

                try {
                    Class clazz = Class.forName(
                        "org.mozilla.jsdebugging.ifcui.launcher.rhino.LaunchNetscapeJavaScriptDebugger");
                    ILaunchableDebugger debugger =
                        (ILaunchableDebugger) clazz.newInstance();
                    debugger.launch(global.debug_dm, global.debug_stm, false);

                } catch (Exception e) {
                    // eat it...
                    out.println(e);
                    out.println("Failed to launch the JSDebugger");
                }
            }
            out.println("Debug level set to "+cx.getDebugLevel());
        }
        */

        if (global.processStdin)
            processSource(cx, args.length == 0 ? null : args[0]);

        cx.exit();
    }

    /**
     * Parse arguments.
     */
    public static String[] processOptions(Context cx, String args[]) {
        cx.setTargetPackage("");    // default to no package
        for (int i=0; i < args.length; i++) {
            String arg = args[i];
            if (!arg.startsWith("-")) {
                String[] result = new String[args.length - i];
                for (int j=i; j < args.length; j++)
                    result[j-i] = args[j];
                return result;
            }
            if (arg.equals("-version")) {
                if (++i == args.length)
                    usage(arg);
                double d = cx.toNumber(args[i]);
                if (d != d)
                    usage(arg);
                cx.setLanguageVersion((int) d);
                continue;
            }
            if (arg.equals("-opt") || arg.equals("-O")) {
                if (++i == args.length)
                    usage(arg);
                double d = cx.toNumber(args[i]);
                if (d != d)
                    usage(arg);
                cx.setOptimizationLevel((int)d);
                continue;
            }
            if (arg.equals("-e")) {
                global.processStdin = false;
                if (++i == args.length)
                    usage(arg);
                try {
                    cx.evaluateString(global, args[i], "command line", 1, 
                                      null);
                }
                catch (EvaluatorException ee) {
                    break;
                }
                catch (JavaScriptException jse) {
                    Context.reportError(ToolErrorReporter.getMessage(
                        "msg.uncaughtJSException",
                        jse.getMessage()));
                    break;
                }
                continue;
            }
            if (arg.equals("-w")) {
                errorReporter.setIsReportingWarnings(true);
                continue;
            }
            if (arg.equals("-f")) {
                if (++i == args.length)
                    usage(arg);
                if (args[i].equals("-"))
                    global.processStdin = false;
                processSource(cx, args[i]);
                continue;
            }
            if (arg.startsWith("-debug")) {

                int level = 9;
                if (i+1 < args.length) {
                    double d = cx.toNumber(args[i+1]);
                    if (d == d) {
                        level = (int)d;
                        i++;
                    }
                }
                if (arg.equals("-debug"))
                    global.showDebuggerUI = true;
                else if (arg.equals("-debugQ"))
                    global.showDebuggerUI = false;
                else
                    usage(arg);

                cx.setDebugLevel(level);
                global.debug = true;
                continue;
            }
            usage(arg);
        }
        return new String[0];
    }

    /**
     * Print a usage message.
     */
    public static void usage(String s) {
        p(ToolErrorReporter.getMessage("msg.shell.usage", s));
        System.exit(1);
    }

    /**
     * Evaluate JavaScript source.
     *
     * @param cx the current context
     * @param filename the name of the file to compile, or null
     *                 for interactive mode.
     */
    public static void processSource(Context cx, String filename) {
        SourceTextManager stm = cx.getSourceTextManager();
        if (filename == null || filename.equals("-")) {
            BufferedReader in = new BufferedReader
                (new InputStreamReader(Main.in));
            if(null != stm)
                in = new DebugReader(in, stm, "<stdin>");           
            int lineno = 1;
            boolean hitEOF = false;
            while (!hitEOF && !global.quitting) {
                int startline = lineno;
                if (filename == null)
                    err.print("js> ");
                err.flush();
                try {
                    String source = "";
                    
                    // Collect lines of source to compile.
                    while(true) {
                        String newline;
                        newline = in.readLine();
                        if (newline == null) {
                            hitEOF = true;
                            break;
                        }
                        source = source + newline + "\n";
                        lineno++;
                        if (cx.stringIsCompilableUnit(source))
                            break;
                    }
                    Object result = cx.evaluateString(global, source,
                                                      "<stdin>", startline,
                                                      null);
                    if (result != cx.getUndefinedValue()) {
                        err.println(cx.toString(result));
                    }
                    NativeArray h = global.history;
                    h.put((int)h.jsGet_length(), h, source);
                }
                catch (WrappedException we) {
                    // Some form of exception was caught by JavaScript and
                    // propagated up.
                    err.println(we.getWrappedException().toString());
                    we.printStackTrace();
                }
                catch (EvaluatorException ee) {
                    // Some form of JavaScript error.
                    // Already printed message, so just fall through.
                }
                catch (EcmaError ee) {
                    Context.reportError(ToolErrorReporter.getMessage(
                        "msg.uncaughtJSException",
                        ee.toString()));
                }
                catch (JavaScriptException jse) {
                	// Need to propagate ThreadDeath exceptions.
                	Object value = jse.getValue();
                	if (value instanceof ThreadDeath)
                		throw (ThreadDeath) value;
                    Context.reportError(ToolErrorReporter.getMessage(
                        "msg.uncaughtJSException",
                        jse.getMessage()));
                }
                catch (IOException ioe) {
                    err.println(ioe.toString());
                }
                if (global.quitting) {
                    // The user executed the quit() function.
                    break;
                }
            }
            err.println();
        } else {
            Reader in = null;
            try {
                in = new PushbackReader(new FileReader(filename));
                int c = in.read();
                // Support the executable script #! syntax:  If
                // the first line begins with a '#', treat the whole
                // line as a comment.
                if (c == '#') {
                    while ((c = in.read()) != -1) {
                        if (c == '\n' || c == '\r')
                            break;
                    }
                    ((PushbackReader) in).unread(c);
                } else {
                    // No '#' line, just reopen the file and forget it
                    // ever happened.  OPT closing and reopening
                    // undoubtedly carries some cost.  Is this faster
                    // or slower than leaving the PushbackReader
                    // around?
                    in.close();
                    in = new FileReader(filename);
                }
                if(null != stm)
                    in = new DebugReader(in, stm, filename);           
            }
            catch (FileNotFoundException ex) {
                Context.reportError(ToolErrorReporter.getMessage(
                    "msg.couldnt.open",
                    filename));
                return;
            } catch (IOException ioe) {
                err.println(ioe.toString());
            }

            try {
                // Here we evalute the entire contents of the file as
                // a script. Text is printed only if the print() function
                // is called.
                cx.evaluateReader(global, in, filename, 1, null);
            }
            catch (WrappedException we) {
                err.println(we.getWrappedException().toString());
                we.printStackTrace();
            }
            catch (EcmaError ee) {
                if (ee.getSourceName() != null) {
                    Object[] args = { ee.getSourceName(), 
                                      new Integer(ee.getLineNumber()),
                                      ee.toString() };
                    Context.reportError(ToolErrorReporter.getMessage(
                        "msg.uncaughtJSExceptionLine", args));
                } else {
                    Context.reportError(ToolErrorReporter.getMessage(
                        "msg.uncaughtJSException",
                        ee.toString()));
                }
            }
            catch (EvaluatorException ee) {
                // Already printed message, so just fall through.
            }
            catch (JavaScriptException jse) {
	        	// Need to propagate ThreadDeath exceptions.
	        	Object value = jse.getValue();
	        	if (value instanceof ThreadDeath)
	        		throw (ThreadDeath) value;
                Context.reportError(ToolErrorReporter.getMessage(
                    "msg.uncaughtJSException",
                    jse.getMessage()));
            }
            catch (IOException ioe) {
                err.println(ioe.toString());
            }
            finally {
                try {
                    in.close();
                }
                catch (IOException ioe) {
                    err.println(ioe.toString());
                }
            }
        }
        System.gc();
    }

    private static void p(String s) {
        out.println(s);
    }

    public static void setIn(InputStream _in) {
        in = _in;
    }

    public static void setOut(PrintStream _out) {
        out = _out;
    }

    public static void setErr(PrintStream _err) {
        err = _err;
    }

    static protected ToolErrorReporter errorReporter;
    static protected Global global;
    static public InputStream in = System.in;
    static public PrintStream out = System.out;
    static public PrintStream err = System.err;
    
    SourceTextManager debug_stm;
    //DebugManager debug_dm; // TODO: enable debugger
}
