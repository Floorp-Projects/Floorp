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
 * Kemal Bayram
 * Patrick Beard
 * Norris Boyd
 * Igor Bukanov
 * Brendan Eich
 * Ethan Hugg
 * Roger Lawrence
 * Terry Lucas
 * Mike McCabe
 * Milen Nankov
 * Ian D. Stewart
 * Andi Vajda
 * Andrew Wason
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
import java.util.Hashtable;
import java.util.Locale;
import java.lang.reflect.*;
import org.mozilla.javascript.debug.*;

import org.mozilla.javascript.xml.XMLLib;

/**
 * This class represents the runtime context of an executing script.
 *
 * Before executing a script, an instance of Context must be created
 * and associated with the thread that will be executing the script.
 * The Context will be used to store information about the executing
 * of the script such as the call stack. Contexts are associated with
 * the current thread  using the {@link #call(ContextAction)}
 * or {@link #enter()} methods.<p>
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

public class Context
{
    /**
     * Language versions.
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
     * JavaScript 1.5
     */
    public static final int VERSION_1_6 =      160;

    /**
     * Controls behaviour of <tt>Date.prototype.getYear()</tt>.
     * If <tt>hasFeature(FEATURE_NON_ECMA_GET_YEAR)</tt> returns true,
     * Date.prototype.getYear subtructs 1900 only if 1900 <= date < 2000.
     * The default behavior of {@link #hasFeature(int)} is always to subtruct
     * 1900 as rquired by ECMAScript B.2.4.
     */
    public static final int FEATURE_NON_ECMA_GET_YEAR = 1;

    /**
     * Control if member expression as function name extension is available.
     * If <tt>hasFeature(FEATURE_MEMBER_EXPR_AS_FUNCTION_NAME)</tt> returns
     * true, allow <tt>function memberExpression(args) { body }</tt> to be
     * syntax sugar for <tt>memberExpression = function(args) { body }</tt>,
     * when memberExpression is not a simple identifier.
     * See ECMAScript-262, section 11.2 for definition of memberExpression.
     * By default {@link #hasFeature(int)} returns false.
     */
    public static final int FEATURE_MEMBER_EXPR_AS_FUNCTION_NAME = 2;

    /**
     * Control if reserved keywords are treated as identifiers.
     * If <tt>hasFeature(RESERVED_KEYWORD_AS_IDENTIFIER)</tt> returns true,
     * treat future reserved keyword (see  Ecma-262, section 7.5.3) as ordinary
     * identifiers but warn about this usage.
     *
     * By default {@link #hasFeature(int)} returns false.
     */
    public static final int FEATURE_RESERVED_KEYWORD_AS_IDENTIFIER = 3;

    /**
     * Control if <tt>toString()</tt> should returns the same result
     * as  <tt>toSource()</tt> when applied to objects and arrays.
     * If <tt>hasFeature(FEATURE_TO_STRING_AS_SOURCE)</tt> returns true,
     * calling <tt>toString()</tt> on JS objects gives the same result as
     * calling <tt>toSource()</tt>. That is it returns JS source with code
     * to create an object with all enumeratable fields of the original object
     * instead of printing <tt>[object <i>result of
     * {@link Scriptable#getClassName()}</i>]</tt>.
     * <p>
     * By default {@link #hasFeature(int)} returns true only if
     * the current JS version is set to {@link #VERSION_1_2}.
     */
    public static final int FEATURE_TO_STRING_AS_SOURCE = 4;

    /**
     * Control if properties <tt>__proto__</tt> and <tt>__parent__</tt>
     * are treated specially.
     * If <tt>hasFeature(FEATURE_PARENT_PROTO_PROPRTIES)</tt> returns true,
     * treat <tt>__parent__</tt> and <tt>__proto__</tt> as special properties.
     * <p>
     * The properties allow to query and set scope and prototype chains for the
     * objects. The special meaning of the properties is available
     * only when they are used as the right hand side of the dot operator.
     * For example, while <tt>x.__proto__ = y</tt> changes the prototype
     * chain of the object <tt>x</tt> to point to <tt>y</tt>,
     * <tt>x["__proto__"] = y</tt> simply assigns a new value to the property
     * <tt>__proto__</tt> in <tt>x</tt> even when the feature is on.
     *
     * By default {@link #hasFeature(int)} returns true.
     */
    public static final int FEATURE_PARENT_PROTO_PROPRTIES = 5;

    /**
     * Control if support for E4X(ECMAScript for XML) extension is available.
     * If hasFeature(FEATURE_E4X) returns true, the XML syntax is available.
     * <p>
     * By default {@link #hasFeature(int)} returns true if
     * the current JS version is set to {@link #VERSION_DEFAULT}
     * or is greater then {@link #VERSION_1_6}.
     * @since 1.6 Release 1
     */
    public static final int FEATURE_E4X = 6;

    /**
     * Control if dynamic scope should be used for name access.
     * If hasFeature(FEATURE_DYNAMIC_SCOPE) returns true, then the name lookup
     * during name resolution will use the top scope of the script or function
     * which is at the top of JS execution stack instead of the top scope of the
     * script or function from the current stack frame if the top scope of
     * the top stack frame contains the top scope of the current stack frame
     * on its prototype chain.
     * <p>
     * This is useful to define shared scope containing functions that can
     * be called from scripts and functions using private scopes.
     * <p>
     * By default {@link #hasFeature(int)} returns false.
     * @since 1.6 Release 1
     */
    public static final int FEATURE_DYNAMIC_SCOPE = 7;

    /**
     * Control if strict mode is enabled.
     * With strict mode enabled Rhino reports runtime errors in the following
     * cases:
     * <ul>
     * <li> Assignment to non-existing names which typically indicates missed
     *      <b>var</b> declaration.
     * <li> Passing non-string arguments to the eval function.
     * </ul>
     * <p>
     * By default {@link #hasFeature(int)} returns false.
     * @since 1.6 Release 1
     */
    public static final int FEATURE_STRICT_MODE = 8;

    public static final String languageVersionProperty = "language version";
    public static final String errorReporterProperty   = "error reporter";

    /**
     * Convinient value to use as zero-length array of objects.
     */
    public static final Object[] emptyArgs = ScriptRuntime.emptyArgs;

    /**
     * Create a new Context.
     *
     * Note that the Context must be associated with a thread before
     * it can be used to execute a script.
     *
     * @see #enter()
     * @see #call(ContextAction)
     */
    public Context()
    {
        setLanguageVersion(VERSION_DEFAULT);
        optimizationLevel = codegenClass != null ? 0 : -1;
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
     *      } finally {
     *          Context.exit();
     *      }
     * </pre>
     * Instead of using <tt>enter()</tt>, <tt>exit()</tt> pair consider using
     * {@link #call(ContextAction)} which guarantees proper
     * association of Context instances with the current thread and is faster.
     * With this method the above example becomes:
     * <pre>
     *      Context.call(new ContextAction() {
     *          public Object run(Context cx) {
     *              ...
     *              cx.evaluateString(...);
     *              return null;
     *          }
     *      });
     * </pre>
     *
     * @return a Context associated with the current thread
     * @see #getCurrentContext()
     * @see #exit()
     * @see #call(ContextAction)
     */
    public static Context enter()
    {
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
     *
     * @see #enter()
     * @see #call(ContextAction)
     * @see ContextFactory#call(ContextAction)
     */
    public static Context enter(Context cx)
    {
        Context[] storage = getThreadContextStorage();
        Context old;
        if (storage != null) {
            old = storage[0];
        } else {
            old = getCurrentContext_jdk11();
        }

        if (old != null) {
            if (old.factory != null) {
                throw new IllegalStateException("Context.enter can not be used to recursively enter Context instances already associated with the current thread using Context.call(ContextAction)");
            }
            if (cx != null && cx != old && cx.enterCount != 0) {
                // The suplied context must be the context for
                // the current thread if it is already entered
                throw new IllegalArgumentException(
                    "Cannot enter Context active on another thread");
            }
            if (old.sealed) onSealedMutation();
            cx = old;
        } else {
            if (cx == null) {
                cx = ContextFactory.getGlobal().makeContext();
            } else {
                if (cx.sealed) onSealedMutation();
            }
            if (cx.enterCount != 0 || cx.factory != null) {
                throw new IllegalStateException();
            }

            if (!cx.creationEventWasSent) {
                cx.creationEventWasSent = true;
                ContextFactory.getGlobal().onContextCreated(cx);
            }
        }

        if (old == null) {
            if (storage != null) {
                storage[0] = cx;
            } else {
                setThreadContext_jdk11(cx);
            }
        }
        ++cx.enterCount;

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
     * @see org.mozilla.javascript.Context#enter()
     * @see #call(ContextAction)
     * @see ContextFactory#call(ContextAction)
     */
    public static void exit()
    {
        Context[] storage = getThreadContextStorage();
        Context cx;
        if (storage != null) {
            cx = storage[0];
        } else {
            cx = getCurrentContext_jdk11();
        }
        if (cx == null) {
            throw new IllegalStateException(
                "Calling Context.exit without previous Context.enter");
        }
        if (cx.factory != null) {
            throw new IllegalStateException("Context.exit can not be used to exit context associated with the current thread using Context.call(ContextAction)");
        }
        if (cx.enterCount < 1) Kit.codeBug();
        if (cx.sealed) onSealedMutation();
        --cx.enterCount;
        if (cx.enterCount == 0) {
            if (storage != null) {
                storage[0] = null;
            } else {
                setThreadContext_jdk11(null);
            }
        }

        if (cx.enterCount == 0) {
            ContextFactory.getGlobal().onContextReleased(cx);
        }
    }

    /**
     * Call {@link ContextAction#run(Context cx)}
     * using the Context instance associated with the current thread.
     * If no Context is associated with the thread, then
     * <tt>ContextFactory.getGlobal().makeContext()</tt> will be called to
     * construct new Context instance. The instance will be temporary
     * associated with the thread during call to
     * {@link ContextAction#run(Context)}.
     *
     * @return The result of {@link ContextAction#run(Context)}.
     */
    public static Object call(ContextAction action)
    {
        return call(ContextFactory.getGlobal(), action);
    }

    /**
     * Call {@link
     * Callable#call(Context cx, Scriptable scope, Scriptable thisObj,
     *               Object[] args)}
     * using the Context instance associated with the current thread.
     * If no Context is associated with the thread, then
     * {@link ContextFactory#makeContext()} will be called to construct
     * new Context instance. The instance will be temporary associated
     * with the thread during call to {@link ContextAction#run(Context)}.
     * <p>
     * It is allowed to use null for <tt>factory</tt> argument
     * in which case the factory associated with the scope will be
     * used to create new context instances.
     *
     * @see ContextFactory#call(ContextAction)
     */
    public static Object call(ContextFactory factory, Callable callable,
                              Scriptable scope, Scriptable thisObj,
                              Object[] args)
    {
        if (factory == null) {
            factory = ScriptRuntime.getContextFactory(scope);
        }
        Context[] storage = getThreadContextStorage();
        Context cx;
        if (storage != null) {
            cx = storage[0];
        } else {
            cx = getCurrentContext_jdk11();
        }

        if (cx != null) {
            if (cx.factory != null) {
                return callable.call(cx, scope, thisObj, args);
            } else {
                cx.factory = factory;
                try {
                    return callable.call(cx, scope, thisObj, args);
                } finally {
                    cx.factory = null;
                }
            }
        }

        cx = prepareNewContext(factory, storage);
        try {
            return callable.call(cx, scope, thisObj, args);
        } finally {
            releaseContext(storage, cx);
        }
    }

    /**
     * The method implements {@links ContextFactory#call(ContextAction)} logic.
     */
    static Object call(ContextFactory factory, ContextAction action)
    {
        Context[] storage = getThreadContextStorage();
        Context cx;
        if (storage != null) {
            cx = storage[0];
        } else {
            cx = getCurrentContext_jdk11();
        }

        if (cx != null) {
            if (cx.factory != null) {
                return action.run(cx);
            } else {
                cx.factory = factory;
                try {
                    return action.run(cx);
                } finally {
                    cx.factory = null;
                }
            }
        }

        cx = prepareNewContext(factory, storage);
        try {
            return action.run(cx);
        } finally {
            releaseContext(storage, cx);
        }
    }

    private static Context prepareNewContext(ContextFactory factory,
                                             Context[] storage)
    {
        Context cx = factory.makeContext();
        if (cx.factory != null || cx.enterCount != 0) {
            throw new IllegalStateException("factory.makeContext() returned Context instance already associated with some thread");
        }
        cx.factory = factory;
        factory.onContextCreated(cx);
        if (factory.isSealed() && !cx.isSealed()) {
            cx.seal(null);
        }
        if (storage != null) {
            storage[0] = cx;
        } else {
            setThreadContext_jdk11(cx);
        }

        return cx;
    }

    private static void releaseContext(Context[] storage, Context cx)
    {
        if (storage != null) {
            storage[0] = null;
        } else {
            setThreadContext_jdk11(null);
        }
        try {
            cx.factory.onContextReleased(cx);
        } finally {
            cx.factory = null;
        }
    }

    /**
     * @deprecated Use
     * {@link ContextFactory#addListener(ContextFactory.Listener)}.
     * A simple way to upgrate the current code to new API is to replace
     * <tt>Context.addContextListener(listener)</tt> with
     * <tt>ContextFactory.getGlobal().addListener(listener)</tt>.
     */
    public static void addContextListener(ContextListener listener)
    {
        // Special workaround for the debugger
        String DBG = "org.mozilla.javascript.tools.debugger.Main";
        if (DBG.equals(listener.getClass().getName())) {
            Class cl = listener.getClass();
            Class factoryClass = Kit.classOrNull(
                "org.mozilla.javascript.ContextFactory");
            Class[] sig = { factoryClass };
            Object[] args = { ContextFactory.getGlobal() };
            try {
                Method m = cl.getMethod("attachTo", sig);
                m.invoke(listener, args);
            } catch (Exception ex) {
                RuntimeException rex = new RuntimeException();
                Kit.initCause(rex, ex);
                throw rex;
            }
            return;
        }

        ContextFactory.getGlobal().addListener(listener);
    }

    /**
     * @deprecated Use
     * {@link ContextFactory#removeListener(ContextFactory.Listener)}.
     * A simple way to upgrate the current code to new API is to replace
     * <tt>Context.removeContextListener(listener)</tt> with
     * <tt>ContextFactory.getGlobal().removeListener(listener)</tt>.
     */
    public static void removeContextListener(ContextListener listener)
    {
        ContextFactory.getGlobal().addListener(listener);
    }

    /**
     * @deprecated Use {@link ContextFactory#seal()} to seal
     * appropriate <tt>ContextFactory()</tt>.
     */
    public static void disableStaticContextListening()
    {
        ContextFactory.getGlobal().disableContextListening();
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
    public static Context getCurrentContext()
    {
        Context[] storage = getThreadContextStorage();
        if (storage != null) {
            return storage[0];
        }
        return getCurrentContext_jdk11();
    }

    private static Context[] getThreadContextStorage()
    {
        if (threadLocalCx != null) {
            try {
                Context[] storage
                    = (Context[])threadLocalGet.invoke(threadLocalCx, null);
                if (storage == null) {
                    storage = new Context[1];
                    threadLocalSet.invoke(threadLocalCx,
                                          new Object[] { storage });
                }
                return storage;
            } catch (Exception ex) { }
        }
        return null;
    }

    private static Context getCurrentContext_jdk11()
    {
        Thread t = Thread.currentThread();
        return (Context) threadContexts.get(t);
    }

    private static void setThreadContext_jdk11(Context cx)
    {
        Thread t = Thread.currentThread();
        if (cx != null) {
            threadContexts.put(t, cx);
        } else {
            threadContexts.remove(t);
        }
    }

    /**
     * Return {@link ContextFactory} instance used to create this Context
     * or the result of {@link ContextFactory#getGlobal()} if no factory
     * was used for Context creation.
     */
    public final ContextFactory getFactory()
    {
        ContextFactory result = factory;
        if (result == null) {
            result = ContextFactory.getGlobal();
        }
        return result;
    }

    /**
     * Checks if this is a sealed Context. A sealed Context instance does not
     * allow to modify any of its properties and will throw an exception
     * on any such attempt.
     * @see #seal(Object sealKey)
     */
    public final boolean isSealed()
    {
        return sealed;
    }

    /**
     * Seal this Context object so any attempt to modify any of its properties
     * including calling {@link #enter()} and {@link #exit()} methods will
     * throw an exception.
     * <p>
     * If <tt>sealKey</tt> is not null, calling
     * {@link #unseal(Object sealKey)} with the same key unseals
     * the object. If <tt>sealKey</tt> is null, unsealing is no longer possible.
     *
     * @see #isSealed()
     * @see #unseal(Object)
     */
    public final void seal(Object sealKey)
    {
        if (sealed) onSealedMutation();
        sealed = true;
        this.sealKey = sealKey;
    }

    /**
     * Unseal previously sealed Context object.
     * The <tt>sealKey</tt> argument should not be null and should match
     * <tt>sealKey</tt> suplied with the last call to
     * {@link #seal(Object)} or an exception will be thrown.
     *
     * @see #isSealed()
     * @see #seal(Object sealKey)
     */
    public final void unseal(Object sealKey)
    {
        if (sealKey == null) throw new IllegalArgumentException();
        if (this.sealKey != sealKey) throw new IllegalArgumentException();
        if (!sealed) throw new IllegalStateException();
        sealed = false;
        this.sealKey = null;
    }

    static void onSealedMutation()
    {
        throw new IllegalStateException();
    }

    /**
     * Get the current language version.
     * <p>
     * The language version number affects JavaScript semantics as detailed
     * in the overview documentation.
     *
     * @return an integer that is one of VERSION_1_0, VERSION_1_1, etc.
     */
    public final int getLanguageVersion()
    {
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
    public void setLanguageVersion(int version)
    {
        if (sealed) onSealedMutation();
        checkLanguageVersion(version);
        Object listeners = propertyListeners;
        if (listeners != null && version != this.version) {
            firePropertyChangeImpl(listeners, languageVersionProperty,
                               new Integer(this.version),
                               new Integer(version));
        }
        this.version = version;
    }

    public static boolean isValidLanguageVersion(int version)
    {
        switch (version) {
            case VERSION_DEFAULT:
            case VERSION_1_0:
            case VERSION_1_1:
            case VERSION_1_2:
            case VERSION_1_3:
            case VERSION_1_4:
            case VERSION_1_5:
            case VERSION_1_6:
                return true;
        }
        return false;
    }

    public static void checkLanguageVersion(int version)
    {
        if (isValidLanguageVersion(version)) {
            return;
        }
        throw new IllegalArgumentException("Bad language version: "+version);
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
    public final String getImplementationVersion()
    {
        // XXX Probably it would be better to embed this directly into source
        // with special build preprocessing but that would require some ant
        // tweaking and then replacing token in resource files was simpler
        if (implementationVersion == null) {
            implementationVersion
                = ScriptRuntime.getMessage0("implementation.version");
        }
        return implementationVersion;
    }

    /**
     * Get the current error reporter.
     *
     * @see org.mozilla.javascript.ErrorReporter
     */
    public final ErrorReporter getErrorReporter()
    {
        if (errorReporter == null) {
            return DefaultErrorReporter.instance;
        }
        return errorReporter;
    }

    /**
     * Change the current error reporter.
     *
     * @return the previous error reporter
     * @see org.mozilla.javascript.ErrorReporter
     */
    public final ErrorReporter setErrorReporter(ErrorReporter reporter)
    {
        if (sealed) onSealedMutation();
        if (reporter == null) throw new IllegalArgumentException();
        ErrorReporter old = getErrorReporter();
        if (reporter == old) {
            return old;
        }
        Object listeners = propertyListeners;
        if (listeners != null) {
            firePropertyChangeImpl(listeners, errorReporterProperty,
                                   old, reporter);
        }
        this.errorReporter = reporter;
        return old;
    }

    /**
     * Get the current locale.  Returns the default locale if none has
     * been set.
     *
     * @see java.util.Locale
     */

    public final Locale getLocale()
    {
        if (locale == null)
            locale = Locale.getDefault();
        return locale;
    }

    /**
     * Set the current locale.
     *
     * @see java.util.Locale
     */
    public final Locale setLocale(Locale loc)
    {
        if (sealed) onSealedMutation();
        Locale result = locale;
        locale = loc;
        return result;
    }

    /**
     * Register an object to receive notifications when a bound property
     * has changed
     * @see java.beans.PropertyChangeEvent
     * @see #removePropertyChangeListener(java.beans.PropertyChangeListener)
     * @param l the listener
     */
    public final void addPropertyChangeListener(PropertyChangeListener l)
    {
        if (sealed) onSealedMutation();
        propertyListeners = Kit.addListener(propertyListeners, l);
    }

    /**
     * Remove an object from the list of objects registered to receive
     * notification of changes to a bounded property
     * @see java.beans.PropertyChangeEvent
     * @see #addPropertyChangeListener(java.beans.PropertyChangeListener)
     * @param l the listener
     */
    public final void removePropertyChangeListener(PropertyChangeListener l)
    {
        if (sealed) onSealedMutation();
        propertyListeners = Kit.removeListener(propertyListeners, l);
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
    final void firePropertyChange(String property, Object oldValue,
                                  Object newValue)
    {
        Object listeners = propertyListeners;
        if (listeners != null) {
            firePropertyChangeImpl(listeners, property, oldValue, newValue);
        }
    }

    private void firePropertyChangeImpl(Object listeners, String property,
                                        Object oldValue, Object newValue)
    {
        for (int i = 0; ; ++i) {
            Object l = Kit.getListener(listeners, i);
            if (l == null)
                break;
            if (l instanceof PropertyChangeListener) {
                PropertyChangeListener pcl = (PropertyChangeListener)l;
                pcl.propertyChange(new PropertyChangeEvent(
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
    public static void reportWarning(String message)
    {
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
            cx.getErrorReporter().error(message, sourceName, lineno,
                                        lineSource, lineOffset);
        } else {
            throw new EvaluatorException(message, sourceName, lineno,
                                         lineSource, lineOffset);
        }
    }

    /**
     * Report an error using the error reporter for the current thread.
     *
     * @param message the error message to report
     * @see org.mozilla.javascript.ErrorReporter
     */
    public static void reportError(String message)
    {
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
            return cx.getErrorReporter().
                            runtimeError(message, sourceName, lineno,
                                         lineSource, lineOffset);
        } else {
            throw new EvaluatorException(message, sourceName, lineno,
                                         lineSource, lineOffset);
        }
    }

    static EvaluatorException reportRuntimeError0(String messageId)
    {
        String msg = ScriptRuntime.getMessage0(messageId);
        return reportRuntimeError(msg);
    }

    static EvaluatorException reportRuntimeError1(String messageId,
                                                  Object arg1)
    {
        String msg = ScriptRuntime.getMessage1(messageId, arg1);
        return reportRuntimeError(msg);
    }

    static EvaluatorException reportRuntimeError2(String messageId,
                                                  Object arg1, Object arg2)
    {
        String msg = ScriptRuntime.getMessage2(messageId, arg1, arg2);
        return reportRuntimeError(msg);
    }

    static EvaluatorException reportRuntimeError3(String messageId,
                                                  Object arg1, Object arg2,
                                                  Object arg3)
    {
        String msg = ScriptRuntime.getMessage3(messageId, arg1, arg2, arg3);
        return reportRuntimeError(msg);
    }

    static EvaluatorException reportRuntimeError4(String messageId,
                                                  Object arg1, Object arg2,
                                                  Object arg3, Object arg4)
    {
        String msg
            = ScriptRuntime.getMessage4(messageId, arg1, arg2, arg3, arg4);
        return reportRuntimeError(msg);
    }

    /**
     * Report a runtime error using the error reporter for the current thread.
     *
     * @param message the error message to report
     * @see org.mozilla.javascript.ErrorReporter
     */
    public static EvaluatorException reportRuntimeError(String message)
    {
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
     * @return the initialized scope
     */
    public final ScriptableObject initStandardObjects()
    {
        return initStandardObjects(null, false);
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
     * @return the initialized scope. The method returns the value of the scope
     *         argument if it is not null or newly allocated scope object which
     *         is an instance {@link ScriptableObject}.
     */
    public final Scriptable initStandardObjects(ScriptableObject scope)
    {
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
     * objects. An object that is sealed cannot have properties added, changed,
     * or removed. This is useful to create a "superglobal" that can be shared
     * among several top-level objects. Note that sealing is not allowed in
     * the current ECMA/ISO language specification, but is likely for
     * the next version.
     *
     * @param scope the scope to initialize, or null, in which case a new
     *        object will be created to serve as the scope
     * @param sealed whether or not to create sealed standard objects that
     *        cannot be modified.
     * @return the initialized scope. The method returns the value of the scope
     *         argument if it is not null or newly allocated scope object.
     * @since 1.4R3
     */
    public ScriptableObject initStandardObjects(ScriptableObject scope,
                                                boolean sealed)
    {
        return ScriptRuntime.initStandardObjects(this, scope, sealed);
    }

    /**
     * Get the singleton object that represents the JavaScript Undefined value.
     */
    public static Object getUndefinedValue()
    {
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
     * @see org.mozilla.javascript.SecurityController
     */
    public final Object evaluateString(Scriptable scope, String source,
                                       String sourceName, int lineno,
                                       Object securityDomain)
    {
        Script script = compileString(source, sourceName, lineno,
                                      securityDomain);
        if (script != null) {
            return script.exec(this, scope);
        } else {
            return null;
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
     */
    public final Object evaluateReader(Scriptable scope, Reader in,
                                       String sourceName, int lineno,
                                       Object securityDomain)
        throws IOException
    {
        Script script = compileReader(scope, in, sourceName, lineno,
                                      securityDomain);
        if (script != null) {
            return script.exec(this, scope);
        } else {
            return null;
        }
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
    public final boolean stringIsCompilableUnit(String source)
    {
        boolean errorseen = false;
        CompilerEnvirons compilerEnv = new CompilerEnvirons();
        compilerEnv.initFromContext(this);
        // no source name or source text manager, because we're just
        // going to throw away the result.
        compilerEnv.setGeneratingSource(false);
        Parser p = new Parser(compilerEnv, DefaultErrorReporter.instance);
        try {
            p.parse(source, null, 1);
        } catch (EvaluatorException ee) {
            errorseen = true;
        }
        // Return false only if an error occurred as a result of reading past
        // the end of the file, i.e. if the source could be fixed by
        // appending more source.
        if (errorseen && p.eof())
            return false;
        else
            return true;
    }

    /**
     * @deprecated Use
     * {@link #compileReader(Reader in, String sourceName, int lineno,
     *                       Object securityDomain)}.
     */
    public final Script compileReader(Scriptable scope, Reader in,
                                      String sourceName, int lineno,
                                      Object securityDomain)
        throws IOException
    {
        return compileReader(in, sourceName, lineno, securityDomain);
    }

    /**
     * Compiles the source in the given reader.
     * <p>
     * Returns a script that may later be executed.
     * Will consume all the source in the reader.
     *
     * @param in the input reader
     * @param sourceName a string describing the source, such as a filename
     * @param lineno the starting line number for reporting errors
     * @param securityDomain an arbitrary object that specifies security
     *        information about the origin or owner of the script. For
     *        implementations that don't care about security, this value
     *        may be null.
     * @return a script that may later be executed
     * @exception IOException if an IOException was generated by the Reader
     * @see org.mozilla.javascript.Script
     */
    public final Script compileReader(Reader in, String sourceName,
                                      int lineno, Object securityDomain)
        throws IOException
    {
        if (lineno < 0) {
            throw new IllegalArgumentException(
                "Line number can not be negative:"+lineno);
        }
        return (Script) compileImpl(null, in, null, sourceName, lineno,
                                    securityDomain, false, null, null);
    }

    /**
     * Compiles the source in the given string.
     * <p>
     * Returns a script that may later be executed.
     *
     * @param source the source string
     * @param sourceName a string describing the source, such as a filename
     * @param lineno the starting line number for reporting errors
     * @param securityDomain an arbitrary object that specifies security
     *        information about the origin or owner of the script. For
     *        implementations that don't care about security, this value
     *        may be null.
     * @return a script that may later be executed
     * @see org.mozilla.javascript.Script
     */
    public final Script compileString(String source,
                                      String sourceName, int lineno,
                                      Object securityDomain)
    {
        if (lineno < 0) {
            throw new IllegalArgumentException(
                "Line number can not be negative:"+lineno);
        }
        return compileString(source, null, null, sourceName, lineno,
                             securityDomain);
    }

    final Script compileString(String source,
                               Interpreter compiler,
                               ErrorReporter compilationErrorReporter,
                               String sourceName, int lineno,
                               Object securityDomain)
    {
        try {
            return (Script) compileImpl(null, null, source, sourceName, lineno,
                                        securityDomain, false,
                                        compiler, compilationErrorReporter);
        } catch (IOException ex) {
            // Should not happen when dealing with source as string
            throw new RuntimeException();
        }
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
    public final Function compileFunction(Scriptable scope, String source,
                                          String sourceName, int lineno,
                                          Object securityDomain)
    {
        return compileFunction(scope, source, null, null, sourceName, lineno,
                               securityDomain);
    }

    final Function compileFunction(Scriptable scope, String source,
                                   Interpreter compiler,
                                   ErrorReporter compilationErrorReporter,
                                   String sourceName, int lineno,
                                   Object securityDomain)
    {
        try {
            return (Function) compileImpl(scope, null, source, sourceName,
                                          lineno, securityDomain, true,
                                          compiler, compilationErrorReporter);
        }
        catch (IOException ioe) {
            // Should never happen because we just made the reader
            // from a String
            throw new RuntimeException();
        }
    }

    /**
     * @deprecated Use {@link #decompileScript(Script script, int indent)}
     *             instead.
     * <p>
     * The implementation simply calls <tt>decompileScript(script, indent)</tt>
     * ignoring the scope argument.
     */
    public final String decompileScript(Script script, Scriptable scope,
                                        int indent)
    {
        return decompileScript(script, indent);
    }

    /**
     * Decompile the script.
     * <p>
     * The canonical source of the script is returned.
     *
     * @param script the script to decompile
     * @param indent the number of spaces to indent the result
     * @return a string representing the script source
     */
    public final String decompileScript(Script script, int indent)
    {
        NativeFunction scriptImpl = (NativeFunction) script;
        return scriptImpl.decompile(indent, 0);
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
    public final String decompileFunction(Function fun, int indent)
    {
        if (fun instanceof BaseFunction)
            return ((BaseFunction)fun).decompile(indent, 0);
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
    public final String decompileFunctionBody(Function fun, int indent)
    {
        if (fun instanceof BaseFunction) {
            BaseFunction bf = (BaseFunction)fun;
            return bf.decompile(indent, Decompiler.ONLY_BODY_FLAG);
        }
        // ALERT: not sure what the right response here is.
        return "[native code]\n";
    }

    /**
     * Create a new JavaScript object.
     *
     * Equivalent to evaluating "new Object()".
     * @param scope the scope to search for the constructor and to evaluate
     *              against
     * @return the new object
     */
    public final Scriptable newObject(Scriptable scope)
    {
        return newObject(scope, "Object", ScriptRuntime.emptyArgs);
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
     */
    public final Scriptable newObject(Scriptable scope, String constructorName)
    {
        return newObject(scope, constructorName, ScriptRuntime.emptyArgs);
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
     */
    public final Scriptable newObject(Scriptable scope, String constructorName,
                                      Object[] args)
    {
        scope = ScriptableObject.getTopLevelScope(scope);
        Function ctor = ScriptRuntime.getExistingCtor(this, scope,
                                                      constructorName);
        if (args == null) { args = ScriptRuntime.emptyArgs; }
        return ctor.construct(this, scope, args);
    }

    /**
     * Create an array with a specified initial length.
     * <p>
     * @param scope the scope to create the object in
     * @param length the initial length (JavaScript arrays may have
     *               additional properties added dynamically).
     * @return the new array object
     */
    public final Scriptable newArray(Scriptable scope, int length)
    {
        NativeArray result = new NativeArray(length);
        ScriptRuntime.setObjectProtoAndParent(result, scope);
        return result;
    }

    /**
     * Create an array with a set of initial elements.
     *
     * @param scope the scope to create the object in.
     * @param elements the initial elements. Each object in this array
     *                 must be an acceptable JavaScript type and type
     *                 of array should be exactly Object[], not
     *                 SomeObjectSubclass[].
     * @return the new array object.
     */
    public final Scriptable newArray(Scriptable scope, Object[] elements)
    {
        if (elements.getClass().getComponentType() != ScriptRuntime.ObjectClass)
            throw new IllegalArgumentException();
        NativeArray result = new NativeArray(elements);
        ScriptRuntime.setObjectProtoAndParent(result, scope);
        return result;
    }

    /**
     * Get the elements of a JavaScript array.
     * <p>
     * If the object defines a length property convertible to double number,
     * then the number is converted Uint32 value as defined in Ecma 9.6
     * and Java array of that size is allocated.
     * The array is initialized with the values obtained by
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
    public final Object[] getElements(Scriptable object)
    {
        return ScriptRuntime.getArrayElements(object);
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
    public static boolean toBoolean(Object value)
    {
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
    public static double toNumber(Object value)
    {
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
    public static String toString(Object value)
    {
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
    public static Scriptable toObject(Object value, Scriptable scope)
    {
        return ScriptRuntime.toObject(scope, value);
    }

    /**
     * @deprecated Use {@link #toObject(Object, Scriptable)}.
     */
    public static Scriptable toObject(Object value, Scriptable scope,
                                      Class staticType)
    {
        return ScriptRuntime.toObject(scope, value);
    }

    /**
     * Convenient method to convert java value to its closest representation
     * in JavaScript.
     * <p>
     * If value is an instance of String, Number, Boolean, Function or
     * Scriptable, it is returned as it and will be treated as the corresponding
     * JavaScript type of string, number, boolean, function and object.
     * <p>
     * Note that for Number instances during any arithmetic operation in
     * JavaScript the engine will always use the result of
     * <tt>Number.doubleValue()</tt> resulting in a precision loss if
     * the number can not fit into double.
     * <p>
     * If value is an instance of Character, it will be converted to string of
     * length 1 and its JavaScript type will be string.
     * <p>
     * The rest of values will be wrapped as LiveConnect objects
     * by calling {@link WrapFactory#wrap(Context cx, Scriptable scope,
     * Object obj, Class staticType)} as in:
     * <pre>
     *    Context cx = Context.getCurrentContext();
     *    return cx.getWrapFactory().wrap(cx, scope, value, null);
     * </pre>
     *
     * @param value any Java object
     * @param scope top scope object
     * @return value suitable to pass to any API that takes JavaScript values.
     */
    public static Object javaToJS(Object value, Scriptable scope)
    {
        if (value instanceof String || value instanceof Number
            || value instanceof Boolean || value instanceof Scriptable)
        {
            return value;
        } else if (value instanceof Character) {
            return String.valueOf(((Character)value).charValue());
        } else {
            Context cx = Context.getContext();
            return cx.getWrapFactory().wrap(cx, scope, value, null);
        }
    }

    /**
     * Convert a JavaScript value into the desired type.
     * Uses the semantics defined with LiveConnect3 and throws an
     * Illegal argument exception if the conversion cannot be performed.
     * @param value the JavaScript value to convert
     * @param desiredType the Java type to convert to. Primitive Java
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
     * Rethrow the exception wrapping it as the script runtime exception.
     * Unless the exception is instance of {@link EcmaError} or
     * {@link EvaluatorException} it will be wrapped as
     * {@link WrappedException}, a subclass of {@link EvaluatorException}.
     * The resulting exception object always contains
     * source name and line number of script that triggered exception.
     * <p>
     * This method always throws an exception, its return value is provided
     * only for convenience to allow a usage like:
     * <pre>
     * throw Context.throwAsScriptRuntimeEx(ex);
     * </pre>
     * to indicate that code after the method is unreachable.
     * @throws EvaluatorException
     * @throws EcmaError
     */
    public static RuntimeException throwAsScriptRuntimeEx(Throwable e)
    {
        while ((e instanceof InvocationTargetException)) {
            e = ((InvocationTargetException) e).getTargetException();
        }
        // special handling of Error so scripts would not catch them
        if (e instanceof Error) {
            throw (Error)e;
        }
        if (e instanceof EvaluatorException) {
            throw (EvaluatorException)e;
        }
        if (e instanceof EcmaError) {
            throw (EcmaError)e;
        }
        throw new WrappedException(e);
    }

    /**
     * Tell whether debug information is being generated.
     * @since 1.3
     */
    public final boolean isGeneratingDebug()
    {
        return generatingDebug;
    }

    /**
     * Specify whether or not debug information should be generated.
     * <p>
     * Setting the generation of debug information on will set the
     * optimization level to zero.
     * @since 1.3
     */
    public final void setGeneratingDebug(boolean generatingDebug)
    {
        if (sealed) onSealedMutation();
        generatingDebugChanged = true;
        if (generatingDebug && getOptimizationLevel() > 0)
            setOptimizationLevel(0);
        this.generatingDebug = generatingDebug;
    }

    /**
     * Tell whether source information is being generated.
     * @since 1.3
     */
    public final boolean isGeneratingSource()
    {
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
    public final void setGeneratingSource(boolean generatingSource)
    {
        if (sealed) onSealedMutation();
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
    public final int getOptimizationLevel()
    {
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
    public final void setOptimizationLevel(int optimizationLevel)
    {
        if (sealed) onSealedMutation();
        if (optimizationLevel == -2) {
            // To be compatible with Cocoon fork
            optimizationLevel = -1;
        }
        checkOptimizationLevel(optimizationLevel);
        if (codegenClass == null)
            optimizationLevel = -1;
        this.optimizationLevel = optimizationLevel;
    }

    public static boolean isValidOptimizationLevel(int optimizationLevel)
    {
        return -1 <= optimizationLevel && optimizationLevel <= 9;
    }

    public static void checkOptimizationLevel(int optimizationLevel)
    {
        if (isValidOptimizationLevel(optimizationLevel)) {
            return;
        }
        throw new IllegalArgumentException(
            "Optimization level outside [-1..9]: "+optimizationLevel);
    }

    /**
     * Set the security controller for this context.
     * <p> SecurityController may only be set if it is currently null
     * and {@link SecurityController#hasGlobal()} is <tt>false</tt>.
     * Otherwise a SecurityException is thrown.
     * @param controller a SecurityController object
     * @throws SecurityException if there is already a SecurityController
     *         object for this Context or globally installed.
     * @see SecurityController#initGlobal(SecurityController controller)
     * @see SecurityController#hasGlobal()
     */
    public final void setSecurityController(SecurityController controller)
    {
        if (sealed) onSealedMutation();
        if (controller == null) throw new IllegalArgumentException();
        if (securityController != null) {
            throw new SecurityException("Can not overwrite existing SecurityController object");
        }
        if (SecurityController.hasGlobal()) {
            throw new SecurityException("Can not overwrite existing global SecurityController object");
        }
        securityController = controller;
    }

    /**
     * Set the LiveConnect access filter for this context.
     * <p> {@link ClassShutter} may only be set if it is currently null.
     * Otherwise a SecurityException is thrown.
     * @param shutter a ClassShutter object
     * @throws SecurityException if there is already a ClassShutter
     *         object for this Context
     */
    public final void setClassShutter(ClassShutter shutter)
    {
        if (sealed) onSealedMutation();
        if (shutter == null) throw new IllegalArgumentException();
        if (classShutter != null) {
            throw new SecurityException("Cannot overwrite existing " +
                                        "ClassShutter object");
        }
        classShutter = shutter;
    }

    final ClassShutter getClassShutter()
    {
        return classShutter;
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
    public final Object getThreadLocal(Object key)
    {
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
    public final void putThreadLocal(Object key, Object value)
    {
        if (sealed) onSealedMutation();
        if (hashtable == null)
            hashtable = new Hashtable();
        hashtable.put(key, value);
    }

    /**
     * Remove values from thread-local storage.
     * @param key the key for the entry to remove.
     * @since 1.5 release 2
     */
    public final void removeThreadLocal(Object key)
    {
        if (sealed) onSealedMutation();
        if (hashtable == null)
            return;
        hashtable.remove(key);
    }

    /**
     * @deprecated Use {@link #hasFeature(int)} and
     * {@link #FEATURE_DYNAMIC_SCOPE} to control dynamic scoping that also works
     * with nested functions defined by functions in the shared scope.
     */
    public final boolean hasCompileFunctionsWithDynamicScope()
    {
        return compileFunctionsWithDynamicScopeFlag;
    }

    /**
     * @deprecated Use {@link #hasFeature(int)} and
     * {@link #FEATURE_DYNAMIC_SCOPE} to control dynamic scoping that also works
     * with nested functions defined by functions in the shared scope.
     */
    public final void setCompileFunctionsWithDynamicScope(boolean flag)
    {
        if (sealed) onSealedMutation();
        compileFunctionsWithDynamicScopeFlag = flag;
    }

    /**
     * @deprecated Use {@link ClassCache#get(Scriptable)} and
     * {@link ClassCache#setCachingEnabled(boolean)}.
     */
    public static void setCachingEnabled(boolean cachingEnabled)
    {
    }

    /**
     * @deprecated Proxy to allow to use deprecated WrapHandler in place
     * of WrapFactory.
     */
    private static class WrapHandlerProxy extends WrapFactory
    {
        WrapHandler _handler;

        /**
         * @deprecated
         */
        WrapHandlerProxy(WrapHandler handler)
        {
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
     * @deprecated  Use {@link WrapFactory} and {@link #setWrapFactory(WrapFactory)}.
     */
    public final void setWrapHandler(WrapHandler wrapHandler)
    {
        if (sealed) onSealedMutation();
        if (wrapHandler == null) {
            setWrapFactory(new WrapFactory());
        } else {
            setWrapFactory(new WrapHandlerProxy(wrapHandler));
        }
    }

    /**
     * @deprecated  Use {@link WrapFactory} and {@link #getWrapFactory()}.
     */
    public final WrapHandler getWrapHandler()
    {
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
    public final void setWrapFactory(WrapFactory wrapFactory)
    {
        if (sealed) onSealedMutation();
        if (wrapFactory == null) throw new IllegalArgumentException();
        this.wrapFactory = wrapFactory;
    }

    /**
     * Return the current WrapHandler, or null if none is defined.
     * @see org.mozilla.javascript.WrapHandler
     * @since 1.5 Release 4
     */
    public final WrapFactory getWrapFactory()
    {
        if (wrapFactory == null) {
            wrapFactory = new WrapFactory();
        }
        return wrapFactory;
    }

    /**
     * Return the current debugger.
     * @return the debugger, or null if none is attached.
     */
    public final Debugger getDebugger()
    {
        return debugger;
    }

    /**
     * Return the debugger context data associated with current context.
     * @return the debugger data, or null if debugger is not attached
     */
    public final Object getDebuggerContextData()
    {
        return debuggerData;
    }

    /**
     * Set the associated debugger.
     * @param debugger the debugger to be used on callbacks from
     * the engine.
     * @param contextData arbitrary object that debugger can use to store
     *        per Context data.
     */
    public final void setDebugger(Debugger debugger, Object contextData)
    {
        if (sealed) onSealedMutation();
        this.debugger = debugger;
        debuggerData = contextData;
    }

    /**
     * Return DebuggableScript instance if any associated with the script.
     * If callable supports DebuggableScript implementation, the method
     * returns it. Otherwise null is returned.
     */
    public static DebuggableScript getDebuggableView(Script script)
    {
        if (script instanceof NativeFunction) {
            return ((NativeFunction)script).getDebuggableView();
        }
        return null;
    }

    /**
     * Controls certain aspects of script semantics.
     * Should be overwritten to alter default behavior.
     * <p>
     * The default implementation calls
     * {@link ContextFactory#hasFeature(Context cx, int featureIndex)}
     * that allows to customize Context behavior without introducing
     * Context subclasses.  {@link ContextFactory} documentation gives
     * an example of hasFeature implementation.
     *
     * @param featureIndex feature index to check
     * @return true if the <code>featureIndex</code> feature is turned on
     * @see #FEATURE_NON_ECMA_GET_YEAR
     * @see #FEATURE_MEMBER_EXPR_AS_FUNCTION_NAME
     * @see #FEATURE_RESERVED_KEYWORD_AS_IDENTIFIER
     * @see #FEATURE_TO_STRING_AS_SOURCE
     * @see #FEATURE_PARENT_PROTO_PROPRTIES
     * @see #FEATURE_E4X
     * @see #FEATURE_DYNAMIC_SCOPE
     * @see #FEATURE_STRICT_MODE
     */
    public boolean hasFeature(int featureIndex)
    {
        ContextFactory f = getFactory();
        return f.hasFeature(this, featureIndex);
    }

    /**
     * Get/Set threshold of executed instructions counter that triggers call to
     * <code>observeInstructionCount()</code>.
     * When the threshold is zero, instruction counting is disabled,
     * otherwise each time the run-time executes at least the threshold value
     * of script instructions, <code>observeInstructionCount()</code> will
     * be called.
     */
    public final int getInstructionObserverThreshold()
    {
        return instructionThreshold;
    }

    public final void setInstructionObserverThreshold(int threshold)
    {
        if (sealed) onSealedMutation();
        if (threshold < 0) throw new IllegalArgumentException();
        instructionThreshold = threshold;
    }

    /**
     * Allow application to monitor counter of executed script instructions
     * in Context subclasses.
     * Run-time calls this when instruction counting is enabled and the counter
     * reaches limit set by <code>setInstructionObserverThreshold()</code>.
     * The method is useful to observe long running scripts and if necessary
     * to terminate them.
     * <p>
     * The instruction counting support is available only for interpreted
     * scripts generated when the optimization level is set to -1.
     * <p>
     * The default implementation calls
     * {@link ContextFactory#observeInstructionCount(Context cx,
     *                                               int instructionCount)}
     * that allows to customize Context behavior without introducing
     * Context subclasses.
     *
     * @param instructionCount amount of script instruction executed since
     * last call to <code>observeInstructionCount</code>
     * @throws Error to terminate the script
     * @see #setOptimizationLevel(int)
     */
    protected void observeInstructionCount(int instructionCount)
    {
        ContextFactory f = getFactory();
        f.observeInstructionCount(this, instructionCount);
    }

    /**
     * Create class loader for generated classes.
     * The method calls {@link ContextFactory#createClassLoader(ClassLoader)}
     * using the result of {@link #getFactory()}.
     */
    public GeneratedClassLoader createClassLoader(ClassLoader parent)
    {
        ContextFactory f = getFactory();
        return f.createClassLoader(parent);
    }

    public final ClassLoader getApplicationClassLoader()
    {
        if (applicationClassLoader == null) {
            // If Context was subclassed, the following gets the loader
            // for the subclass which can be different from Rhino loader,
            // but then proper Rhino classes should be accessible through it
            // in any case or JVM class loading is severely broken
            Class cxClass = this.getClass();
            ClassLoader loader = cxClass.getClassLoader();
            if (method_getContextClassLoader != null) {
                Thread thread = Thread.currentThread();
                ClassLoader threadLoader = null;
                try {
                    threadLoader = (ClassLoader)method_getContextClassLoader.
                                       invoke(thread, ScriptRuntime.emptyArgs);
                } catch (Exception ex) { }
                if (threadLoader != null && threadLoader != loader) {
                    if (testIfCanUseLoader(threadLoader, cxClass)) {
                        // Thread.getContextClassLoader is not cached since
                        // its caching prevents it from GC which may lead to
                        // a memory leak and hides updates to
                        // Thread.getContextClassLoader
                        return threadLoader;
                    }
                }
            }
            applicationClassLoader = loader;
        }
        return applicationClassLoader;
    }

    public final void setApplicationClassLoader(ClassLoader loader)
    {
        if (sealed) onSealedMutation();
        if (loader == null) {
            // restore default behaviour
            applicationClassLoader = null;
            return;
        }
        if (!testIfCanUseLoader(loader, this.getClass())) {
            throw new IllegalArgumentException(
                "Loader can not resolve Rhino classes");
        }
        applicationClassLoader = loader;
    }

    private static boolean testIfCanUseLoader(ClassLoader loader, Class cxClass)
    {
        // Check that Context or its suclass is accesible from this loader
        Class x = Kit.classOrNull(loader, cxClass.getName());
        if (x != cxClass) {
            // The check covers the case when x == null =>
            // loader does not know about Rhino or the case
            // when x != null && x != cxClass =>
            // loader loads unrelated Rhino instance
            return false;
        }
        return true;
    }

    /********** end of API **********/

    /**
     * Internal method that reports an error for missing calls to
     * enter().
     */
    static Context getContext()
    {
        Context cx = getCurrentContext();
        if (cx == null) {
            throw new RuntimeException(
                "No Context associated with current Thread");
        }
        return cx;
    }

    private Object compileImpl(Scriptable scope,
                               Reader sourceReader, String sourceString,
                               String sourceName, int lineno,
                               Object securityDomain, boolean returnFunction,
                               Interpreter compiler,
                               ErrorReporter compilationErrorReporter)
        throws IOException
    {
        if (securityDomain != null && securityController == null) {
            throw new IllegalArgumentException(
                "securityDomain should be null if setSecurityController() was never called");
        }

        // One of sourceReader or sourceString has to be null
        if (!(sourceReader == null ^ sourceString == null)) Kit.codeBug();
        // scope should be given if and only if compiling function
        if (!(scope == null ^ returnFunction)) Kit.codeBug();

        CompilerEnvirons compilerEnv = new CompilerEnvirons();
        compilerEnv.initFromContext(this);
        if (compilationErrorReporter == null) {
            compilationErrorReporter = compilerEnv.getErrorReporter();
        }

        if (debugger != null) {
            if (sourceReader != null) {
                sourceString = Kit.readReader(sourceReader);
                sourceReader = null;
            }
        }

        Parser p = new Parser(compilerEnv, compilationErrorReporter);
        ScriptOrFnNode tree;
        if (sourceString != null) {
            tree = p.parse(sourceString, sourceName, lineno);
        } else {
            tree = p.parse(sourceReader, sourceName, lineno);
        }
        if (returnFunction) {
            if (!(tree.getFunctionCount() == 1
                  && tree.getFirstChild() != null
                  && tree.getFirstChild().getType() == Token.FUNCTION))
            {
                // XXX: the check just look for the first child
                // and allows for more nodes after it for compatibility
                // with sources like function() {};;;
                throw new IllegalArgumentException(
                    "compileFunction only accepts source with single JS function: "+sourceString);
            }
        }

        if (compiler == null) {
            compiler = createCompiler();
        }

        String encodedSource = p.getEncodedSource();

        Object bytecode = compiler.compile(compilerEnv,
                                           tree, encodedSource,
                                           returnFunction);

        if (debugger != null) {
            if (sourceString == null) Kit.codeBug();
            if (bytecode instanceof DebuggableScript) {
                DebuggableScript dscript = (DebuggableScript)bytecode;
                notifyDebugger_r(this, dscript, sourceString);
            } else {
                throw new RuntimeException("NOT SUPPORTED");
            }
        }

        Object result;
        if (returnFunction) {
            result = compiler.createFunctionObject(this, scope, bytecode,
                                                   securityDomain);
        } else {
            result = compiler.createScriptObject(bytecode, securityDomain);
        }

        return result;
    }

    private static void notifyDebugger_r(Context cx, DebuggableScript dscript,
                                         String debugSource)
    {
        cx.debugger.handleCompilationDone(cx, dscript, debugSource);
        for (int i = 0; i != dscript.getFunctionCount(); ++i) {
            notifyDebugger_r(cx, dscript.getFunction(i), debugSource);
        }
    }

    private static Class codegenClass = Kit.classOrNull(
                             "org.mozilla.javascript.optimizer.Codegen");

    private Interpreter createCompiler()
    {
        Interpreter result = null;
        if (optimizationLevel >= 0 && codegenClass != null) {
            result = (Interpreter)Kit.newInstanceOrNull(codegenClass);
        }
        if (result == null) {
            result = new Interpreter();
        }
        return result;
    }

    static String getSourcePositionFromStack(int[] linep)
    {
        Context cx = getCurrentContext();
        if (cx == null)
            return null;
        if (cx.interpreterLineCounting != null) {
            return Interpreter.getSourcePositionFromStack(cx, linep);
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
                        if (linep[0] < 0) {
                            linep[0] = 0;
                        }
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

    RegExpProxy getRegExpProxy()
    {
        if (regExpProxy == null) {
            Class cl = Kit.classOrNull(
                          "org.mozilla.javascript.regexp.RegExpImpl");
            if (cl != null) {
                regExpProxy = (RegExpProxy)Kit.newInstanceOrNull(cl);
            }
        }
        return regExpProxy;
    }

    final boolean isVersionECMA1()
    {
        return version == VERSION_DEFAULT || version >= VERSION_1_3;
    }

// The method must NOT be public or protected
    SecurityController getSecurityController()
    {
        SecurityController global = SecurityController.global();
        if (global != null) {
            return global;
        }
        return securityController;
    }

    public final boolean isGeneratingDebugChanged()
    {
        return generatingDebugChanged;
    }

    /**
     * Add a name to the list of names forcing the creation of real
     * activation objects for functions.
     *
     * @param name the name of the object to add to the list
     */
    public void addActivationName(String name)
    {
        if (sealed) onSealedMutation();
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
    public final boolean isActivationNeeded(String name)
    {
        return activationNames != null && activationNames.containsKey(name);
    }

    /**
     * Remove a name from the list of names forcing the creation of real
     * activation objects for functions.
     *
     * @param name the name of the object to remove from the list
     */
    public void removeActivationName(String name)
    {
        if (sealed) onSealedMutation();
        if (activationNames != null)
            activationNames.remove(name);
    }

    private static Hashtable threadContexts = new Hashtable(11);
    private static Object threadLocalCx;
    private static Method threadLocalGet;
    private static Method threadLocalSet;

    static {
        Class cl = Kit.classOrNull("java.lang.ThreadLocal");
        if (cl != null) {
            try {
                threadLocalGet = cl.getMethod("get", null);
                threadLocalSet = cl.getMethod("set",
                    new Class[] { ScriptRuntime.ObjectClass });
                threadLocalCx = cl.newInstance();
            } catch (Exception ex) { }
        }
    }

    // We'd like to use "Thread.getContextClassLoader", but
    // that's only available on Java2.
    private static Method method_getContextClassLoader;

    static {
        // Don't use "Thread.class": that performs the lookup
        // in the class initializer, which doesn't allow us to
        // catch possible security exceptions.
        Class threadClass = Kit.classOrNull("java.lang.Thread");
        if (threadClass != null) {
            try {
                method_getContextClassLoader =
                    threadClass.getDeclaredMethod("getContextClassLoader",
                                                   new Class[0]);
            } catch (Exception ex) { }
        }
    }

    private static String implementationVersion;

    private ContextFactory factory;
    private boolean sealed;
    private Object sealKey;

    Scriptable topCallScope;
    NativeCall currentActivationCall;
    XMLLib cachedXMLLib;

    // for Objects, Arrays to tag themselves as being printed out,
    // so they don't print themselves out recursively.
    // Use ObjToIntMap instead of java.util.HashSet for JDK 1.1 compatibility
    ObjToIntMap iterating;

    Object interpreterSecurityDomain;

    int version;

    private SecurityController securityController;
    private ClassShutter classShutter;
    private ErrorReporter errorReporter;
    private RegExpProxy regExpProxy;
    private Locale locale;
    private boolean generatingDebug;
    private boolean generatingDebugChanged;
    private boolean generatingSource=true;
    boolean compileFunctionsWithDynamicScopeFlag;
    boolean useDynamicScope;
    private int optimizationLevel;
    private WrapFactory wrapFactory;
    Debugger debugger;
    private Object debuggerData;
    private int enterCount;
    private Object propertyListeners;
    private Hashtable hashtable;
    private ClassLoader applicationClassLoader;
    private boolean creationEventWasSent;

    /**
     * This is the list of names of objects forcing the creation of
     * function activation records.
     */
    Hashtable activationNames;

    // For the interpreter to indicate line/source for error reports.
    Object interpreterLineCounting;

    // For instruction counting (interpreter only)
    int instructionCount;
    int instructionThreshold;

    // It can be used to return the second index-like result from function
    int scratchIndex;

    // It can be used to return the second uint32 result from function
    long scratchUint32;

    // It can be used to return the second Scriptable result from function
    Scriptable scratchScriptable;

    // It is used to return the target component for Ref instance
    Scriptable scratchRefTarget;
}
