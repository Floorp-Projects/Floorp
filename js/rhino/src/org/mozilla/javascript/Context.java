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
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Patrick Beard
 * Norris Boyd
 * Igor Bukanov
 * Brendan Eich
 * Roger Lawrence
 * Mike McCabe
 * Ian D. Stewart
 * Andi Vajda
 * Andrew Wason
 * Kemal Bayram
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

// API class

package org.mozilla.javascript;

import java.beans.*;
import java.io.*;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.Locale;
import java.util.ResourceBundle;
import java.text.MessageFormat;
import java.lang.reflect.*;
import org.mozilla.javascript.debug.*;

/**
 * This class represents the runtime context of an executing script.
 *
 * Before executing a script, an instance of Context must be created
 * and associated with the thread that will be executing the script.
 * The Context will be used to store information about the executing
 * of the script such as the call stack. Contexts are associated with
 * the current thread  using the <a href="#enter()">enter()</a> method.<p>
 *
 * The behavior of the execution engine may be altered through methods
 * such as <a href="#setLanguageVersion>setLanguageVersion</a> and
 * <a href="#setErrorReporter>setErrorReporter</a>.<p>
 *
 * Different forms of script execution are supported. Scripts may be
 * evaluated from the source directly, or first compiled and then later
 * executed. Interactive execution is also supported.<p>
 *
 * Some aspects of script execution, such as type conversions and
 * object creation, may be accessed directly through methods of
 * Context.
 *
 * @see Scriptable
 * @author Norris Boyd
 * @author Brendan Eich
 */

public class Context {
    public static final String languageVersionProperty = "language version";
    public static final String errorReporterProperty   = "error reporter";

    /**
     * Create a new Context.
     *
     * Note that the Context must be associated with a thread before
     * it can be used to execute a script.
     *
     * @see org.mozilla.javascript.Context#enter
     */
    public Context() {
        setLanguageVersion(VERSION_DEFAULT);
        optimizationLevel = codegenClass != null ? 0 : -1;
        Object[] array = contextListeners;
        if (array != null) {
            for (int i = array.length; i-- != 0;) {
                ((ContextListener)array[i]).contextCreated(this);
            }
        }
    }

/**
@deprecated The {@link SecuritySupport} class is deprecated. See its documentation for the upgrade path.
*/
    public Context(SecuritySupport x) {
        this();
        setClassShutter(x);
    }

    /**
     * Get a context associated with the current thread, creating
     * one if need be.
     *
     * The Context stores the execution state of the JavaScript
     * engine, so it is required that the context be entered
     * before execution may begin. Once a thread has entered
     * a Context, then getCurrentContext() may be called to find
     * the context that is associated with the current thread.
     * <p>
     * Calling <code>enter()</code> will
     * return either the Context currently associated with the
     * thread, or will create a new context and associate it
     * with the current thread. Each call to <code>enter()</code>
     * must have a matching call to <code>exit()</code>. For example,
     * <pre>
     *      Context cx = Context.enter();
     *      try {
     *          ...
     *          cx.evaluateString(...);
     *      }
     *      finally { Context.exit(); }
     * </pre>
     * @return a Context associated with the current thread
     * @see org.mozilla.javascript.Context#getCurrentContext
     * @see org.mozilla.javascript.Context#exit
     */
    public static Context enter() {
        return enter(null);
    }

    /**
     * Get a Context associated with the current thread, using
     * the given Context if need be.
     * <p>
     * The same as <code>enter()</code> except that <code>cx</code>
     * is associated with the current thread and returned if
     * the current thread has no associated context and <code>cx</code>
     * is not associated with any other thread.
     * @param cx a Context to associate with the thread if possible
     * @return a Context associated with the current thread
     */
    public static Context enter(Context cx) {

        Context old = getCurrentContext();

        if (cx == null) {
            if (old != null) {
                cx = old;
            } else {
                cx = new Context();
                setThreadContext(cx);
            }
        } else {
            if (cx.enterCount != 0) {
                   // The suplied context must be the context for
                // the current thread if it is already entered
                if (cx != old) {
                    throw new RuntimeException
                        ("Cannot enter Context active on another thread");
                }
            } else {
                if (old != null) {
                    cx = old;
                } else {
                    setThreadContext(cx);
                }
            }
        }

        ++cx.enterCount;

        Object[] listeners = contextListeners;
        if (listeners != null) {
            for (int i = listeners.length; i-- != 0;) {
                ((ContextListener)listeners[i]).contextEntered(cx);
            }
        }
        return cx;
     }

    /**
     * Exit a block of code requiring a Context.
     *
     * Calling <code>exit()</code> will remove the association between
     * the current thread and a Context if the prior call to
     * <code>enter()</code> on this thread newly associated a Context
     * with this thread.
     * Once the current thread no longer has an associated Context,
     * it cannot be used to execute JavaScript until it is again associated
     * with a Context.
     *
     * @see org.mozilla.javascript.Context#enter
     */
    public static void exit() {
        boolean released = false;
        Context cx = getCurrentContext();
        if (cx == null) {
            throw new RuntimeException
                ("Calling Context.exit without previous Context.enter");
        }
        if (Context.check && cx.enterCount < 1) Context.codeBug();
        --cx.enterCount;
        if (cx.enterCount == 0) {
            released = true;
            setThreadContext(null);
        }

        Object[] listeners = contextListeners;
        if (listeners != null) {
            for (int i = listeners.length; i-- != 0;) {
                ((ContextListener)listeners[i]).contextExited(cx);
            }
            if (released) {
                for (int i = listeners.length; i-- != 0;) {
                    ((ContextListener)listeners[i]).contextReleased(cx);
                }
            }
        }
    }

    /**
     * Add a Context listener.
     */
    public static void addContextListener(ContextListener listener) {
        synchronized (contextListenersLock) {
            contextListeners = ListenerArray.add(contextListeners, listener);
        }
    }

    /**
     * Remove a Context listener.
     * @param listener the listener to remove.
     */
    public static void removeContextListener(ContextListener listener) {
        synchronized (contextListenersLock) {
            contextListeners = ListenerArray.remove(contextListeners, listener);
        }
    }

    /**
     * Get the current Context.
     *
     * The current Context is per-thread; this method looks up
     * the Context associated with the current thread. <p>
     *
     * @return the Context associated with the current thread, or
     *         null if no context is associated with the current
     *         thread.
     * @see org.mozilla.javascript.Context#enter
     * @see org.mozilla.javascript.Context#exit
     */
    public static Context getCurrentContext() {
        if (threadLocalCx != null) {
            try {
                return (Context)threadLocalGet.invoke(threadLocalCx, null);
            } catch (Exception ex) { }
        }
        Thread t = Thread.currentThread();
        return (Context) threadContexts.get(t);
    }

    private static void setThreadContext(Context cx) {
        if (threadLocalCx != null) {
            try {
                threadLocalSet.invoke(threadLocalCx, new Object[] { cx });
                return;
            } catch (Exception ex) { }
        }
        Thread t = Thread.currentThread();
        if (cx != null) {
            threadContexts.put(t, cx);
        } else {
            threadContexts.remove(t);
        }
    }

    /**
     * Language versions
     *
     * All integral values are reserved for future version numbers.
     */

    /**
     * The unknown version.
     */
    public static final int VERSION_UNKNOWN =   -1;

    /**
     * The default version.
     */
    public static final int VERSION_DEFAULT =    0;

    /**
     * JavaScript 1.0
     */
    public static final int VERSION_1_0 =      100;

    /**
     * JavaScript 1.1
     */
    public static final int VERSION_1_1 =      110;

    /**
     * JavaScript 1.2
     */
    public static final int VERSION_1_2 =      120;

    /**
     * JavaScript 1.3
     */
    public static final int VERSION_1_3 =      130;

    /**
     * JavaScript 1.4
     */
    public static final int VERSION_1_4 =      140;

    /**
     * JavaScript 1.5
     */
    public static final int VERSION_1_5 =      150;

    /**
     * Get the current language version.
     * <p>
     * The language version number affects JavaScript semantics as detailed
     * in the overview documentation.
     *
     * @return an integer that is one of VERSION_1_0, VERSION_1_1, etc.
     */
    public int getLanguageVersion() {
       return version;
    }

    /**
     * Set the language version.
     *
     * <p>
     * Setting the language version will affect functions and scripts compiled
     * subsequently. See the overview documentation for version-specific
     * behavior.
     *
     * @param version the version as specified by VERSION_1_0, VERSION_1_1, etc.
     */
    public void setLanguageVersion(int version) {
        Object[] array = listeners;
        if (array != null && version != this.version) {
            firePropertyChangeImpl(array, languageVersionProperty,
                               new Integer(this.version),
                               new Integer(version));
        }
        this.version = version;
    }

