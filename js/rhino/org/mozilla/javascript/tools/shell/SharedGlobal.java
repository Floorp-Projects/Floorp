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
import java.lang.reflect.*;
import org.mozilla.javascript.*;
import org.mozilla.javascript.tools.ToolErrorReporter;

/**
 * This class provides for sharing functions across multiple threads.
 * This is of particular interest to server applications.
 *
 * @author Norris Boyd
 */
public class SharedGlobal extends ImporterTopLevel {

    /**
     * Initialize new SharedGlobal object.
     */
    public SharedGlobal() {
        Context cx = Context.enter();
        
        // Initialize the standard objects (Object, Function, etc.)
        // This must be done before scripts can be executed.
        cx.initStandardObjects(this, false);
		
        // Define some global functions particular to the shell. Note
        // that these functions are not part of ECMA.
        String[] names = { "print", "quit", "version", "load", "help",
                           "loadClass", "defineClass", "spawn" };
        try {
            defineFunctionProperties(names, SharedGlobal.class,
                                     ScriptableObject.DONTENUM);
        } catch (PropertyException e) {
            throw new Error(e.getMessage());
        } finally {
            Context.exit();
        }   
    }

    /**
     * Print a help message.
     *
     * This method is defined as a JavaScript function.
     */
    public static void help(String s) {
        System.out.println(ToolErrorReporter.getMessage("msg.help"));
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
        Main.global.quitting = true;
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
            Main.processSource(cx, cx.toString(args[i]));
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
        ScriptableObject.defineClass(Main.global, clazz);
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
        script.exec(cx, Main.global);
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

    /**
     * The spawn function runs a given function or script in a different 
     * thread with a different set of globals. It does share the properties
     * of the SharedGlobal object.
     * 
     * js> function g() { a = 7; }
     * js> a = 3;
     * 3
     * js> spawn(g)
     * Thread[Thread-1,5,main]
     * js> a
     * 3
     */
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
