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

package org.mozilla.javascript.tools.shell;

import java.io.*;
import java.util.*;
import java.lang.reflect.*;
import org.mozilla.javascript.*;
import java.text.MessageFormat;
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
 
 
public class Main extends ScriptableObject {

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

        errorReporter = new ToolErrorReporter(false);
        cx.setErrorReporter(errorReporter);

        // A bit of shorthand: since Main extends ScriptableObject,
        // we can make it the global object.
        // We create a shared global with sealed properties that
        // we can share with other threads (see spawn).
        sharedGlobal = new Main();

        // Initialize the standard objects (Object, Function, etc.)
        // This must be done before scripts can be executed.
        cx.initStandardObjects(sharedGlobal, true);
        
		
        // Define some global functions particular to the shell. Note
        // that these functions are not part of ECMA.
        String[] names = { "print", "quit", "version", "load", "help",
                           "loadClass", "defineClass", "spawn" };
        try {
            sharedGlobal.defineFunctionProperties(names, Main.class,
                                                  ScriptableObject.DONTENUM);
        } catch (PropertyException e) {
            throw new Error(e.getMessage());
        }
        
        // Create the "global" object where top-level variables will live.
        // Set its prototype to the sharedGlobal where all the standard
        // functions and objects live.
        global = new Main();
        global.setPrototype(sharedGlobal);

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
                       
        /*
        TODO: enable debugger
        if (global.debug) {

            global.debug_dm = new DebugManager();
            global.debug_stm = new SourceTextManagerImpl();
            global.debug_dm.setSourceTextManager(global.debug_stm);
            cx.setSourceTextManager(global.debug_stm);
            global.debug_dm.createdContext(cx);

            if (global.showDebuggerUI) {
                System.out.println("Launching JSDebugger...");

                try {
                    Class clazz = Class.forName(
                        "com.netscape.jsdebugging.ifcui.launcher.rhino.LaunchNetscapeJavaScriptDebugger");
                    ILaunchableDebugger debugger =
                        (ILaunchableDebugger) clazz.newInstance();
                    debugger.launch(global.debug_dm, global.debug_stm, false);

                } catch (Exception e) {
                    // eat it...
                    System.out.println(e);
                    System.out.println("Failed to launch the JSDebugger");
                }
            }
            System.out.println("Debug level set to "+cx.getDebugLevel());
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
     * Return name of this class, the global object.
     *
     * This method must be implemented in all concrete classes
     * extending ScriptableObject.
     *
     * @see org.mozilla.javascript.Scriptable#getClassName
     */
    public String getClassName() {
        return "global";
    }

    /**
     * Print a usage message.
     */
    public static void usage(String s) {
        p(ToolErrorReporter.getMessage("msg.shell.usage", s));
        System.exit(1);
    }

    /**
     * Print a help message.
     *
     * This method is defined as a JavaScript function.
     */
    public static void help(String s) {
        p(ToolErrorReporter.getMessage("msg.help"));
    }

    /**
     * Print the string values of its arguments.
     *
     * This method is defined as a JavaScript function.
     * Note that its arguments are of the "varargs" form, which
     * allows it to handle an arbitrary number of arguments
     * supplied to the JavaScript function.
     *
     */
    public static Object print(Context cx, Scriptable thisObj,
                               Object[] args, Function funObj)
    {
        for (int i=0; i < args.length; i++) {
            if (i > 0)
                System.out.print(" ");

            // Convert the
            // arbitrary JavaScript value into a string form.

            String s = Context.toString(args[i]);

            System.out.print(s);
        }
        System.out.println();
        return Context.getUndefinedValue();
    }

    /**
     * Quit the shell.
     *
     * This only affects the interactive mode.
     *
     * This method is defined as a JavaScript function.
     */
    public static void quit() {
        global.quitting = true;
    }

    /**
     * Get and set the language version.
     *
     * This method is defined as a JavaScript function.
     */
    public static double version(Context cx, Scriptable thisObj,
                                 Object[] args, Function funObj)
    {
        double result = (double) cx.getLanguageVersion();
        if (args.length > 0) {
            double d = cx.toNumber(args[0]);
            cx.setLanguageVersion((int) d);
        }
        return result;
    }

    /**
     * Load and execute a set of JavaScript source files.
     *
     * This method is defined as a JavaScript function.
     *
     */
    public static void load(Context cx, Scriptable thisObj,
                            Object[] args, Function funObj)
    {
        for (int i=0; i < args.length; i++) {
            processSource(cx, cx.toString(args[i]));
        }
    }

    /**
     * Load a Java class that defines a JavaScript object using the
     * conventions outlined in ScriptableObject.defineClass.
     * <p>
     * This method is defined as a JavaScript function.
     * @exception IllegalAccessException if access is not available
     *            to a reflected class member
     * @exception InstantiationException if unable to instantiate
     *            the named class
     * @exception InvocationTargetException if an exception is thrown
     *            during execution of methods of the named class
     * @exception ClassDefinitionException if the format of the
     *            class causes this exception in ScriptableObject.defineClass
     * @exception PropertyException if the format of the
     *            class causes this exception in ScriptableObject.defineClass
     * @see org.mozilla.javascript.ScriptableObject#defineClass
     */
    public static void defineClass(Context cx, Scriptable thisObj,
                                   Object[] args, Function funObj)
        throws IllegalAccessException, InstantiationException,
               InvocationTargetException, ClassDefinitionException,
               PropertyException
    {
        Class clazz = getClass(args);
        ScriptableObject.defineClass(global, clazz);
    }