    /**
     * Get the implementation version.
     *
     * <p>
     * The implementation version is of the form
     * <pre>
     *    "<i>name langVer</i> <code>release</code> <i>relNum date</i>"
     * </pre>
     * where <i>name</i> is the name of the product, <i>langVer</i> is
     * the language version, <i>relNum</i> is the release number, and
     * <i>date</i> is the release date for that specific
     * release in the form "yyyy mm dd".
     *
     * @return a string that encodes the product, language version, release
     *         number, and date.
     */
     public String getImplementationVersion() {
        return "Rhino 1.5 release 4 0000 00 00 (in progress)";
     }

    /**
     * Get the current error reporter.
     *
     * @see org.mozilla.javascript.ErrorReporter
     */
    public ErrorReporter getErrorReporter() {
        if (errorReporter == null) {
            errorReporter = new DefaultErrorReporter();
        }
        return errorReporter;
    }

    /**
     * Change the current error reporter.
     *
     * @return the previous error reporter
     * @see org.mozilla.javascript.ErrorReporter
     */
    public ErrorReporter setErrorReporter(ErrorReporter reporter) {
        ErrorReporter result = errorReporter;
        Object[] array = listeners;
        if (array != null && errorReporter != reporter) {
            firePropertyChangeImpl(array, errorReporterProperty,
                                   errorReporter, reporter);
        }
        errorReporter = reporter;
        return result;
    }

    /**
     * Get the current locale.  Returns the default locale if none has
     * been set.
     *
     * @see java.util.Locale
     */

    public Locale getLocale() {
        if (locale == null)
            locale = Locale.getDefault();
        return locale;
    }

    /**
     * Set the current locale.
     *
     * @see java.util.Locale
     */
    public Locale setLocale(Locale loc) {
        Locale result = locale;
        locale = loc;
        return result;
    }

    /**
     * Register an object to receive notifications when a bound property
     * has changed
     * @see java.beans.PropertyChangeEvent
     * @see #removePropertyChangeListener(java.beans.PropertyChangeListener)
     * @param  listener  the listener
     */
    public void addPropertyChangeListener(PropertyChangeListener listener) {
        synchronized (this) {
            listeners = ListenerArray.add(listeners, listener);
        }
    }

    /**
     * Remove an object from the list of objects registered to receive
     * notification of changes to a bounded property
     * @see java.beans.PropertyChangeEvent
     * @see #addPropertyChangeListener(java.beans.PropertyChangeListener)
     * @param listener  the listener
     */
    public void removePropertyChangeListener(PropertyChangeListener listener) {
        synchronized (this) {
            listeners = ListenerArray.remove(listeners, listener);
        }
    }

    /**
     * Notify any registered listeners that a bounded property has changed
     * @see #addPropertyChangeListener(java.beans.PropertyChangeListener)
     * @see #removePropertyChangeListener(java.beans.PropertyChangeListener)
     * @see java.beans.PropertyChangeListener
     * @see java.beans.PropertyChangeEvent
     * @param  property  the bound property
     * @param  oldValue  the old value
     * @param  newVale   the new value
     */
    void firePropertyChange(String property, Object oldValue,
                            Object newValue)
    {
        Object[] array = listeners;
        if (array != null) {
            firePropertyChangeImpl(array, property, oldValue, newValue);
        }
    }

    private void firePropertyChangeImpl(Object[] array, String property,
                                        Object oldValue, Object newValue)
    {
        for (int i = array.length; i-- != 0;) {
            Object obj = array[i];
            if (obj instanceof PropertyChangeListener) {
                PropertyChangeListener l = (PropertyChangeListener)obj;
                l.propertyChange(new PropertyChangeEvent(
                    this, property, oldValue, newValue));
            }
    }
    }

    /**
     * Report a warning using the error reporter for the current thread.
     *
     * @param message the warning message to report
     * @param sourceName a string describing the source, such as a filename
     * @param lineno the starting line number
     * @param lineSource the text of the line (may be null)
     * @param lineOffset the offset into lineSource where problem was detected
     * @see org.mozilla.javascript.ErrorReporter
     */
    public static void reportWarning(String message, String sourceName,
                                     int lineno, String lineSource,
                                     int lineOffset)
    {
        Context cx = Context.getContext();
        cx.getErrorReporter().warning(message, sourceName, lineno,
                                      lineSource, lineOffset);
    }

    /**
     * Report a warning using the error reporter for the current thread.
     *
     * @param message the warning message to report
     * @see org.mozilla.javascript.ErrorReporter
     */
    public static void reportWarning(String message) {
        int[] linep = { 0 };
        String filename = getSourcePositionFromStack(linep);
        Context.reportWarning(message, filename, linep[0], null, 0);
    }

    /**
     * Report an error using the error reporter for the current thread.
     *
     * @param message the error message to report
     * @param sourceName a string describing the source, such as a filename
     * @param lineno the starting line number
     * @param lineSource the text of the line (may be null)
     * @param lineOffset the offset into lineSource where problem was detected
     * @see org.mozilla.javascript.ErrorReporter
     */
    public static void reportError(String message, String sourceName,
                                   int lineno, String lineSource,
                                   int lineOffset)
    {
        Context cx = getCurrentContext();
        if (cx != null) {
            cx.errorCount++;
            cx.getErrorReporter().error(message, sourceName, lineno,
                                        lineSource, lineOffset);
        } else {
            throw new EvaluatorException(message);
        }
    }

    /**
     * Report an error using the error reporter for the current thread.
     *
     * @param message the error message to report
     * @see org.mozilla.javascript.ErrorReporter
     */
    public static void reportError(String message) {
        int[] linep = { 0 };
        String filename = getSourcePositionFromStack(linep);
        Context.reportError(message, filename, linep[0], null, 0);
    }

    /**
     * Report a runtime error using the error reporter for the current thread.
     *
     * @param message the error message to report
     * @param sourceName a string describing the source, such as a filename
     * @param lineno the starting line number
     * @param lineSource the text of the line (may be null)
     * @param lineOffset the offset into lineSource where problem was detected
     * @return a runtime exception that will be thrown to terminate the
     *         execution of the script
     * @see org.mozilla.javascript.ErrorReporter
     */
    public static EvaluatorException reportRuntimeError(String message,
                                                      String sourceName,
                                                      int lineno,
                                                      String lineSource,
                                                      int lineOffset)
    {
        Context cx = getCurrentContext();
        if (cx != null) {
            cx.errorCount++;
            return cx.getErrorReporter().
                            runtimeError(message, sourceName, lineno,
                                         lineSource, lineOffset);
        } else {
            throw new EvaluatorException(message);
        }
    }

    static EvaluatorException reportRuntimeError0(String messageId) {
        return reportRuntimeError(getMessage0(messageId));
    }

    static EvaluatorException reportRuntimeError1
        (String messageId, Object arg1)
    {
        return reportRuntimeError(getMessage1(messageId, arg1));
    }

    static EvaluatorException reportRuntimeError2
        (String messageId, Object arg1, Object arg2)
    {
        return reportRuntimeError(getMessage2(messageId, arg1, arg2));
    }

    static EvaluatorException reportRuntimeError3
        (String messageId, Object arg1, Object arg2, Object arg3)
    {
        return reportRuntimeError(getMessage3(messageId, arg1, arg2, arg3));
    }

    /**
     * Report a runtime error using the error reporter for the current thread.
     *
     * @param message the error message to report
     * @see org.mozilla.javascript.ErrorReporter
     */
    public static EvaluatorException reportRuntimeError(String message) {
        int[] linep = { 0 };
        String filename = getSourcePositionFromStack(linep);
        return Context.reportRuntimeError(message, filename, linep[0], null, 0);
    }

    /**
     * Initialize the standard objects.
     *
     * Creates instances of the standard objects and their constructors
     * (Object, String, Number, Date, etc.), setting up 'scope' to act
     * as a global object as in ECMA 15.1.<p>
     *
     * This method must be called to initialize a scope before scripts
     * can be evaluated in that scope.<p>
     *
     * This method does not affect the Context it is called upon.
     *
     * @param scope the scope to initialize, or null, in which case a new
     *        object will be created to serve as the scope
     * @return the initialized scope
     */
    public Scriptable initStandardObjects(ScriptableObject scope) {
        return initStandardObjects(scope, false);
    }

