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
 * Matthias Radestock
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
import java.lang.reflect.*;
import org.mozilla.javascript.*;
import org.mozilla.javascript.tools.ToolErrorReporter;

/**
 * This class provides for sharing functions across multiple threads.
 * This is of particular interest to server applications.
 *
 * @author Norris Boyd
 */
public class Global extends ImporterTopLevel {

    public Global(Context cx) 
    {
        // Define some global functions particular to the shell. Note
        // that these functions are not part of ECMA.
        super(cx);
        String[] names = { "print", "quit", "version", "load", "help",
                           "loadClass", "defineClass", "spawn", "sync",
                           "serialize", "deserialize" };
        try {
            defineFunctionProperties(names, Global.class,
                                           ScriptableObject.DONTENUM);
        } catch (PropertyException e) {
            throw new Error();  // shouldn't occur.
        }
        defineProperty(privateName, this, ScriptableObject.DONTENUM);
        
        // Set up "environment" in the global scope to provide access to the
        // System environment variables.
        Environment.defineClass(this);
        Environment environment = new Environment(this);
        defineProperty("environment", environment,
                       ScriptableObject.DONTENUM);
        
        history = (NativeArray) cx.newArray(this, 0);
        defineProperty("history", history, ScriptableObject.DONTENUM);
    }
    
    /**
     * Print a help message.
     *
     * This method is defined as a JavaScript function.
     */
    public static void help(Context cx, Scriptable thisObj,
                            Object[] args, Function funObj) 
    {
        PrintStream out = getInstance(thisObj).getOut();
        out.println(ToolErrorReporter.getMessage("msg.help"));
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
        PrintStream out = getInstance(thisObj).getOut();
        for (int i=0; i < args.length; i++) {
            if (i > 0)
                out.print(" ");

            // Convert the arbitrary JavaScript value into a string form.
            String s = Context.toString(args[i]);

            out.print(s);
        }
        out.println();
        return Context.getUndefinedValue();
    }
    
    /**
     * Quit the shell.
     *
     * This only affects the interactive mode.
     *
     * This method is defined as a JavaScript function.
     */
    public static void quit(Context cx, Scriptable thisObj,
                            Object[] args, Function funObj)
    {

        System.exit((args.length > 0) ?
                    ((int) Context.toNumber(args[0])) : 0);
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
            Main.processFile(cx, thisObj, cx.toString(args[i]));
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
        ScriptableObject.defineClass(thisObj, clazz);
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
        script.exec(cx, thisObj);
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

    public static void serialize(Context cx, Scriptable thisObj,
                                 Object[] args, Function funObj)
        throws IOException
    {
        if (args.length < 2) {
            throw Context.reportRuntimeError(
                "Expected an object to serialize and a filename to write " +
                "the serialization to");
        }
        Object obj = args[0];
        String filename = cx.toString(args[1]);
        FileOutputStream fos = new FileOutputStream(filename);
        Scriptable scope = ScriptableObject.getTopLevelScope(thisObj);
        ScriptableOutputStream out = new ScriptableOutputStream(fos, scope);
        out.writeObject(obj);
        out.close();
    }

    public static Object deserialize(Context cx, Scriptable thisObj,
                                     Object[] args, Function funObj)
        throws IOException, ClassNotFoundException
    {
        if (args.length < 1) {
            throw Context.reportRuntimeError(
                "Expected a filename to read the serialization from");
        }
        String filename = cx.toString(args[0]); 
        FileInputStream fis = new FileInputStream(filename);
        Scriptable scope = ScriptableObject.getTopLevelScope(thisObj);
        ObjectInputStream in = new ScriptableInputStream(fis, scope);
        Object deserialized = in.readObject();
        in.close();
        return cx.toObject(deserialized, scope);
    }

    /**
     * The spawn function runs a given function or script in a different 
     * thread.
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
        Scriptable scope = funObj.getParentScope();
        Runner runner;
        if (args.length != 0 && args[0] instanceof Function) {
            Object[] newArgs = null;
            if (args.length > 1 && args[1] instanceof Scriptable) {
                newArgs = cx.getElements((Scriptable) args[1]);
            }
            runner = new Runner(scope, (Function) args[0], newArgs);
        } else if (args.length != 0 && args[0] instanceof Script) {
            runner = new Runner(scope, (Script) args[0]);
        } else {
            throw Context.reportRuntimeError(ToolErrorReporter.getMessage(
                "msg.spawn.args"));
        }
        Thread thread = new Thread(runner);
        thread.start();
        return thread;
    }
    
    /**
     * The sync function creates a synchronized function (in the sense
     * of a Java synchronized method) from an existing function. The
     * new function synchronizes on the <code>this</code> object of
     * its invocation.
     * js> var o = { f : sync(function(x) {
     *       print("entry");
     *       Packages.java.lang.Thread.sleep(x*1000);
     *       print("exit");
     *     })};
     * js> spawn(function() {o.f(5);});
     * Thread[Thread-0,5,main]
     * entry
     * js> spawn(function() {o.f(5);});
     * Thread[Thread-1,5,main]
     * js>
     * exit
     * entry
     * exit
     */
    public static Object sync(Context cx, Scriptable thisObj, Object[] args,
                               Function funObj)
    {
        if (args.length == 1 && args[0] instanceof Function) {
            return new Synchronizer((Function)args[0]);
        }
        else {
            throw Context.reportRuntimeError(ToolErrorReporter.getMessage(
                "msg.spawn.args"));
        }
    }
    
    public InputStream getIn() {
        return inStream == null ? System.in : inStream;
    }
    
    public void setIn(InputStream in) {
        inStream = in;
    }

    public PrintStream getOut() {
        return outStream == null ? System.out : outStream;
    }
    
    public void setOut(PrintStream out) {
        outStream = out;
    }

    public PrintStream getErr() { 
        return errStream == null ? System.err : errStream;
    }

    public void setErr(PrintStream err) {
        errStream = err;
    }
        
    static final String privateName = "org.mozilla.javascript.tools.shell.Global private";
    
    public static Global getInstance(Scriptable scope) {
        Object v = ScriptableObject.getProperty(scope,privateName);
        if (v instanceof Global)
            return (Global) v;
        return null;
    }

    NativeArray history;
    public InputStream inStream;
    public PrintStream outStream;
    public PrintStream errStream;
}


class Runner implements Runnable {

    Runner(Scriptable scope, Function func, Object[] args) {
        this.scope = scope;
        f = func;
        this.args = args;
    }

    Runner(Scriptable scope, Script script) {
        this.scope = scope;
        s = script;
    }

    public void run() {
        Context cx = Context.enter();

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

    private Scriptable scope;
    private Function f;
    private Script s;
    private Object[] args;
}
