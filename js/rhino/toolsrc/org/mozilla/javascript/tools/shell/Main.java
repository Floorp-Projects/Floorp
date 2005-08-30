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
 * Norris Boyd
 * Igor Bukanov
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
import java.net.URL;
import java.net.URLConnection;
import java.net.MalformedURLException;
import java.util.*;
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
public class Main
{
    public static final ShellContextFactory
        shellContextFactory = new ShellContextFactory();

    /**
     * Proxy class to avoid proliferation of anonymous classes.
     */
    private static class IProxy implements ContextAction
    {
        private static final int PROCESS_FILES = 1;
        private static final int EVAL_INLINE_SCRIPT = 2;

        private int type;
        String[] args;
        String scriptText;

        IProxy(int type)
        {
            this.type = type;
        }

        public Object run(Context cx)
        {
            if (type == PROCESS_FILES) {
                processFiles(cx, args);
            } else if (type == EVAL_INLINE_SCRIPT) {
                Script script = loadScriptFromSource(cx, scriptText,
                                                     "<command>", 1, null);
                if (script != null) {
                    evaluateScript(script, cx, getGlobal());
                }
            } else {
                throw Kit.codeBug();
            }
            return null;
        }
    }


    /**
     * Main entry point.
     *
     * Process arguments as would a normal Java program. Also
     * create a new Context and associate it with the current thread.
     * Then set up the execution environment and begin to
     * execute scripts.
     */
    public static void main(String args[]) {
        try {
            if (Boolean.getBoolean("rhino.use_java_policy_security")) {
                initJavaPolicySecuritySupport();
            }
        } catch (SecurityException ex) {
            ex.printStackTrace(System.err);
        }

        int result = exec(args);
        if (result != 0) {
            System.exit(result);
        }
    }

    /**
     *  Execute the given arguments, but don't System.exit at the end.
     */
    public static int exec(String origArgs[])
    {

        for (int i=0; i < origArgs.length; i++) {
            String arg = origArgs[i];
            if (arg.equals("-sealedlib")) {
                sealedStdLib = true;
                break;
            }
        }

        errorReporter = new ToolErrorReporter(false, global.getErr());
        shellContextFactory.setErrorReporter(errorReporter);
        String[] args = processOptions(origArgs);
        if (processStdin)
            fileList.addElement(null);

        IProxy iproxy = new IProxy(IProxy.PROCESS_FILES);
        iproxy.args = args;
        shellContextFactory.call(iproxy);

        return exitCode;
    }

    static void processFiles(Context cx, String[] args)
    {
        if (!global.initialized) {
            global.init(cx);
        }
        // define "arguments" array in the top-level object:
        // need to allocate new array since newArray requires instances
        // of exactly Object[], not ObjectSubclass[]
        Object[] array = new Object[args.length];
        System.arraycopy(args, 0, array, 0, args.length);
        Scriptable argsObj = cx.newArray(global, array);
        global.defineProperty("arguments", argsObj,
                              ScriptableObject.DONTENUM);

        for (int i=0; i < fileList.size(); i++) {
            processSource(cx, (String) fileList.elementAt(i));
        }

    }

    public static Global getGlobal()
    {
        return global;
    }