    /**
     * Initialize the standard objects.
     *
     * Creates instances of the standard objects and their constructors
     * (Object, String, Number, Date, etc.), setting up 'scope' to act
     * as a global object as in ECMA 15.1.<p>
     *
     * This method must be called to initialize a scope before scripts
     * can be evaluated in that scope.<p>
     *
     * This method does not affect the Context it is called upon.<p>
     *
     * This form of the method also allows for creating "sealed" standard
     * objects. An object that is sealed cannot have properties added or
     * removed. This is useful to create a "superglobal" that can be shared
     * among several top-level objects. Note that sealing is not allowed in
     * the current ECMA/ISO language specification, but is likely for
     * the next version.
     *
     * @param scope the scope to initialize, or null, in which case a new
     *        object will be created to serve as the scope
     * @param sealed whether or not to create sealed standard objects that
     *        cannot be modified.
     * @return the initialized scope
     * @since 1.4R3
     */
    public ScriptableObject initStandardObjects(ScriptableObject scope,
                                                boolean sealed)
    {
        if (scope == null)
            scope = new NativeObject();

        BaseFunction.init(this, scope, sealed);
        NativeObject.init(this, scope, sealed);

        Scriptable objectProto = ScriptableObject.getObjectPrototype(scope);

        // Function.prototype.__proto__ should be Object.prototype
        Scriptable functionProto = ScriptableObject.getFunctionPrototype(scope);
        functionProto.setPrototype(objectProto);

        // Set the prototype of the object passed in if need be
        if (scope.getPrototype() == null)
            scope.setPrototype(objectProto);

        // must precede NativeGlobal since it's needed therein
        NativeError.init(this, scope, sealed);
        NativeGlobal.init(this, scope, sealed);

        NativeArray.init(this, scope, sealed);
        NativeString.init(this, scope, sealed);
        NativeBoolean.init(this, scope, sealed);
        NativeNumber.init(this, scope, sealed);
        NativeDate.init(this, scope, sealed);
        NativeMath.init(this, scope, sealed);

        NativeWith.init(this, scope, sealed);
        NativeCall.init(this, scope, sealed);
        NativeScript.init(this, scope, sealed);

        new LazilyLoadedCtor(scope,
                             "RegExp",
                             "org.mozilla.javascript.regexp.NativeRegExp",
                             sealed);

        // This creates the Packages and java package roots.
        new LazilyLoadedCtor(scope,
                             "Packages",
                             "org.mozilla.javascript.NativeJavaPackage",
                             sealed);
        new LazilyLoadedCtor(scope,
                             "java",
                             "org.mozilla.javascript.NativeJavaPackage",
                             sealed);
        new LazilyLoadedCtor(scope,
                             "getClass",
                             "org.mozilla.javascript.NativeJavaPackage",
                             sealed);

        // Define the JavaAdapter class, allowing it to be overridden.
        String adapterClass = "org.mozilla.javascript.JavaAdapter";
        String adapterProperty = "JavaAdapter";
        try {
            adapterClass = System.getProperty(adapterClass, adapterClass);
            adapterProperty = System.getProperty
                ("org.mozilla.javascript.JavaAdapterClassName",
                 adapterProperty);
        }
        catch (SecurityException e) {
            // We may not be allowed to get system properties. Just
            // use the default adapter in that case.
        }

        new LazilyLoadedCtor(scope, adapterProperty, adapterClass, sealed);

        return scope;
    }

    /**
     * Get the singleton object that represents the JavaScript Undefined value.
     */
    public static Object getUndefinedValue() {
        return Undefined.instance;
    }

    /**
     * Evaluate a JavaScript source string.
     *
     * The provided source name and line number are used for error messages
     * and for producing debug information.
     *
     * @param scope the scope to execute in
     * @param source the JavaScript source
     * @param sourceName a string describing the source, such as a filename
     * @param lineno the starting line number
     * @param securityDomain an arbitrary object that specifies security
     *        information about the origin or owner of the script. For
     *        implementations that don't care about security, this value
     *        may be null.
     * @return the result of evaluating the string
     * @exception JavaScriptException if an uncaught JavaScript exception
     *            occurred while evaluating the source string
     * @see org.mozilla.javascript.SecuritySupport
     */
    public Object evaluateString(Scriptable scope, String source,
                                 String sourceName, int lineno,
                                 Object securityDomain)
        throws JavaScriptException
    {
        try {
            Reader in = new StringReader(source);
            return evaluateReader(scope, in, sourceName, lineno,
                                  securityDomain);
        }
        catch (IOException ioe) {
            // Should never occur because we just made the reader from a String
            throw new RuntimeException();
        }
    }

    /**
     * Evaluate a reader as JavaScript source.
     *
     * All characters of the reader are consumed.
     *
     * @param scope the scope to execute in
     * @param in the Reader to get JavaScript source from
     * @param sourceName a string describing the source, such as a filename
     * @param lineno the starting line number
     * @param securityDomain an arbitrary object that specifies security
     *        information about the origin or owner of the script. For
     *        implementations that don't care about security, this value
     *        may be null.
     * @return the result of evaluating the source
     *
     * @exception IOException if an IOException was generated by the Reader
     * @exception JavaScriptException if an uncaught JavaScript exception
     *            occurred while evaluating the Reader
     */
    public Object evaluateReader(Scriptable scope, Reader in,
                                 String sourceName, int lineno,
                                 Object securityDomain)
        throws IOException, JavaScriptException
    {
        Script script = compileReader(scope, in, sourceName, lineno,
                                      securityDomain);
        if (script != null)
            return script.exec(this, scope);
        else
            return null;
    }

    /**
     * Check whether a string is ready to be compiled.
     * <p>
     * stringIsCompilableUnit is intended to support interactive compilation of
     * javascript.  If compiling the string would result in an error
     * that might be fixed by appending more source, this method
     * returns false.  In every other case, it returns true.
     * <p>
     * Interactive shells may accumulate source lines, using this
     * method after each new line is appended to check whether the
     * statement being entered is complete.
     *
     * @param source the source buffer to check
     * @return whether the source is ready for compilation
     * @since 1.4 Release 2
     */
    synchronized public boolean stringIsCompilableUnit(String source)
    {
        Reader in = new StringReader(source);
        // no source name or source text manager, because we're just
        // going to throw away the result.
        TokenStream ts = new TokenStream(in, null, null, 1);

        // Temporarily set error reporter to always be the exception-throwing
        // DefaultErrorReporter.  (This is why the method is synchronized...)
        ErrorReporter currentReporter =
            setErrorReporter(new DefaultErrorReporter());

        boolean errorseen = false;
        try {
            IRFactory irf = new IRFactory(ts, null);
            Parser p = new Parser(irf);
            p.parse(ts);
        } catch (IOException ioe) {
            errorseen = true;
        } catch (EvaluatorException ee) {
            errorseen = true;
        } finally {
            // Restore the old error reporter.
            setErrorReporter(currentReporter);
        }
        // Return false only if an error occurred as a result of reading past
        // the end of the file, i.e. if the source could be fixed by
        // appending more source.
        if (errorseen && ts.eof())
            return false;
        else
            return true;
    }

    /**
     * Compiles the source in the given reader.
     * <p>
     * Returns a script that may later be executed.
     * Will consume all the source in the reader.
     *
     * @param scope if nonnull, will be the scope in which the script object
     *        is created. The script object will be a valid JavaScript object
     *        as if it were created using the JavaScript1.3 Script constructor
     * @param in the input reader
     * @param sourceName a string describing the source, such as a filename
     * @param lineno the starting line number for reporting errors
     * @param securityDomain an arbitrary object that specifies security
     *        information about the origin or owner of the script. For
     *        implementations that don't care about security, this value
     *        may be null.
     * @return a script that may later be executed
     * @see org.mozilla.javascript.Script#exec
     * @exception IOException if an IOException was generated by the Reader
     */
    public Script compileReader(Scriptable scope, Reader in, String sourceName,
                                int lineno, Object securityDomain)
        throws IOException
    {
        return (Script) compile(scope, in, sourceName, lineno, securityDomain,
                                false);
    }