    /**
     * Load and execute a script compiled to a class file.
     * <p>
     * This method is defined as a JavaScript function.
     * When called as a JavaScript function, a single argument is
     * expected. This argument should be the name of a class that
     * implements the Script interface, as will any script
     * compiled by jsc.
     *
     * @exception IllegalAccessException if access is not available
     *            to the class
     * @exception InstantiationException if unable to instantiate
     *            the named class
     * @exception InvocationTargetException if an exception is thrown
     *            during execution of methods of the named class
     * @exception JavaScriptException if a JavaScript exception is thrown
     *            during execution of the compiled script
     * @see org.mozilla.javascript.ScriptableObject#defineClass
     */
    public static void loadClass(Context cx, Scriptable thisObj,
                                 Object[] args, Function funObj)
        throws IllegalAccessException, InstantiationException,
               InvocationTargetException, JavaScriptException
    {
        Class clazz = getClass(args);
        if (!Script.class.isAssignableFrom(clazz)) {
            throw Context.reportRuntimeError(ToolErrorReporter.getMessage(
                "msg.must.implement.Script"));
        }
        Script script = (Script) clazz.newInstance();
        script.exec(cx, global);
    }

    private static Class getClass(Object[] args)
        throws IllegalAccessException, InstantiationException,
               InvocationTargetException
    {
        if (args.length == 0) {
            throw Context.reportRuntimeError(ToolErrorReporter.getMessage(
                "msg.expected.string.arg"));
        }
        String className = Context.toString(args[0]);
        try {
            return Class.forName(className);
        }
        catch (ClassNotFoundException cnfe) {
            throw Context.reportRuntimeError(ToolErrorReporter.getMessage(
                "msg.class.not.found",
                className));
        }
    }

    public static Object spawn(Context cx, Scriptable thisObj, Object[] args, 
                               Function funObj)
    {
        Runner runner;
        if (args.length != 0 && args[0] instanceof Function) {
            Object[] newArgs = null;
            if (args.length > 1 && args[1] instanceof Scriptable) {
                newArgs = cx.getElements((Scriptable) args[1]);
            }
            runner = new Runner((Function) args[0], newArgs);
        } else if (args.length != 0 && args[0] instanceof Script) {
            runner = new Runner((Script) args[0]);
        } else {
            throw Context.reportRuntimeError(ToolErrorReporter.getMessage(
                "msg.spawn.args"));
        }
        Thread thread = new Thread(runner);
        thread.start();
        return thread;
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
                (new InputStreamReader(System.in));
            if(null != stm)
                in = new DebugReader(in, stm, "<stdin>");           
            int lineno = 1;
            boolean hitEOF = false;
            while (!hitEOF && !global.quitting) {
                int startline = lineno;
                if (filename == null)
                    System.err.print("js> ");
                System.err.flush();
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
                        System.err.println(cx.toString(result));
                    }
                }
                catch (WrappedException we) {
                    // Some form of exception was caught by JavaScript and
                    // propagated up.
                    System.err.println(we.getWrappedException().toString());
                    we.printStackTrace();
                }
                catch (EvaluatorException ee) {
                    // Some form of JavaScript error.
                    // Already printed message, so just fall through.
                }
                catch (JavaScriptException jse) {
                    Context.reportError(ToolErrorReporter.getMessage(
                        "msg.uncaughtJSException",
                        jse.getMessage()));
                }
                catch (IOException ioe) {
                    System.err.println(ioe.toString());
                }
                if (global.quitting) {
                    // The user executed the quit() function.
                    break;
                }
            }
            System.err.println();
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
                System.err.println(ioe.toString());
            }

            try {
                // Here we evalute the entire contents of the file as
                // a script. Text is printed only if the print() function
                // is called.
                cx.evaluateReader(global, in, filename, 1, null);
            }
            catch (WrappedException we) {
                System.err.println(we.getWrappedException().toString());
                we.printStackTrace();
            }
            catch (EvaluatorException ee) {
                // Already printed message, so just fall through.
            }
            catch (JavaScriptException jse) {
                Context.reportError(ToolErrorReporter.getMessage(
                    "msg.uncaughtJSException",
                    jse.getMessage()));
            }
            catch (IOException ioe) {
                System.err.println(ioe.toString());
            }
            finally {
                try {
                    in.close();
                }
                catch (IOException ioe) {
                    System.err.println(ioe.toString());
                }
            }
        }
        System.gc();
    }

    private static void p(String s) {
        System.out.println(s);
    }

    static ToolErrorReporter errorReporter;
    static Main global;
    static Main sharedGlobal;
    
    boolean quitting;
    SourceTextManager debug_stm;
    //DebugManager debug_dm; // TODO: enable debugger
    boolean debug = false;
    boolean processStdin = true;
    boolean showDebuggerUI = false;
}


class Runner implements Runnable {

    Runner(Function func, Object[] args) {
        f = func;
        this.args = args;
    }

    Runner(Script script) {
        s = script;
    }

    public void run() {
        Context cx = Context.enter();

        // Create a local scope for this thread to execute in, and use
        // the shared global properties.
        Main scope = new Main();
        scope.setPrototype(Main.sharedGlobal);
        
        try {
            if (f != null)
                f.call(cx, scope, scope, args);
            else
                s.exec(cx, scope);
        } catch (JavaScriptException e) {
            Context.reportError(ToolErrorReporter.getMessage(
                "msg.uncaughtJSException",
                e.getMessage()));
        } 
        
        cx.exit();
    }

    private Function f;
    private Script s;
    private Object[] args;
}