    /**
     * Parse arguments.
     */
    public static String[] processOptions(String args[])
    {
        String usageError;
        goodUsage: for (int i = 0; ; ++i) {
            if (i == args.length) {
                return new String[0];
            }
            String arg = args[i];
            if (!arg.startsWith("-")) {
                processStdin = false;
                fileList.addElement(arg);
                String[] result = new String[args.length - i - 1];
                System.arraycopy(args, i+1, result, 0, args.length - i - 1);
                return result;
            }
            if (arg.equals("-version")) {
                if (++i == args.length) {
                    usageError = arg;
                    break goodUsage;
                }
                int version;
                try {
                    version = Integer.parseInt(args[i]);
                } catch (NumberFormatException ex) {
                    usageError = args[i];
                    break goodUsage;
                }
                if (!Context.isValidLanguageVersion(version)) {
                    usageError = args[i];
                    break goodUsage;
                }
                shellContextFactory.setLanguageVersion(version);
                continue;
            }
            if (arg.equals("-opt") || arg.equals("-O")) {
                if (++i == args.length) {
                    usageError = arg;
                    break goodUsage;
                }
                int opt;
                try {
                    opt = Integer.parseInt(args[i]);
                } catch (NumberFormatException ex) {
                    usageError = args[i];
                    break goodUsage;
                }
                if (opt == -2) {
                    // Compatibility with Cocoon Rhino fork
                    opt = -1;
                } else if (!Context.isValidOptimizationLevel(opt)) {
                    usageError = args[i];
                    break goodUsage;
                }
                shellContextFactory.setOptimizationLevel(opt);
                continue;
            }
            if (arg.equals("-strict")) {
                shellContextFactory.setStrictMode(true);
                continue;
            }
            if (arg.equals("-e")) {
                processStdin = false;
                if (++i == args.length) {
                    usageError = arg;
                    break goodUsage;
                }
                IProxy iproxy = new IProxy(IProxy.EVAL_INLINE_SCRIPT);
                iproxy.scriptText = args[i];
                shellContextFactory.call(iproxy);
                continue;
            }
            if (arg.equals("-w")) {
                errorReporter.setIsReportingWarnings(true);
                continue;
            }
            if (arg.equals("-f")) {
                processStdin = false;
                if (++i == args.length) {
                    usageError = arg;
                    break goodUsage;
                }
                fileList.addElement(args[i].equals("-") ? null : args[i]);
                continue;
            }
            if (arg.equals("-sealedlib")) {
                // Should already be processed
                if (!sealedStdLib) Kit.codeBug();
                continue;
            }
            usageError = arg;
            break goodUsage;
        }
        // print usage message
        p(ToolErrorReporter.getMessage("msg.shell.usage", usageError));
        System.exit(1);
        return null;
    }

    private static void initJavaPolicySecuritySupport()
    {
        Throwable exObj;
        try {
            Class cl = Class.forName
                ("org.mozilla.javascript.tools.shell.JavaPolicySecurity");
            securityImpl = (SecurityProxy)cl.newInstance();
            SecurityController.initGlobal(securityImpl);
            return;
        } catch (ClassNotFoundException ex) {
            exObj = ex;
        } catch (IllegalAccessException ex) {
            exObj = ex;
        } catch (InstantiationException ex) {
            exObj = ex;
        } catch (LinkageError ex) {
            exObj = ex;
        }
        throw Kit.initCause(new IllegalStateException(
            "Can not load security support: "+exObj), exObj);
    }