    /**
     * Compile a JavaScript function.
     * <p>
     * The function source must be a function definition as defined by
     * ECMA (e.g., "function f(a) { return a; }").
     *
     * @param scope the scope to compile relative to
     * @param source the function definition source
     * @param sourceName a string describing the source, such as a filename
     * @param lineno the starting line number
     * @param securityDomain an arbitrary object that specifies security
     *        information about the origin or owner of the script. For
     *        implementations that don't care about security, this value
     *        may be null.
     * @return a Function that may later be called
     * @see org.mozilla.javascript.Function
     */
    public Function compileFunction(Scriptable scope, String source,
                                    String sourceName, int lineno,
                                    Object securityDomain)
    {
        Reader in = new StringReader(source);
        try {
            return (Function) compile(scope, in, sourceName, lineno,
                                      securityDomain, true);
        }
        catch (IOException ioe) {
            // Should never happen because we just made the reader
            // from a String
            throw new RuntimeException();
        }
    }

    /**
     * Decompile the script.
     * <p>
     * The canonical source of the script is returned.
     *
     * @param script the script to decompile
     * @param scope the scope under which to decompile
     * @param indent the number of spaces to indent the result
     * @return a string representing the script source
     */
     public String decompileScript(Script script, Scriptable scope,
                                   int indent)
    {
        NativeScript ns = (NativeScript) script;
        ns.initScript(scope);
        return ns.decompile(this, indent, false);
    }

    /**
     * Decompile a JavaScript Function.
     * <p>
     * Decompiles a previously compiled JavaScript function object to
     * canonical source.
     * <p>
     * Returns function body of '[native code]' if no decompilation
     * information is available.
     *
     * @param fun the JavaScript function to decompile
     * @param indent the number of spaces to indent the result
     * @return a string representing the function source
     */
    public String decompileFunction(Function fun, int indent) {
        if (fun instanceof BaseFunction)
            return ((BaseFunction)fun).decompile(this, indent, false);
        else
            return "function " + fun.getClassName() +
                   "() {\n\t[native code]\n}\n";
    }

    /**
     * Decompile the body of a JavaScript Function.
     * <p>
     * Decompiles the body a previously compiled JavaScript Function
     * object to canonical source, omitting the function header and
     * trailing brace.
     *
     * Returns '[native code]' if no decompilation information is available.
     *
     * @param fun the JavaScript function to decompile
     * @param indent the number of spaces to indent the result
     * @return a string representing the function body source.
     */
    public String decompileFunctionBody(Function fun, int indent) {
        if (fun instanceof BaseFunction)
            return ((BaseFunction)fun).decompile(this, indent, true);
        else
            // not sure what the right response here is.  JSRef currently
            // dumps core.
            return "[native code]\n";
    }

    /**
     * Create a new JavaScript object.
     *
     * Equivalent to evaluating "new Object()".
     * @param scope the scope to search for the constructor and to evaluate
     *              against
     * @return the new object
     * @exception PropertyException if "Object" cannot be found in
     *            the scope
     * @exception NotAFunctionException if the "Object" found in the scope
     *            is not a function
     * @exception JavaScriptException if an uncaught JavaScript exception
     *            occurred while creating the object
     */
    public Scriptable newObject(Scriptable scope)
        throws PropertyException,
               NotAFunctionException,
               JavaScriptException
    {
        return newObject(scope, "Object", null);
    }

    /**
     * Create a new JavaScript object by executing the named constructor.
     *
     * The call <code>newObject(scope, "Foo")</code> is equivalent to
     * evaluating "new Foo()".
     *
     * @param scope the scope to search for the constructor and to evaluate against
     * @param constructorName the name of the constructor to call
     * @return the new object
     * @exception PropertyException if a property with the constructor
     *            name cannot be found in the scope
     * @exception NotAFunctionException if the property found in the scope
     *            is not a function
     * @exception JavaScriptException if an uncaught JavaScript exception
     *            occurred while creating the object
     */
    public Scriptable newObject(Scriptable scope, String constructorName)
        throws PropertyException,
               NotAFunctionException,
               JavaScriptException
    {
        return newObject(scope, constructorName, null);
    }

    /**
     * Creates a new JavaScript object by executing the named constructor.
     *
     * Searches <code>scope</code> for the named constructor, calls it with
     * the given arguments, and returns the result.<p>
     *
     * The code
     * <pre>
     * Object[] args = { "a", "b" };
     * newObject(scope, "Foo", args)</pre>
     * is equivalent to evaluating "new Foo('a', 'b')", assuming that the Foo
     * constructor has been defined in <code>scope</code>.
     *
     * @param scope The scope to search for the constructor and to evaluate
     *              against
     * @param constructorName the name of the constructor to call
     * @param args the array of arguments for the constructor
     * @return the new object
     * @exception PropertyException if a property with the constructor
     *            name cannot be found in the scope
     * @exception NotAFunctionException if the property found in the scope
     *            is not a function
     * @exception JavaScriptException if an uncaught JavaScript exception
     *            occurs while creating the object
     */
    public Scriptable newObject(Scriptable scope, String constructorName,
                                Object[] args)
        throws PropertyException,
               NotAFunctionException,
               JavaScriptException
    {
        scope = ScriptableObject.getTopLevelScope(scope);
        Object ctorVal = ScriptableObject.getProperty(scope, constructorName);
        if (ctorVal instanceof Function) {
            Function ctor = (Function) ctorVal;
            if (args == null) { args = ScriptRuntime.emptyArgs; }
            return ctor.construct(this, scope, args);
        }
        if (ctorVal == Scriptable.NOT_FOUND) {
            String message = getMessage1("msg.ctor.not.found", constructorName);
            throw new PropertyException(message);
        } else {
            String message = getMessage1("msg.not.ctor", constructorName);
            throw new NotAFunctionException(message);
        }
    }

    /**
     * Create an array with a specified initial length.
     * <p>
     * @param scope the scope to create the object in
     * @param length the initial length (JavaScript arrays may have
     *               additional properties added dynamically).
     * @return the new array object
     */
    public Scriptable newArray(Scriptable scope, int length) {
        Scriptable result = new NativeArray(length);
        newArrayHelper(scope, result);
        return result;
    }

    /**
     * Create an array with a set of initial elements.
     * <p>
     * @param scope the scope to create the object in
     * @param elements the initial elements. Each object in this array
     *                 must be an acceptable JavaScript type.
     * @return the new array object
     */
    public Scriptable newArray(Scriptable scope, Object[] elements) {
        Scriptable result = new NativeArray(elements);
        newArrayHelper(scope, result);
        return result;
    }

    /**
     * Get the elements of a JavaScript array.
     * <p>
     * If the object defines a length property which can be converted to
     * a number a Java array with length given by
     * {@link ScriptRuntime#toUint32(double)} is created and initialized with
     * the values obtained by
     * calling get() on object for each value of i in [0,length-1]. If
     * there is not a defined value for a property the Undefined value
     * is used to initialize the corresponding element in the array. The
     * Java array is then returned.
     * If the object doesn't define a length property or it is not a number,
     * empty array is returned.
     * @param object the JavaScript array or array-like object
     * @return a Java array of objects
     * @since 1.4 release 2
     */
    public Object[] getElements(Scriptable object) {
        long longLen = NativeArray.getLengthProperty(object);
        if (longLen > Integer.MAX_VALUE) {
            // arrays beyond  MAX_INT is not in Java in any case
            throw new IllegalArgumentException();
        }
        int len = (int) longLen;
        if (len == 0) {
            return ScriptRuntime.emptyArgs;
        } else {
            Object[] result = new Object[len];
            for (int i=0; i < len; i++) {
                Object elem = ScriptableObject.getProperty(object, i);
                result[i] = (elem == Scriptable.NOT_FOUND) ? Undefined.instance
                                                           : elem;
            }
            return result;
        }
    }

    /**
     * Convert the value to a JavaScript boolean value.
     * <p>
     * See ECMA 9.2.
     *
     * @param value a JavaScript value
     * @return the corresponding boolean value converted using
     *         the ECMA rules
     */
    public static boolean toBoolean(Object value) {
        return ScriptRuntime.toBoolean(value);
    }

    /**
     * Convert the value to a JavaScript Number value.
     * <p>
     * Returns a Java double for the JavaScript Number.
     * <p>
     * See ECMA 9.3.
     *
     * @param value a JavaScript value
     * @return the corresponding double value converted using
     *         the ECMA rules
     */
    public static double toNumber(Object value) {
        return ScriptRuntime.toNumber(value);
    }

    /**
     * Convert the value to a JavaScript String value.
     * <p>
     * See ECMA 9.8.
     * <p>
     * @param value a JavaScript value
     * @return the corresponding String value converted using
     *         the ECMA rules
     */
    public static String toString(Object value) {
        return ScriptRuntime.toString(value);
    }

