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
 * Rob Ginda
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
        int result = exec(args);
        if (result != 0)
            System.exit(result);
    }
    
    /**
     *  Execute the given arguments, but don't System.exit at the end.
     */
    public static int exec(String args[]) {
        Context cx = Context.enter();
        // Create the top-level scope object.
        global = new Global(cx);
        errorReporter = new ToolErrorReporter(false, global.getErr());
        cx.setErrorReporter(errorReporter);

        args = processOptions(cx, args);

        // get the command line arguments after the name of the script,
        // and define "arguments" array in the top-level object
        Object[] array = args;
        if (args.length > 0) {
            int length = args.length - 1;
            array = new Object[length];
            System.arraycopy(args, 1, array, 0, length);
        }
        Scriptable argsObj = cx.newArray(global, array);
        global.defineProperty("arguments", argsObj,
                             ScriptableObject.DONTENUM);
        
        if (processStdin)
            processSource(cx, args.length == 0 ? null : args[0]);

        cx.exit();
        return exitCode;
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
                processStdin = false;
                if (++i == args.length)
                    usage(arg);
                Reader reader = new StringReader(args[i]);
                evaluateReader(cx, global, reader, "<command>", 1);
                continue;
            }
            if (arg.equals("-w")) {
                errorReporter.setIsReportingWarnings(true);
                continue;
            }
            if (arg.equals("-f")) {
                processStdin = false;
                if (++i == args.length)
                    usage(arg);
                if (args[i].equals("-"))
                    processStdin = false;
                processSource(cx, args[i]);
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
        if (filename == null || filename.equals("-")) {
            // Use the interpreter for interactive input
            cx.setOptimizationLevel(-1);
            
            BufferedReader in = new BufferedReader
                (new InputStreamReader(global.getIn()));
            int lineno = 1;
            boolean hitEOF = false;
            while (!hitEOF) {
                int startline = lineno;
                if (filename == null)
                    global.getErr().print("js> ");
                global.getErr().flush();
                String source = "";
                    
                // Collect lines of source to compile.
                while (true) {
                    String newline;
                    try {
                        newline = in.readLine();
                    }
                    catch (IOException ioe) {
                        global.getErr().println(ioe.toString());
                        break;
                    }
                    if (newline == null) {
                        hitEOF = true;
                        break;
                    }
                    source = source + newline + "\n";
                    lineno++;
                    if (cx.stringIsCompilableUnit(source))
                        break;
                }
                Reader reader = new StringReader(source);
                Object result = evaluateReader(cx, global, reader, 
                                               "<stdin>", startline);
                if (result != cx.getUndefinedValue()) {
                    try {
                        global.getErr().println(cx.toString(result));
                    } catch (EcmaError ee) {
                        String msg = ToolErrorReporter.getMessage(
                            "msg.uncaughtJSException", ee.toString());
                        exitCode = EXITCODE_RUNTIME_ERROR;
                        if (ee.getSourceName() != null) {
                            Context.reportError(msg, ee.getSourceName(), 
                                                ee.getLineNumber(), 
                                                ee.getLineSource(), 
                                                ee.getColumnNumber());
                        } else {
                            Context.reportError(msg);
                        }
                    }
                }
                NativeArray h = global.history;
                h.put((int)h.jsGet_length(), h, source);
            }
            global.getErr().println();
        } else processFile(cx, global, filename);
        System.gc();
    }
    
    public static void processFile(Context cx, Scriptable scope,
                                   String filename)
    {
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
                filename = new java.io.File(filename).getCanonicalPath();
            }
            catch (FileNotFoundException ex) {
                Context.reportError(ToolErrorReporter.getMessage(
                    "msg.couldnt.open",
                    filename));
                exitCode = EXITCODE_FILE_NOT_FOUND;
                return;
            } catch (IOException ioe) {
                global.getErr().println(ioe.toString());
            }
            
            // Here we evalute the entire contents of the file as
            // a script. Text is printed only if the print() function
            // is called.
            evaluateReader(cx, scope, in, filename, 1);
    }

    public static Object evaluateReader(Context cx, Scriptable scope, 
                                        Reader in, String sourceName, 
                                        int lineno)
    {
        Object result = cx.getUndefinedValue();
        try {
            result = cx.evaluateReader(scope, in, sourceName, lineno, null);
        }
        catch (WrappedException we) {
            global.getErr().println(we.getWrappedException().toString());
            we.printStackTrace();
        }
        catch (EcmaError ee) {
            String msg = ToolErrorReporter.getMessage(
                "msg.uncaughtJSException", ee.toString());
            exitCode = EXITCODE_RUNTIME_ERROR;
            if (ee.getSourceName() != null) {
                Context.reportError(msg, ee.getSourceName(), 
                                    ee.getLineNumber(), 
                                    ee.getLineSource(), 
                                    ee.getColumnNumber());
            } else {
                Context.reportError(msg);
            }
        }
        catch (EvaluatorException ee) {
            // Already printed message.
            exitCode = EXITCODE_RUNTIME_ERROR;
        }
        catch (JavaScriptException jse) {
	    	// Need to propagate ThreadDeath exceptions.
	    	Object value = jse.getValue();
	    	if (value instanceof ThreadDeath)
	    		throw (ThreadDeath) value;
            exitCode = EXITCODE_RUNTIME_ERROR;
            Context.reportError(ToolErrorReporter.getMessage(
                "msg.uncaughtJSException",
                jse.getMessage()));
        }
        catch (IOException ioe) {
            global.getErr().println(ioe.toString());
        }
        finally {
            try {
                in.close();
            }
            catch (IOException ioe) {
                global.getErr().println(ioe.toString());
            }
        }
        return result;
    }

    private static void p(String s) {
        global.getOut().println(s);
    }
    
    public static ScriptableObject getScope() {
        return global;
    }

    public static InputStream getIn() {
        return Global.getInstance(global).getIn();
    }
    
    public static void setIn(InputStream in) {
        Global.getInstance(global).setIn(in);
    }

    public static PrintStream getOut() {
        return Global.getInstance(global).getOut();
    }
    
    public static void setOut(PrintStream out) {
        Global.getInstance(global).setOut(out);
    }

    public static PrintStream getErr() { 
        return Global.getInstance(global).getErr();
    }

    public static void setErr(PrintStream err) {
        Global.getInstance(global).setErr(err);
    }

    static protected ToolErrorReporter errorReporter;
    static protected Global global;
    static protected int exitCode = 0;
    static private final int EXITCODE_RUNTIME_ERROR = 3;
    static private final int EXITCODE_FILE_NOT_FOUND = 4;
    //static private DebugShell debugShell;
    static boolean processStdin = true;
}