    /**
     * Evaluate JavaScript source.
     *
     * @param cx the current context
     * @param filename the name of the file to compile, or null
     *                 for interactive mode.
     */
    public static void processSource(Context cx, String filename)
    {
        if (filename == null || filename.equals("-")) {
            if (filename == null) {
                // print implementation version
                getOut().println(cx.getImplementationVersion());
            }

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
                Script script = loadScriptFromSource(cx, source, "<stdin>",
                                                     lineno, null);
                if (script != null) {
                    Object result = evaluateScript(script, cx, global);
                    if (result != Context.getUndefinedValue()) {
                        try {
                            global.getErr().println(Context.toString(result));
                        } catch (RhinoException rex) {
                            errorReporter.reportException(rex);
                        }
                    }
                    NativeArray h = global.history;
                    h.put((int)h.getLength(), h, source);
                }
            }
            global.getErr().println();
        } else {
            processFile(cx, global, filename);
        }
        System.gc();
    }

    public static void processFile(Context cx, Scriptable scope,
                                   String filename)
    {
        if (securityImpl == null) {
            processFileSecure(cx, scope, filename, null);
        } else {
            securityImpl.callProcessFileSecure(cx, scope, filename);
        }
    }

    static void processFileSecure(Context cx, Scriptable scope,
                                  String path, Object securityDomain)
    {
        Script script;
        if (path.endsWith(".class")) {
            script = loadCompiledScript(cx, path, securityDomain);
        } else {
            String source = (String)readFileOrUrl(path, true);
            if (source == null) {
                exitCode = EXITCODE_FILE_NOT_FOUND;
                return;
            }

            // Support the executable script #! syntax:  If
            // the first line begins with a '#', treat the whole
            // line as a comment.
            if (source.length() > 0 && source.charAt(0) == '#') {
                for (int i = 1; i != source.length(); ++i) {
                    int c = source.charAt(i);
                    if (c == '\n' || c == '\r') {
                        source = source.substring(i);
                        break;
                    }
                }
            }
            script = loadScriptFromSource(cx, source, path, 1, securityDomain);
        }
        if (script != null) {
            evaluateScript(script, cx, scope);
        }
    }

    public static Script loadScriptFromSource(Context cx, String scriptSource,
                                              String path, int lineno,
                                              Object securityDomain)
    {
        try {
            return cx.compileString(scriptSource, path, lineno,
                                    securityDomain);
        } catch (WrappedException we) {
            global.getErr().println(we.getWrappedException().toString());
            we.printStackTrace();
        } catch (EvaluatorException ee) {
            // Already printed message.
            exitCode = EXITCODE_RUNTIME_ERROR;
        } catch (RhinoException rex) {
            errorReporter.reportException(rex);
            exitCode = EXITCODE_RUNTIME_ERROR;
        } catch (VirtualMachineError ex) {
            // Treat StackOverflow and OutOfMemory as runtime errors
            ex.printStackTrace();
            String msg = ToolErrorReporter.getMessage(
                "msg.uncaughtJSException", ex.toString());
            exitCode = EXITCODE_RUNTIME_ERROR;
            Context.reportError(msg);
        }
        return null;
    }

    private static Script loadCompiledScript(Context cx, String path,
                                             Object securityDomain)
    {
        byte[] data = (byte[])readFileOrUrl(path, false);
        if (data == null) {
            exitCode = EXITCODE_FILE_NOT_FOUND;
            return null;
        }
        // XXX: For now extract class name of compiled Script from path
        // instead of parsing class bytes
        int nameStart = path.lastIndexOf('/');
        if (nameStart < 0) {
            nameStart = 0;
        } else {
            ++nameStart;
        }
        int nameEnd = path.lastIndexOf('.');
        if (nameEnd < nameStart) {
            // '.' does not exist in path (nameEnd < 0)
            // or it comes before nameStart
            nameEnd = path.length();
        }
        String name = path.substring(nameStart, nameEnd);
        try {
            GeneratedClassLoader loader = SecurityController.createLoader(cx.getApplicationClassLoader(), securityDomain);
            Class clazz = loader.defineClass(name, data);
            loader.linkClass(clazz);
            if (!Script.class.isAssignableFrom(clazz)) {
                throw Context.reportRuntimeError("msg.must.implement.Script");
            }
            return (Script) clazz.newInstance();
         } catch (RhinoException rex) {
            errorReporter.reportException(rex);
            exitCode = EXITCODE_RUNTIME_ERROR;
        } catch (IllegalAccessException iaex) {
            exitCode = EXITCODE_RUNTIME_ERROR;
            Context.reportError(iaex.toString());
        } catch (InstantiationException inex) {
            exitCode = EXITCODE_RUNTIME_ERROR;
            Context.reportError(inex.toString());
        }
        return null;
    }

    public static Object evaluateScript(Script script, Context cx,
                                        Scriptable scope)
    {
        if (!global.initialized) {
            global.init(cx);
        }
        try {
            return script.exec(cx, scope);
        } catch (WrappedException we) {
            global.getErr().println(we.getWrappedException().toString());
            we.printStackTrace();
        } catch (RhinoException rex) {
            errorReporter.reportException(rex);
            exitCode = EXITCODE_RUNTIME_ERROR;
        } catch (VirtualMachineError ex) {
            // Treat StackOverflow and OutOfMemory as runtime errors
            ex.printStackTrace();
            String msg = ToolErrorReporter.getMessage(
                "msg.uncaughtJSException", ex.toString());
            exitCode = EXITCODE_RUNTIME_ERROR;
            Context.reportError(msg);
        }
        return Context.getUndefinedValue();
    }

    private static void p(String s) {
        global.getOut().println(s);
    }

    public static ScriptableObject getScope() {
        if (!global.initialized) {
            global.init(Context.getCurrentContext());
        }
        return global;
    }

    public static InputStream getIn() {
        return Global.getInstance(getGlobal()).getIn();
    }

    public static void setIn(InputStream in) {
        Global.getInstance(getGlobal()).setIn(in);
    }

    public static PrintStream getOut() {
        return Global.getInstance(getGlobal()).getOut();
    }

    public static void setOut(PrintStream out) {
        Global.getInstance(getGlobal()).setOut(out);
    }

    public static PrintStream getErr() {
        return Global.getInstance(getGlobal()).getErr();
    }

    public static void setErr(PrintStream err) {
        Global.getInstance(getGlobal()).setErr(err);
    }

    /**
     * Read file or url specified by <tt>path</tt>.
     * @return file or url content as <tt>byte[]</tt> or as <tt>String</tt> if
     * <tt>convertToString</tt> is true.
     */
    private static Object readFileOrUrl(String path, boolean convertToString)
    {
        URL url = null;
        // Assume path is URL if it contains dot and there are at least
        // 2 characters in the protocol part. The later allows under Windows
        // to interpret paths with driver letter as file, not URL.
        if (path.indexOf(':') >= 2) {
            try {
                url = new URL(path);
            } catch (MalformedURLException ex) {
            }
        }

        InputStream is = null;
        int capacityHint = 0;
        if (url == null) {
            File file = new File(path);
            capacityHint = (int)file.length();
            try {
                is = new FileInputStream(file);
            } catch (IOException ex) {
                Context.reportError(ToolErrorReporter.getMessage(
                    "msg.couldnt.open", path));
                return null;
            }
        } else {
            try {
                URLConnection uc = url.openConnection();
                is = uc.getInputStream();
                capacityHint = uc.getContentLength();
                // Ignore insane values for Content-Length
                if (capacityHint > (1 << 20)) {
                    capacityHint = -1;
                }
            } catch (IOException ex) {
                Context.reportError(ToolErrorReporter.getMessage(
                    "msg.couldnt.open.url", url.toString(), ex.toString()));
                return null;
            }
        }
        if (capacityHint <= 0) {
            capacityHint = 4096;
        }

        byte[] data;
        try {
            try {
                data = Kit.readStream(is, capacityHint);
            } finally {
                is.close();
            }
        } catch (IOException ex) {
            Context.reportError(ex.toString());
            return null;
        }

        Object result;
        if (!convertToString) {
            result = data;
        } else {
            // Convert to String using the default encoding
            // XXX: Use 'charset=' argument of Content-Type if URL?
            result = new String(data);
        }
        return result;
    }

    static protected final Global global = new Global();
    static protected ToolErrorReporter errorReporter;
    static protected int exitCode = 0;
    static private final int EXITCODE_RUNTIME_ERROR = 3;
    static private final int EXITCODE_FILE_NOT_FOUND = 4;
    //static private DebugShell debugShell;
    static boolean processStdin = true;
    static boolean sealedStdLib = false;
    static Vector fileList = new Vector(5);
    private static SecurityProxy securityImpl;
}