    /**
     * Convert the value to an JavaScript object value.
     * <p>
     * Note that a scope must be provided to look up the constructors
     * for Number, Boolean, and String.
     * <p>
     * See ECMA 9.9.
     * <p>
     * Additionally, arbitrary Java objects and classes will be
     * wrapped in a Scriptable object with its Java fields and methods
     * reflected as JavaScript properties of the object.
     *
     * @param value any Java object
     * @param scope global scope containing constructors for Number,
     *              Boolean, and String
     * @return new JavaScript object
     */
    public static Scriptable toObject(Object value, Scriptable scope) {
        return ScriptRuntime.toObject(scope, value, null);
    }

    /**
     * Convert the value to an JavaScript object value.
     * <p>
     * Note that a scope must be provided to look up the constructors
     * for Number, Boolean, and String.
     * <p>
     * See ECMA 9.9.
     * <p>
     * Additionally, arbitrary Java objects and classes will be
     * wrapped in a Scriptable object with its Java fields and methods
     * reflected as JavaScript properties of the object. If the
     * "staticType" parameter is provided, it will be used as the static
     * type of the Java value to create.
     *
     * @param value any Java object
     * @param scope global scope containing constructors for Number,
     *              Boolean, and String
     * @param staticType the static type of the Java value to create
     * @return new JavaScript object
     */
    public static Scriptable toObject(Object value, Scriptable scope,
                                      Class staticType) {
        if (value == null && staticType != null)
            return null;
        return ScriptRuntime.toObject(scope, value, staticType);
    }

    /**
     * Convert a JavaScript value into the desired type.
     * Uses the semantics defined with LiveConnect3 and throws an
     * Illegal argument exception if the conversion cannot be performed.
     * @param value the JavaScript value to convert
     * @param desired type the Java type to convert to. Primitive Java
     *        types are represented using the TYPE fields in the corresponding
     *        wrapper class in java.lang.
     * @return the converted value
     * @throws IllegalArgumentException if the conversion cannot be performed
     */
    public static Object toType(Object value, Class desiredType)
        throws IllegalArgumentException
    {
        return NativeJavaObject.coerceType(desiredType, value, false);
    }

    /**
     * Tell whether debug information is being generated.
     * @since 1.3
     */
    public boolean isGeneratingDebug() {
        return generatingDebug;
    }

    /**
     * Specify whether or not debug information should be generated.
     * <p>
     * Setting the generation of debug information on will set the
     * optimization level to zero.
     * @since 1.3
     */
    public void setGeneratingDebug(boolean generatingDebug) {
        generatingDebugChanged = true;
        if (generatingDebug && getOptimizationLevel() > 0)
            setOptimizationLevel(0);
        this.generatingDebug = generatingDebug;
    }

    /**
     * Tell whether source information is being generated.
     * @since 1.3
     */
    public boolean isGeneratingSource() {
        return generatingSource;
    }

    /**
     * Specify whether or not source information should be generated.
     * <p>
     * Without source information, evaluating the "toString" method
     * on JavaScript functions produces only "[native code]" for
     * the body of the function.
     * Note that code generated without source is not fully ECMA
     * conformant.
     * @since 1.3
     */
    public void setGeneratingSource(boolean generatingSource) {
        this.generatingSource = generatingSource;
    }

    /**
     * Get the current optimization level.
     * <p>
     * The optimization level is expressed as an integer between -1 and
     * 9.
     * @since 1.3
     *
     */
    public int getOptimizationLevel() {
        return optimizationLevel;
    }

    /**
     * Set the current optimization level.
     * <p>
     * The optimization level is expected to be an integer between -1 and
     * 9. Any negative values will be interpreted as -1, and any values
     * greater than 9 will be interpreted as 9.
     * An optimization level of -1 indicates that interpretive mode will
     * always be used. Levels 0 through 9 indicate that class files may
     * be generated. Higher optimization levels trade off compile time
     * performance for runtime performance.
     * The optimizer level can't be set greater than -1 if the optimizer
     * package doesn't exist at run time.
     * @param optimizationLevel an integer indicating the level of
     *        optimization to perform
     * @since 1.3
     *
     */
    public void setOptimizationLevel(int optimizationLevel) {
        if (optimizationLevel < 0) {
            optimizationLevel = -1;
        } else if (optimizationLevel > 9) {
                optimizationLevel = 9;
        }
        if (codegenClass == null)
            optimizationLevel = -1;
        this.optimizationLevel = optimizationLevel;
    }

    /**
     * Get the current class name.
     *
     * @since 30/10/01 tip + patch (Kemal Bayram)
     */
    public String getClassName() {
        ClassNameHelper nameHelper = getNameHelper();
        return nameHelper != null ? nameHelper.getClassName() : null;
    }

    /**
     * Set the current class name.
     *
     * @since 30/10/01 tip + patch (Kemal Bayram)
     */
    public void setClassName(String className) {
        ClassNameHelper nameHelper = getNameHelper();
        if (nameHelper != null)
              nameHelper.setClassName(className);
    }

    /**
     * Get the current target class file name.
     * <p>
     * If nonnull, requests to compile source will result in one or
     * more class files being generated.
     * @since 1.3
     */
    public String getTargetClassFileName() {
        ClassNameHelper nameHelper = getNameHelper();
        if (nameHelper != null) {
                ClassRepository repository = nameHelper.getClassRepository();
                if (repository instanceof FileClassRepository)
                    return ((FileClassRepository)repository).getTargetClassFileName(nameHelper.getClassName());
          }
          return null;
    }

    /**
     * Set the current target class file name.
     * <p>
     * If nonnull, requests to compile source will result in one or
     * more class files being generated. If null, classes will only
     * be generated in memory.
     *
     * @since 1.3
     */
    public void setTargetClassFileName(String classFileName) {
        ClassNameHelper nameHelper = getNameHelper();
        if (nameHelper != null) {
            if (classFileName != null)
                nameHelper.setClassRepository(
                    new FileClassRepository(classFileName));
            else
                nameHelper.setClassName(null);
        }
    }

    /**
     * Get the current package to generate classes into.
     *
     * @since 1.3
     */
    public String getTargetPackage() {
        ClassNameHelper nameHelper = getNameHelper();
        return nameHelper != null ? nameHelper.getTargetPackage() : null;
    }

    /**
     * Set the package to generate classes into.
     *
     * @since 1.3
     */
    public void setTargetPackage(String targetPackage) {
        ClassNameHelper nameHelper = getNameHelper();
        if (nameHelper != null)
            nameHelper.setTargetPackage(targetPackage);
    }

    /**
     * Get the current class repository.
     *
     * @see ClassRepository
     * @since 30/10/01 tip + patch (Kemal Bayram)
     */
    public ClassRepository getClassRepository() {
        ClassNameHelper nameHelper = getNameHelper();
        return nameHelper != null ? nameHelper.getClassRepository() : null;
    }

    /**
     * Set the current class repository.
     *
     * @see ClassRepository
     * @since 30/10/01 tip + patch (Kemal Bayram)
     */
    public void setClassRepository(ClassRepository classRepository) {
        ClassNameHelper nameHelper = getNameHelper();
        if (nameHelper != null)
            nameHelper.setClassRepository(classRepository);
    }

    /**
     * Get the current interface to write class bytes into.
     *
     * @see ClassOutput
     * @since 1.5 Release 2
     */
    public ClassOutput getClassOutput() {
        ClassNameHelper nameHelper = getNameHelper();
        if (nameHelper != null) {
            ClassRepository repository = nameHelper.getClassRepository();
            if ((repository != null) &&
                (repository instanceof ClassOutputWrapper))
            {
                return ((ClassOutputWrapper)repository).classOutput;
            }
        }
        return null;
    }

    /**
     * Set the interface to write class bytes into.
     * Unless setTargetClassFileName() has been called classOutput will be
     * used each time the javascript compiler has generated the bytecode for a
     * script class.
     *
     * @see ClassOutput
     * @since 1.5 Release 2
     */
    public void setClassOutput(ClassOutput classOutput) {
        ClassNameHelper nameHelper = getNameHelper();
        if (nameHelper != null) {
            if (classOutput != null)
                nameHelper.setClassRepository(new ClassOutputWrapper(classOutput));
            else
                nameHelper.setClassRepository(null);
        }
    }

