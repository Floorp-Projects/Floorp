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
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Patrick Beard
 * Igor Bukanov
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
import java.net.*;
import java.lang.reflect.*;
import org.mozilla.javascript.*;
import org.mozilla.javascript.tools.ToolErrorReporter;
import org.mozilla.javascript.serialize.*;

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
        super(cx, Main.sealedStdLib);
        String[] names = { "print", "quit", "version", "load", "help",
                           "loadClass", "defineClass", "spawn", "sync",
                           "serialize", "deserialize", "runCommand",
                           "seal", "readFile", "readUrl" };
        try {
            defineFunctionProperties(names, Global.class,
                                     ScriptableObject.DONTENUM);
        } catch (PropertyException e) {
            throw new Error();  // shouldn't occur.
        }

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
            throw reportRuntimeError("msg.must.implement.Script");
        }
        Script script = (Script) clazz.newInstance();
        script.exec(cx, thisObj);
    }

    private static Class getClass(Object[] args)
        throws IllegalAccessException, InstantiationException,
               InvocationTargetException
    {
        if (args.length == 0) {
            throw reportRuntimeError("msg.expected.string.arg");
        }
        Object arg0 = args[0];
        if (arg0 instanceof Wrapper) {
            Object wrapped = ((Wrapper)arg0).unwrap();
            if (wrapped instanceof Class)
                return (Class)wrapped;
        }
        String className = Context.toString(args[0]);
        try {
            return Class.forName(className);
        }
        catch (ClassNotFoundException cnfe) {
            throw reportRuntimeError("msg.class.not.found", className);
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
            if (newArgs == null) { newArgs = ScriptRuntime.emptyArgs; }
            runner = new Runner(scope, (Function) args[0], newArgs);
        } else if (args.length != 0 && args[0] instanceof Script) {
            runner = new Runner(scope, (Script) args[0]);
        } else {
            throw reportRuntimeError("msg.spawn.args");
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
            throw reportRuntimeError("msg.sync.args");
        }
    }

    /**
     * Execute the specified command with the given argument and options
     * as a separate process and return the exit status of the process.
     * <p>
     * Usage:
     * <pre>
     * runCommand(command)
     * runCommand(command, arg1, ..., argN)
     * runCommand(command, arg1, ..., argN, options)
     * </pre>
     * All except the last arguments to runCommand are converted to strings
     * and denote command name and its arguments. If the last argument is a
     * JavaScript object, it is an option object. Otherwise it is converted to
     * string denoting the last argument and options objects assumed to be
     * empty.
     * Te following properties of the option object are processed:
     * <ul>
     * <li><tt>args</tt> - provides an array of additional command arguments
     * <li><tt>env</tt> - explicit environment object. All its enumeratable
     *   properties define the corresponding environment variable names.
     * <li><tt>input</tt> - the process input. If it is not
     *   java.io.InputStream, it is converted to string and sent to the process
     *   as its input. If not specified, no input is provided to the process.
     * <li><tt>output</tt> - the process output instead of java.lang.System.out.
     *   If it is not instance of java.io.OutputStream, the process output is
     *   read, converted to a string, appended to the output property value
     *   converted to string and put as the new value of the output property.
     * <li><tt>err</tt> - the process error output instead of
     *   java.lang.System.err. If it is not instance of java.io.OutputStream,
     *   the process error output is read, converted to a string, appended to
     *   the err property value converted to string and put as the new
     *   value of the err property.
     * </ul>
     */
    public static Object runCommand(Context cx, Scriptable thisObj,
                                    Object[] args, Function funObj)
        throws IOException
    {
        int L = args.length;
        if (L == 0 || (L == 1 && args[0] instanceof Scriptable)) {
            throw reportRuntimeError("msg.runCommand.bad.args");
        }

        InputStream in = null;
        OutputStream out = null, err = null;
        ByteArrayOutputStream outBytes = null, errBytes = null;
        Object outObj = null, errObj = null;
        String[] environment = null;
        Scriptable params = null;
        Object[] addArgs = null;
        if (args[L - 1] instanceof Scriptable) {
            params = (Scriptable)args[L - 1];
            --L;
            Object envObj = ScriptableObject.getProperty(params, "env");
            if (envObj != Scriptable.NOT_FOUND) {
                if (envObj == null) {
                    environment = new String[0];
                } else {
                    if (!(envObj instanceof Scriptable)) {
                        throw reportRuntimeError("msg.runCommand.bad.env");
                    }
                    Scriptable envHash = (Scriptable)envObj;
                    Object[] ids = ScriptableObject.getPropertyIds(envHash);
                    environment = new String[ids.length];
                    for (int i = 0; i != ids.length; ++i) {
                        Object keyObj = ids[i], val;
                        String key;
                        if (keyObj instanceof String) {
                            key = (String)keyObj;
                            val = ScriptableObject.getProperty(envHash, key);
                        } else {
                            int ikey = ((Number)keyObj).intValue();
                            key = Integer.toString(ikey);
                            val = ScriptableObject.getProperty(envHash, ikey);
                        }
                        if (val == ScriptableObject.NOT_FOUND) {
                            val = Undefined.instance;
                        }
                        environment[i] = key+'='+ScriptRuntime.toString(val);
                    }
                }
            }
            Object inObj = ScriptableObject.getProperty(params, "input");
            if (inObj != Scriptable.NOT_FOUND) {
                in = toInputStream(inObj);
            }
            outObj = ScriptableObject.getProperty(params, "output");
            if (outObj != Scriptable.NOT_FOUND) {
                out = toOutputStream(outObj);
                if (out == null) {
                    outBytes = new ByteArrayOutputStream();
                    out = outBytes;
                }
            }
            errObj = ScriptableObject.getProperty(params, "err");
            if (errObj != Scriptable.NOT_FOUND) {
                err = toOutputStream(errObj);
                if (err == null) {
                    errBytes = new ByteArrayOutputStream();
                    err = errBytes;
                }
            }
            Object addArgsObj = ScriptableObject.getProperty(params, "args");
            if (addArgsObj != Scriptable.NOT_FOUND) {
                Scriptable s = cx.toObject(addArgsObj,
                                           getTopLevelScope(thisObj));
                addArgs = cx.getElements(s);
            }
        }
        Global global = getInstance(thisObj);
        if (out == null) {
            out = (global != null) ? global.getOut() : System.out;
        }
        if (err == null) {
            err = (global != null) ? global.getErr() : System.err;
        }
        // If no explicit input stream, do not send any input to process,
        // in particular, do not use System.in to avoid deadlocks
        // when waiting for user input to send to process which is already
        // terminated as it is not always possible to interrupt read method.

        String[] cmd = new String[(addArgs == null) ? L : L + addArgs.length];
        for (int i = 0; i != L; ++i) {
            cmd[i] = ScriptRuntime.toString(args[i]);
        }
        if (addArgs != null) {
            for (int i = 0; i != addArgs.length; ++i) {
                cmd[L + i] = ScriptRuntime.toString(addArgs[i]);
            }
        }

        int exitCode = runProcess(cmd, environment, in, out, err);
        if (outBytes != null) {
            String s = ScriptRuntime.toString(outObj) + outBytes.toString();
            ScriptableObject.putProperty(params, "output", s);
        }
        if (errBytes != null) {
            String s = ScriptRuntime.toString(errObj) + errBytes.toString();
            ScriptableObject.putProperty(params, "err", s);
        }

        return new Integer(exitCode);
    }

    /**
     * The seal function seals all supplied arguments.
     */
    public static void seal(Context cx, Scriptable thisObj, Object[] args,
                            Function funObj)
    {
        for (int i = 0; i != args.length; ++i) {
            Object arg = args[i];
            if (!(arg instanceof ScriptableObject) || arg == Undefined.instance)
            {
                if (!(arg instanceof Scriptable) || arg == Undefined.instance)
                {
                    throw reportRuntimeError("msg.shell.seal.not.object");
                } else {
                    throw reportRuntimeError("msg.shell.seal.not.scriptable");
                }
            }
        }

        for (int i = 0; i != args.length; ++i) {
            Object arg = args[i];
            ((ScriptableObject)arg).sealObject();
        }
    }

    /**
     * The readFile reads the given file context and convert it to a string
     * using the specified character coding or default character coding if
     * explicit coding argument is not given.
     * <p>
     * Usage:
     * <pre>
     * readFile(filePath)
     * readFile(filePath, charCoding)
     * </pre>
     * The first form converts file's context to string using the default
     * character coding.
     */
    public static Object readFile(Context cx, Scriptable thisObj, Object[] args,
                                  Function funObj)
        throws IOException
    {
        if (args.length == 0) {
            throw reportRuntimeError("msg.shell.readFile.bad.args");
        }
        String path = ScriptRuntime.toString(args[0]);
        String charCoding = null;
        if (args.length >= 2) {
            charCoding = ScriptRuntime.toString(args[1]);
        }

        return readUrl(path, charCoding, true);
    }

    /**
     * The readUrl opens connection to the given URL, read all its data
     * and converts them to a string
     * using the specified character coding or default character coding if
     * explicit coding argument is not given.
     * <p>
     * Usage:
     * <pre>
     * readUrl(url)
     * readUrl(url, charCoding)
     * </pre>
     * The first form converts file's context to string using the default
     * charCoding.
     */
    public static Object readUrl(Context cx, Scriptable thisObj, Object[] args,
                                 Function funObj)
        throws IOException
    {
        if (args.length == 0) {
            throw reportRuntimeError("msg.shell.readUrl.bad.args");
        }
        String url = ScriptRuntime.toString(args[0]);
        String charCoding = null;
        if (args.length >= 2) {
            charCoding = ScriptRuntime.toString(args[1]);
        }

        return readUrl(url, charCoding, false);
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

    public static Global getInstance(Scriptable scope)
    {
        scope = ScriptableObject.getTopLevelScope(scope);
        do {
            if (scope instanceof Global) {
                return (Global)scope;
            }
            scope = scope.getPrototype();
        } while (scope != null);

        return null;
    }

    /**
     * If any of in, out, err is null, the corresponding process stream will
     * be closed immediately, otherwise it will be closed as soon as
     * all data will be read from/written to process
     */
    private static int runProcess(String[] cmd, String[] environment,
                                  InputStream in, OutputStream out,
                                  OutputStream err)
        throws IOException
    {
        Process p;
        if (environment == null) {
            p = Runtime.getRuntime().exec(cmd);
        } else {
            p = Runtime.getRuntime().exec(cmd, environment);
        }
        PipeThread inThread = null, errThread = null;
        try {
            InputStream errProcess = null;
            try {
                if (err != null) {
                    errProcess = p.getErrorStream();
                } else {
                    p.getErrorStream().close();
                }
                InputStream outProcess = null;
                try {
                    if (out != null) {
                        outProcess = p.getInputStream();
                    } else {
                        p.getInputStream().close();
                    }
                    OutputStream inProcess = null;
                    try {
                        if (in != null) {
                            inProcess = p.getOutputStream();
                        } else {
                            p.getOutputStream().close();
                        }

                        if (out != null) {
                            // Read process output on this thread
                            if (err != null) {
                                errThread = new PipeThread(true, errProcess,
                                                           err);
                                errThread.start();
                            }
                            if (in != null) {
                                inThread = new PipeThread(false, in,
                                                          inProcess);
                                inThread.start();
                            }
                            pipe(true, outProcess, out);
                        } else if (in != null) {
                            // No output, read process input on this thread
                            if (err != null) {
                                errThread = new PipeThread(true, errProcess,
                                                           err);
                                errThread.start();
                            }
                            pipe(false, in, inProcess);
                            in.close();
                        } else if (err != null) {
                            // No output or input, read process err
                            // on this thread
                            pipe(true, errProcess, err);
                            errProcess.close();
                            errProcess = null;
                        }

                        // wait for process completion
                        for (;;) {
                            try { p.waitFor(); break; }
                            catch (InterruptedException ex) { }
                        }

                        return p.exitValue();
                    } finally {
                        // pipe will close stream as well, but for reliability
                        // duplicate it in any case
                        if (inProcess != null) {
                            inProcess.close();
                        }
                    }
                } finally {
                    if (outProcess != null) {
                        outProcess.close();
                    }
                }
            } finally {
                if (errProcess != null) {
                    errProcess.close();
                }
            }
        } finally {
            p.destroy();
            if (inThread != null) {
                for (;;) {
                    try { inThread.join(); break; }
                    catch (InterruptedException ex) { }
                }
            }
            if (errThread != null) {
                for (;;) {
                    try { errThread.join(); break; }
                    catch (InterruptedException ex) { }
                }
            }
        }
    }

    static void pipe(boolean fromProcess, InputStream from, OutputStream to)
        throws IOException
    {
        try {
            final int SIZE = 4096;
            byte[] buffer = new byte[SIZE];
            for (;;) {
                int n;
                if (!fromProcess) {
                    n = from.read(buffer, 0, SIZE);
                } else {
                    try {
                        n = from.read(buffer, 0, SIZE);
                    } catch (IOException ex) {
                        // Ignore exception as it can be cause by closed pipe
                        break;
                    }
                }
                if (n < 0) { break; }
                if (fromProcess) {
                    to.write(buffer, 0, n);
                    to.flush();
                } else {
                    try {
                        to.write(buffer, 0, n);
                        to.flush();
                    } catch (IOException ex) {
                        // Ignore exception as it can be cause by closed pipe
                        break;
                    }
                }
            }
        } finally {
            if (fromProcess) {
                from.close();
            } else {
                to.close();
            }
        }
    }

    private static InputStream toInputStream(Object value)
        throws IOException
    {
        InputStream is = null;
        String s = null;
        if (value instanceof Wrapper) {
            Object unwrapped = ((Wrapper)value).unwrap();
            if (unwrapped instanceof InputStream) {
                is = (InputStream)unwrapped;
            } else if (unwrapped instanceof byte[]) {
                is = new ByteArrayInputStream((byte[])unwrapped);
            } else if (unwrapped instanceof Reader) {
                s = readReader((Reader)unwrapped);
            } else if (unwrapped instanceof char[]) {
                s = new String((char[])unwrapped);
            }
        }
        if (is == null) {
            if (s == null) { s = ScriptRuntime.toString(value); }
            is = new ByteArrayInputStream(s.getBytes());
        }
        return is;
    }

    private static OutputStream toOutputStream(Object value) {
        OutputStream os = null;
        if (value instanceof Wrapper) {
            Object unwrapped = ((Wrapper)value).unwrap();
            if (unwrapped instanceof OutputStream) {
                os = (OutputStream)unwrapped;
            }
        }
        return os;
    }

    private static String readUrl(String filePath, String charCoding,
                                  boolean urlIsFile)
        throws IOException
    {
        int chunkLength;
        InputStream is = null;
        try {
            if (!urlIsFile) {
                URL urlObj = new URL(filePath);
                URLConnection uc = urlObj.openConnection();
                is = uc.getInputStream();
                chunkLength = uc.getContentLength();
                if (chunkLength <= 0)
                    chunkLength = 1024;
                if (charCoding == null) {
                    String type = uc.getContentType();
                    if (type != null) {
                        charCoding = getCharCodingFromType(type);
                    }
                }
            } else {
                File f = new File(filePath);

                long length = f.length();
                chunkLength = (int)length;
                if (chunkLength != length)
                    throw new IOException("Too big file size: "+length);

                if (chunkLength == 0) { return ""; }

                is = new FileInputStream(f);
            }

            Reader r;
            if (charCoding == null) {
                r = new InputStreamReader(is);
            } else {
                r = new InputStreamReader(is, charCoding);
            }
            return readReader(r, chunkLength);

        } finally {
            if (is != null)
                is.close();
        }
    }

    private static String getCharCodingFromType(String type)
    {
        int i = type.indexOf(';');
        if (i >= 0) {
            int end = type.length();
            ++i;
            while (i != end && type.charAt(i) <= ' ') {
                ++i;
            }
            String charset = "charset";
            if (charset.regionMatches(true, 0, type, i, charset.length()))
            {
                i += charset.length();
                while (i != end && type.charAt(i) <= ' ') {
                    ++i;
                }
                if (i != end && type.charAt(i) == '=') {
                    ++i;
                    while (i != end && type.charAt(i) <= ' ') {
                        ++i;
                    }
                    if (i != end) {
                        // i is at the start of non-empty
                        // charCoding spec
                        while (type.charAt(end -1) <= ' ') {
                            --end;
                        }
                        return type.substring(i, end);
                    }
                }
            }
        }
        return null;
    }

    private static String readReader(Reader reader)
        throws IOException
    {
        return readReader(reader, 4096);
    }

    private static String readReader(Reader reader, int initialBufferSize)
        throws IOException
    {
        char[] buffer = new char[initialBufferSize];
        int offset = 0;
        for (;;) {
            int n = reader.read(buffer, offset, buffer.length - offset);
            if (n < 0) { break;    }
            offset += n;
            if (offset == buffer.length) {
                char[] tmp = new char[buffer.length * 2];
                System.arraycopy(buffer, 0, tmp, 0, offset);
                buffer = tmp;
            }
        }
        return new String(buffer, 0, offset);
    }

    static RuntimeException reportRuntimeError(String msgId) {
        String message = ToolErrorReporter.getMessage(msgId);
        return Context.reportRuntimeError(message);
    }

    static RuntimeException reportRuntimeError(String msgId, String msgArg)
    {
        String message = ToolErrorReporter.getMessage(msgId, msgArg);
        return Context.reportRuntimeError(message);
    }

    NativeArray history;
    public InputStream inStream;
    public PrintStream outStream;
    public PrintStream errStream;
}


class Runner implements Runnable, ContextAction {

    Runner(Scriptable scope, Function func, Object[] args) {
        this.scope = scope;
        f = func;
        this.args = args;
    }

    Runner(Scriptable scope, Script script) {
        this.scope = scope;
        s = script;
    }

    public void run()
    {
        Main.withContext(this);
    }

    public Object run(Context cx)
    {
        if (f != null)
            return f.call(cx, scope, scope, args);
        else
            return s.exec(cx, scope);
    }

    private Scriptable scope;
    private Function f;
    private Script s;
    private Object[] args;
}

class PipeThread extends Thread {

    PipeThread(boolean fromProcess, InputStream from, OutputStream to) {
        setDaemon(true);
        this.fromProcess = fromProcess;
        this.from = from;
        this.to = to;
    }

    public void run() {
        try {
            Global.pipe(fromProcess, from, to);
        } catch (IOException ex) {
            throw Global.reportRuntimeError(
                "msg.uncaughtIOException", ex.getMessage());
        }
    }

    private boolean fromProcess;
    private InputStream from;
    private OutputStream to;
}