    /**
     * Set the security controller for this context.
     * <p> SecurityController may only be set if it is currently null.
     * Otherwise a SecurityException is thrown.
     * @param controller a SecurityController object
     * @throws SecurityException if there is already a SecurityController
     *         object for this Context
     */
    public void setSecurityController(SecurityController controller) {
        if (controller == null) throw new IllegalArgumentException();
        if (securityController != null) {
            throw new SecurityException("Cannot overwrite existing " +
                                        "SecurityController object");
        }
        securityController = controller;
    }

/**
@deprecated The {@link SecuritySupport} class is deprecated. See its documentation for the upgrade path.
*/
    public void setSecuritySupport(SecuritySupport x) {
        setClassShutter(x);
    }

    /**
     * Set the LiveConnect access filter for this context.
     * <p> {@link ClassShutter} may only be set if it is currently null.
     * Otherwise a SecurityException is thrown.
     * @param shutter a ClassShutter object
     * @throws SecurityException if there is already a ClassShutter
     *         object for this Context
     */
    public void setClassShutter(ClassShutter shutter) {
        if (shutter == null) throw new IllegalArgumentException();
        if (classShutter != null) {
            throw new SecurityException("Cannot overwrite existing " +
                                        "ClassShutter object");
        }
        classShutter = shutter;
    }

    ClassShutter getClassShutter() {
        return classShutter;
    }

    /**
     * Set the class that the generated target will extend.
     *
     * @param extendsClass the class it extends
     */
    public void setTargetExtends(Class extendsClass) {
        ClassNameHelper nameHelper = getNameHelper();
        if (nameHelper != null) {
            nameHelper.setTargetExtends(extendsClass);
        }
    }

    /**
     * Set the interfaces that the generated target will implement.
     *
     * @param implementsClasses an array of Class objects, one for each
     *                          interface the target will extend
     */
    public void setTargetImplements(Class[] implementsClasses) {
        ClassNameHelper nameHelper = getNameHelper();
        if (nameHelper != null) {
            nameHelper.setTargetImplements(implementsClasses);
        }
    }

    /**
     * Get a value corresponding to a key.
     * <p>
     * Since the Context is associated with a thread it can be
     * used to maintain values that can be later retrieved using
     * the current thread.
     * <p>
     * Note that the values are maintained with the Context, so
     * if the Context is disassociated from the thread the values
     * cannot be retreived. Also, if private data is to be maintained
     * in this manner the key should be a java.lang.Object
     * whose reference is not divulged to untrusted code.
     * @param key the key used to lookup the value
     * @return a value previously stored using putThreadLocal.
     */
    public Object getThreadLocal(Object key) {
        if (hashtable == null)
            return null;
        return hashtable.get(key);
    }

    /**
     * Put a value that can later be retrieved using a given key.
     * <p>
     * @param key the key used to index the value
     * @param value the value to save
     */
    public void putThreadLocal(Object key, Object value) {
        if (hashtable == null)
            hashtable = new Hashtable();
        hashtable.put(key, value);
    }

    /**
     * Remove values from thread-local storage.
     * @param key the key for the entry to remove.
     * @since 1.5 release 2
     */
    public void removeThreadLocal(Object key) {
        if (hashtable == null)
            return;
        hashtable.remove(key);
    }

    /**
     * Return whether functions are compiled by this context using
     * dynamic scope.
     * <p>
     * If functions are compiled with dynamic scope, then they execute
     * in the scope of their caller, rather than in their parent scope.
     * This is useful for sharing functions across multiple scopes.
     * @since 1.5 Release 1
     */
    public boolean hasCompileFunctionsWithDynamicScope() {
        return compileFunctionsWithDynamicScopeFlag;
    }

    /**
     * Set whether functions compiled by this context should use
     * dynamic scope.
     * <p>
     * @param flag if true, compile functions with dynamic scope
     * @since 1.5 Release 1
     */
    public void setCompileFunctionsWithDynamicScope(boolean flag) {
        compileFunctionsWithDynamicScopeFlag = flag;
    }

    /**
     * Set whether to cache some values statically.
     * <p>
     * By default, the engine will cache some values statically
     * (reflected Java classes, for instance). This can speed
     * execution dramatically, but increases the memory footprint.
     * Also, with caching enabled, references may be held to
     * objects past the lifetime of any real usage.
     * <p>
     * If caching is enabled and this method is called with a
     * <code>false</code> argument, the caches will be emptied.
     * So one strategy could be to clear the caches at times
     * appropriate to the application.
     * <p>
     * Caching is enabled by default.
     *
     * @param cachingEnabled if true, caching is enabled
     * @since 1.5 Release 1
     */
    public static void setCachingEnabled(boolean cachingEnabled) {
        if (isCachingEnabled && !cachingEnabled) {
            // Caching is being turned off. Empty caches.
            JavaMembers.classTable = new Hashtable();
            if (savedNameHelper != null)
                savedNameHelper.reset();
        }
        isCachingEnabled = cachingEnabled;
        FunctionObject.setCachingEnabled(cachingEnabled);
    }

    // Proxy to allow to use deprecated WrapHandler in place of WrapFactory
    private static class WrapHandlerProxy extends WrapFactory {
        WrapHandler _handler;

        WrapHandlerProxy(WrapHandler handler) {
            _handler = handler;
        }

        public Object wrap(Context cx, Scriptable scope,
                           Object obj, Class staticType)
        {
            if (obj == null) { return obj; }
            Object result = _handler.wrap(scope, obj, staticType);
            if (result == null) {
                result = super.wrap(cx, scope, obj, staticType);
            }
            return result;
        }

        public Scriptable wrapNewObject(Context cx, Scriptable scope,
                                        Object obj)
        {
            Object wrap = _handler.wrap(scope, obj, obj.getClass());
            if (wrap instanceof Scriptable) {
                return (Scriptable)wrap;
            }
            if (wrap == null) {
                return super.wrapNewObject(cx, scope, obj);
            }
            throw new RuntimeException
                ("Please upgrade from WrapHandler to WrapFactory");
        }
    }

    /**
     * @deprecated  As of Rhino 1.5 Release 4, use
     * {@link WrapFactory} and {@link #setWrapFactory(WrapFactory)}
     */
    public void setWrapHandler(WrapHandler wrapHandler) {
        if (wrapHandler == null) {
            setWrapFactory(new WrapFactory());
        } else {
            setWrapFactory(new WrapHandlerProxy(wrapHandler));
        }
    }

    /**
     * @deprecated  As of Rhino 1.5 Release 4, use
     * {@link WrapFactory} and {@link #getWrapFactory()}
     */
    public WrapHandler getWrapHandler() {
        WrapFactory f = getWrapFactory();
        if (f instanceof WrapHandlerProxy) {
            return ((WrapHandlerProxy)f)._handler;
        }
        return null;
    }

    /**
     * Set a WrapFactory for this Context.
     * <p>
     * The WrapFactory allows custom object wrapping behavior for
     * Java object manipulated with JavaScript.
     * @see org.mozilla.javascript.WrapFactory
     * @since 1.5 Release 4
     */
    public void setWrapFactory(WrapFactory wrapFactory) {
        if (wrapFactory == null) throw new IllegalArgumentException();
        this.wrapFactory = wrapFactory;
    }

    /**
     * Return the current WrapHandler, or null if none is defined.
     * @see org.mozilla.javascript.WrapHandler
     * @since 1.5 Release 4
     */
    public WrapFactory getWrapFactory() {
        if (wrapFactory == null) {
            wrapFactory = new WrapFactory();
        }
        return wrapFactory;
    }

    /**
     * Return the current debugger.
     * @return the debugger, or null if none is attached.
     */
    public Debugger getDebugger() {
        return debugger;
    }

    /**
     * Return the debugger context data associated with current context.
     * @return the debugger data, or null if debugger is not attached
     */
    public Object getDebuggerContextData() {
        return debuggerData;
    }

    /**
     * Set the associated debugger.
     * @param debugger the debugger to be used on callbacks from
     * the engine.
     * @param contextData arbitrary object that debugger can use to store
     *        per Context data.
     */
    public void setDebugger(Debugger debugger, Object contextData) {
        this.debugger = debugger;
        debuggerData = contextData;
    }

    /**
     * if hasFeature(FEATURE_NON_ECMA_GET_YEAR) returns true,
     * Date.prototype.getYear subtructs 1900 only if 1900 <= date < 2000
     * in deviation with Ecma B.2.4
     */
    public static final int FEATURE_NON_ECMA_GET_YEAR = 1;

    /**
     * if hasFeature(FEATURE_MEMBER_EXPR_AS_FUNCTION_NAME) returns true,
     * allow 'function <MemberExpression>(...) { ... }' to be syntax sugar for
     * '<MemberExpression> = function(...) { ... }', when <MemberExpression>
     * is not simply identifier.
     * See Ecma-262, section 11.2 for definition of <MemberExpression>
     */
    public static final int FEATURE_MEMBER_EXPR_AS_FUNCTION_NAME = 2;

    /**
     * if hasFeature(RESERVED_KEYWORD_AS_IDENTIFIER) returns true,
     * treat future reserved keyword (see  Ecma-262, section 7.5.3) as ordinary
     * identifiers but warn about this usage
     */
    public static final int FEATURE_RESERVED_KEYWORD_AS_IDENTIFIER = 3;

    /**
     * if hasFeature(FEATURE_TO_STRING_AS_SOURCE) returns true,
     * calling toString on JS objects gives JS source with code to create an
     * object with all enumeratable fields of the original object instead of
     * printing "[object <object-type>]".
     * By default {@link #hasFeature(int)} returns true only if
     * the current JS version is set to {@link #VERSION_1_2}.
     */
    public static final int FEATURE_TO_STRING_AS_SOURCE = 4;

    /**
     * Controls certain aspects of script semantics.
     * Should be overwritten to alter default behavior.
     * @param featureIndex feature index to check
     * @return true if the <code>featureIndex</code> feature is turned on
     * @see #FEATURE_NON_ECMA_GET_YEAR
     * @see #FEATURE_MEMBER_EXPR_AS_FUNCTION_NAME
     * @see #FEATURE_RESERVED_KEYWORD_AS_IDENTIFIER
     * @see #FEATURE_TO_STRING_AS_SOURCE
     */
    public boolean hasFeature(int featureIndex) {
        switch (featureIndex) {
            case FEATURE_NON_ECMA_GET_YEAR:
               /*
                * During the great date rewrite of 1.3, we tried to track the
                * evolving ECMA standard, which then had a definition of
                * getYear which always subtracted 1900.  Which we
                * implemented, not realizing that it was incompatible with
                * the old behavior...  now, rather than thrash the behavior
                * yet again, we've decided to leave it with the - 1900
                * behavior and point people to the getFullYear method.  But
                * we try to protect existing scripts that have specified a
                * version...
                */
                return (version == Context.VERSION_1_0
                        || version == Context.VERSION_1_1
                        || version == Context.VERSION_1_2);

            case FEATURE_MEMBER_EXPR_AS_FUNCTION_NAME:
                return false;

            case FEATURE_RESERVED_KEYWORD_AS_IDENTIFIER:
                return false;

            case FEATURE_TO_STRING_AS_SOURCE:
                return version == VERSION_1_2;
        }
        // It is a bug to call the method with unknown featureIndex
        throw new IllegalArgumentException();
    }

    /**
     * Get/Set threshold of executed instructions counter that triggers call to
     * <code>observeInstructionCount()</code>.
     * When the threshold is zero, instruction counting is disabled,
     * otherwise each time the run-time executes at least the threshold value
     * of script instructions, <code>observeInstructionCount()</code> will
     * be called.
     */
    public int getInstructionObserverThreshold() {
        return instructionThreshold;
    }

    public void setInstructionObserverThreshold(int threshold) {
        instructionThreshold = threshold;
    }

    /**
     * Allow application to monitor counter of executed script instructions
     * in Context subclasses.
     * Run-time calls this when instruction counting is enabled and the counter
     * reaches limit set by <code>setInstructionObserverThreshold()</code>.
     * The method is useful to observe long running scripts and if necessary
     * to terminate them.
     * @param instructionCount amount of script instruction executed since
     * last call to <code>observeInstructionCount</code>
     * @throws Error to terminate the script
     */
    protected void observeInstructionCount(int instructionCount) {}

    public GeneratedClassLoader createClassLoader(ClassLoader parent) {
        return new DefiningClassLoader(parent);

    }

    /********** end of API **********/

    static String getMessage0(String messageId) {
        return getMessage(messageId, null);
    }

    static String getMessage1(String messageId, Object arg1) {
        Object[] arguments = {arg1};
        return getMessage(messageId, arguments);
    }

    static String getMessage2(String messageId, Object arg1, Object arg2) {
        Object[] arguments = {arg1, arg2};
        return getMessage(messageId, arguments);
    }

    static String getMessage3
        (String messageId, Object arg1, Object arg2, Object arg3) {
        Object[] arguments = {arg1, arg2, arg3};
        return getMessage(messageId, arguments);
    }
    /**
     * Internal method that reports an error for missing calls to
     * enter().
     */
    static Context getContext() {
        Context cx = getCurrentContext();
        if (cx == null) {
            throw new RuntimeException(
                "No Context associated with current Thread");
        }
        return cx;
    }

    /* OPT there's a noticable delay for the first error!  Maybe it'd
     * make sense to use a ListResourceBundle instead of a properties
     * file to avoid (synchronized) text parsing.
     */
    static final String defaultResource =
        "org.mozilla.javascript.resources.Messages";

    static String getMessage(String messageId, Object[] arguments) {
        Context cx = getCurrentContext();
        Locale locale = cx != null ? cx.getLocale() : Locale.getDefault();

        // ResourceBundle does cacheing.
        ResourceBundle rb = ResourceBundle.getBundle(defaultResource, locale);

        String formatString;
        try {
            formatString = rb.getString(messageId);
        } catch (java.util.MissingResourceException mre) {
            throw new RuntimeException
                ("no message resource found for message property "+ messageId);
        }

        /*
         * It's OK to format the string, even if 'arguments' is null;
         * we need to format it anyway, to make double ''s collapse to
         * single 's.
         */
        // TODO: MessageFormat is not available on pJava
        MessageFormat formatter = new MessageFormat(formatString);
        return formatter.format(arguments);
    }

    // debug flags
    static final boolean printTrees = false;
    static final boolean printICode = false;

    /**
     * Compile a script.
     *
     * Reads script source from the reader and compiles it, returning
     * a class for either the script or the function depending on the
     * value of <code>returnFunction</code>.
     *
     * @param scope the scope to compile relative to
     * @param in the Reader to read source from
     * @param sourceName the name of the origin of the source (usually
     *                   a file or URL)
     * @param lineno the line number of the start of the source
     * @param securityDomain an arbitrary object that specifies security
     *        information about the origin or owner of the script. For
     *        implementations that don't care about security, this value
     *        may be null.
     * @param returnFunction if true, will expect the source to contain
     *                        a function; return value is assumed to
     *                        then be a org.mozilla.javascript.Function
     * @return a class for the script or function
     * @see org.mozilla.javascript.Context#compileReader
     */
    private Object compile(Scriptable scope, Reader in, String sourceName,
                           int lineno, Object securityDomain,
                           boolean returnFunction)
        throws IOException
    {
        Object dynamicDoamin = null;
        if (securityController != null) {
            dynamicDoamin = securityController.
                getDynamicSecurityDomain(securityDomain);
        }

        if (debugger != null && in != null) {
            in = new DebugReader(in);
        }
        TokenStream ts = new TokenStream(in, scope, sourceName, lineno);
        return compile(scope, ts, dynamicDoamin, in, returnFunction);
    }

    private static Class codegenClass;
    private static ClassNameHelper savedNameHelper;
    static {
        try {
            codegenClass = Class.forName(
                "org.mozilla.javascript.optimizer.Codegen");
        } catch (ClassNotFoundException x) {
            // ...must be running lite, that's ok
            codegenClass = null;
        }
    }

    private static ClassNameHelper getNameHelper() {
        if (savedNameHelper != null)
            return savedNameHelper;
        if (codegenClass == null)
            return null;
        try {
            Class nameHelperClass = Class.forName(
                "org.mozilla.javascript.optimizer.OptClassNameHelper");
            savedNameHelper = (ClassNameHelper)nameHelperClass.newInstance();
            return savedNameHelper;
        } catch (ClassNotFoundException x) {
            // ...must be running lite, that's ok
            return null;
        } catch (IllegalAccessException x) {
            return null;
        } catch (InstantiationException x) {
            return null;
        }
    }

    private Interpreter getCompiler() {
        if (codegenClass != null) {
            try {
                return (Interpreter) codegenClass.newInstance();
            }
            catch (SecurityException x) {
            }
            catch (IllegalArgumentException x) {
            }
            catch (InstantiationException x) {
            }
            catch (IllegalAccessException x) {
            }
            // fall through
        }
        return new Interpreter();
    }

    private Object compile(Scriptable scope, TokenStream ts,
                           Object dynamicSecurityDomain, Reader in,
                           boolean returnFunction)
        throws IOException
    {
        Interpreter compiler = optimizationLevel == -1
                               ? new Interpreter()
                               : getCompiler();
        ClassNameHelper nameHelper = optimizationLevel == -1
                                     ? null
                                     : getNameHelper();

        errorCount = 0;
        IRFactory irf = compiler.createIRFactory(ts, nameHelper, scope);
        Parser p = new Parser(irf);
        Node tree = (Node) p.parse(ts);
        if (tree == null)
            return null;

        tree = compiler.transform(tree, ts, scope);

        if (printTrees)
            System.out.println(tree.toStringTree());

        if (returnFunction) {
            Node first = tree.getFirstChild();
            if (first == null)
                return null;
            tree = (Node) first.getProp(Node.FUNCTION_PROP);
            if (tree == null)
                return null;
        }

        if (in instanceof DebugReader) {
            DebugReader dr = (DebugReader) in;
            tree.putProp(Node.DEBUGSOURCE_PROP, dr.getSaved());
        }

        Object result = compiler.compile(this, scope, tree,
                                         dynamicSecurityDomain,
                                         securityController,
                                         nameHelper);

        return errorCount == 0 ? result : null;
    }

    static String getSourcePositionFromStack(int[] linep) {
        Context cx = getCurrentContext();
        if (cx == null)
            return null;
        if (cx.interpreterLine > 0 && cx.interpreterSourceFile != null) {
            linep[0] = cx.interpreterLine;
            return cx.interpreterSourceFile;
        }
        /**
         * A bit of a hack, but the only way to get filename and line
         * number from an enclosing frame.
         */
        CharArrayWriter writer = new CharArrayWriter();
        RuntimeException re = new RuntimeException();
        re.printStackTrace(new PrintWriter(writer));
        String s = writer.toString();
        int open = -1;
        int close = -1;
        int colon = -1;
        for (int i=0; i < s.length(); i++) {
            char c = s.charAt(i);
            if (c == ':')
                colon = i;
            else if (c == '(')
                open = i;
            else if (c == ')')
                close = i;
            else if (c == '\n' && open != -1 && close != -1 && colon != -1 &&
                     open < colon && colon < close)
            {
                String fileStr = s.substring(open + 1, colon);
                if (!fileStr.endsWith(".java")) {
                    String lineStr = s.substring(colon + 1, close);
                    try {
                        linep[0] = Integer.parseInt(lineStr);
                        return fileStr;
                    }
                    catch (NumberFormatException e) {
                        // fall through
                    }
                }
                open = close = colon = -1;
            }
        }

        return null;
    }

    RegExpProxy getRegExpProxy() {
        if (regExpProxy == null) {
            try {
                Class c = Class.forName(
                            "org.mozilla.javascript.regexp.RegExpImpl");
                regExpProxy = (RegExpProxy) c.newInstance();
                return regExpProxy;
            } catch (ClassNotFoundException e) {
            } catch (InstantiationException e) {
            } catch (IllegalAccessException e) {
            }
        }
        return regExpProxy;
    }

    private void newArrayHelper(Scriptable scope, Scriptable array) {
        array.setParentScope(scope);
        Object ctor = ScriptRuntime.getTopLevelProp(scope, "Array");
        if (ctor != null && ctor instanceof Scriptable) {
            Scriptable s = (Scriptable) ctor;
            array.setPrototype((Scriptable) s.get("prototype", s));
        }
    }

    final boolean isVersionECMA1() {
        return version == VERSION_DEFAULT || version >= VERSION_1_3;
    }

// Should not be public
    SecurityController getSecurityController() {
        return securityController;
    }

    public boolean isGeneratingDebugChanged() {
        return generatingDebugChanged;
    }

    /**
     * Add a name to the list of names forcing the creation of real
     * activation objects for functions.
     *
     * @param name the name of the object to add to the list
     */
    public void addActivationName(String name) {
        if (activationNames == null)
            activationNames = new Hashtable(5);
        activationNames.put(name, name);
    }

    /**
     * Check whether the name is in the list of names of objects
     * forcing the creation of activation objects.
     *
     * @param name the name of the object to test
     *
     * @return true if an function activation object is needed.
     */
    public boolean isActivationNeeded(String name) {
        if ("arguments".equals(name))
            return true;
        return activationNames != null && activationNames.containsKey(name);
    }

    /**
     * Remove a name from the list of names forcing the creation of real
     * activation objects for functions.
     *
     * @param name the name of the object to remove from the list
     */
    public void removeActivationName(String name) {
        if (activationNames != null)
            activationNames.remove(name);
    }

// Rudimentary support for Design-by-Contract
    static void codeBug() {
        throw new RuntimeException("FAILED ASSERTION");
    }

    static final boolean check = true;

    static boolean isCachingEnabled = true;

    private static Hashtable threadContexts = new Hashtable(11);
    private static Object threadLocalCx;
    private static Method threadLocalGet;
    private static Method threadLocalSet;

    static {
        try {
            Class cl = Class.forName("java.lang.ThreadLocal");
            threadLocalGet = cl.getMethod("get", null);
            threadLocalSet = cl.getMethod("set",
                new Class[] { ScriptRuntime.ObjectClass });
            threadLocalCx = cl.newInstance();
        } catch (Exception ex) { }
    }

    private static final Object contextListenersLock = new Object();
    private static Object[] contextListeners;

    /**
     * The activation of the currently executing function or script.
     */
    NativeCall currentActivation;

    // for Objects, Arrays to tag themselves as being printed out,
    // so they don't print themselves out recursively.
    // Use ObjToIntMap instead of java.util.HashSet for JDK 1.1 compatibility
    ObjToIntMap iterating;

    Object interpreterSecurityDomain;

    int version;
    int errorCount;

    private SecurityController securityController;
    private ClassShutter classShutter;
    private ErrorReporter errorReporter;
    private RegExpProxy regExpProxy;
    private Locale locale;
    private boolean generatingDebug;
    private boolean generatingDebugChanged;
    private boolean generatingSource=true;
    private boolean compileFunctionsWithDynamicScopeFlag;
    private int optimizationLevel;
    private WrapFactory wrapFactory;
    Debugger debugger;
    private Object debuggerData;
    private int enterCount;
    private Object[] listeners;
    private Hashtable hashtable;

    /**
     * This is the list of names of objects forcing the creation of
     * function activation records.
     */
    private Hashtable activationNames;

    // For the interpreter to indicate line/source for error reports.
    int interpreterLine;
    String interpreterSourceFile;

    // For instruction counting (interpreter only)
    int instructionCount;
    int instructionThreshold;

    // Implement class file saving here instead of inside codegen.
    private class FileClassRepository implements ClassRepository {

        FileClassRepository(String classFileName) {
            int lastSeparator = classFileName.lastIndexOf(File.separatorChar);
            String initialName;
            if (lastSeparator == -1) {
                generatingDirectory = null;
                initialName = classFileName;
            } else {
                generatingDirectory = classFileName.substring(0, lastSeparator);
                initialName = classFileName.substring(lastSeparator+1);
            }
            if (initialName.endsWith(".class"))
                initialName = initialName.substring(0, initialName.length()-6);
            getNameHelper().setClassName(initialName);
        }

        public boolean storeClass(String className, byte[] bytes, boolean tl)
            throws IOException
        {
            // no "elegant" way of getting file name from fully
            // qualified class name.
            String targetPackage = getNameHelper().getTargetPackage();
            if ((targetPackage != null) && (targetPackage.length()>0) &&
                className.startsWith(targetPackage+"."))
            {
                className = className.substring(targetPackage.length()+1);
            }

            FileOutputStream out = new FileOutputStream(getTargetClassFileName(className));
            out.write(bytes);
            out.close();

            return false;
        }

        String getTargetClassFileName(String className) {
            StringBuffer sb = new StringBuffer();
            if (generatingDirectory != null) {
                sb.append(generatingDirectory);
                sb.append(File.separator);
            }
            sb.append(className);
            sb.append(".class");
            return sb.toString();
        }

        String generatingDirectory;
    };

    private static class ClassOutputWrapper implements ClassRepository {

        ClassOutputWrapper(ClassOutput classOutput) {
            this.classOutput = classOutput;
        }

        public boolean storeClass(String name, byte[] bytes, boolean tl)
            throws IOException
        {
            OutputStream out = classOutput.getOutputStream(name, tl);
            out.write(bytes);
            out.close();

            return true;
        }

        ClassOutput classOutput;
    }
}
