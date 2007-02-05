/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Igor Bukanov, igor@fastmail.fm
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 or later (the "GPL"), in which
 * case the provisions of the GPL are applicable instead of those above. If
 * you wish to allow use of your version of this file only under the terms of
 * the GPL and not to allow others to use your version of this file under the
 * MPL, indicate your decision by deleting the provisions above and replacing
 * them with the notice and other provisions required by the GPL. If you do
 * not delete the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * ***** END LICENSE BLOCK ***** */

// API class

package org.mozilla.javascript;

/**
 * Factory class that Rhino runtime use to create new {@link Context}
 * instances or to notify about Context execution.
 * <p>
 * When Rhino runtime needs to create new {@link Context} instance during
 * execution of {@link Context#enter()} or {@link Context}, it will call
 * {@link #makeContext()} of the current global ContextFactory.
 * See {@link #getGlobal()} and {@link #initGlobal(ContextFactory)}.
 * <p>
 * It is also possible to use explicit ContextFactory instances for Context
 * creation. This is useful to have a set of independent Rhino runtime
 * instances under single JVM. See {@link #call(ContextAction)}.
 * <p>
 * The following example demonstrates Context customization to terminate
 * scripts running more then 10 seconds and to provide better compatibility
 * with JavaScript code using MSIE-specific features.
 * <pre>
 * import org.mozilla.javascript.*;
 *
 * class MyFactory extends ContextFactory
 * {
 *
 *     // Custom {@link Context} to store execution time.
 *     private static class MyContext extends Context
 *     {
 *         long startTime;
 *     }
 *
 *     static {
 *         // Initialize GlobalFactory with custom factory
 *         ContextFactory.initGlobal(new MyFactory());
 *     }
 *
 *     // Override {@link #makeContext()}
 *     protected Context makeContext()
 *     {
 *         MyContext cx = new MyContext();
 *         // Use pure interpreter mode to allow for
 *         // {@link #observeInstructionCount(Context, int)} to work
 *         cx.setOptimizationLevel(-1);
 *         // Make Rhino runtime to call observeInstructionCount
 *         // each 10000 bytecode instructions
 *         cx.setInstructionObserverThreshold(10000);
 *         return cx;
 *     }
 *
 *     // Override {@link #hasFeature(Context, int)}
 *     public boolean hasFeature(Context cx, int featureIndex)
 *     {
 *         // Turn on maximum compatibility with MSIE scripts
 *         switch (featureIndex) {
 *             case {@link Context#FEATURE_NON_ECMA_GET_YEAR}:
 *                 return true;
 *
 *             case {@link Context#FEATURE_MEMBER_EXPR_AS_FUNCTION_NAME}:
 *                 return true;
 *
 *             case {@link Context#FEATURE_RESERVED_KEYWORD_AS_IDENTIFIER}:
 *                 return true;
 *
 *             case {@link Context#FEATURE_PARENT_PROTO_PROPERTIES}:
 *                 return false;
 *         }
 *         return super.hasFeature(cx, featureIndex);
 *     }
 *
 *     // Override {@link #observeInstructionCount(Context, int)}
 *     protected void observeInstructionCount(Context cx, int instructionCount)
 *     {
 *         MyContext mcx = (MyContext)cx;
 *         long currentTime = System.currentTimeMillis();
 *         if (currentTime - mcx.startTime > 10*1000) {
 *             // More then 10 seconds from Context creation time:
 *             // it is time to stop the script.
 *             // Throw Error instance to ensure that script will never
 *             // get control back through catch or finally.
 *             throw new Error();
 *         }
 *     }
 *
 *     // Override {@link #doTopCall(Callable, Context, Scriptable scope, Scriptable thisObj, Object[] args)}
 *     protected Object doTopCall(Callable callable,
 *                                Context cx, Scriptable scope,
 *                                Scriptable thisObj, Object[] args)
 *     {
 *         MyContext mcx = (MyContext)cx;
 *         mcx.startTime = System.currentTimeMillis();
 *
 *         return super.doTopCall(callable, cx, scope, thisObj, args);
 *     }
 *
 * }
 *
 * </pre>
 */

public class ContextFactory
{
    private static volatile boolean hasCustomGlobal;
    private static ContextFactory global = new ContextFactory();

    private volatile boolean sealed;

    private final Object listenersLock = new Object();
    private volatile Object listeners;
    private boolean disabledListening;
    private ClassLoader applicationClassLoader;

    /**
     * Listener of {@link Context} creation and release events.
     */
    public interface Listener
    {
        /**
         * Notify about newly created {@link Context} object.
         */
        public void contextCreated(Context cx);

        /**
         * Notify that the specified {@link Context} instance is no longer
         * associated with the current thread.
         */
        public void contextReleased(Context cx);
    }

    /**
     * Get global ContextFactory.
     *
     * @see #hasExplicitGlobal()
     * @see #initGlobal(ContextFactory)
     */
    public static ContextFactory getGlobal()
    {
        return global;
    }

    /**
     * Check if global factory was set.
     * Return true to indicate that {@link #initGlobal(ContextFactory)} was
     * already called and false to indicate that the global factory was not
     * explicitly set.
     *
     * @see #getGlobal()
     * @see #initGlobal(ContextFactory)
     */
    public static boolean hasExplicitGlobal()
    {
        return hasCustomGlobal;
    }

    /**
     * Set global ContextFactory.
     * The method can only be called once.
     *
     * @see #getGlobal()
     * @see #hasExplicitGlobal()
     */
    public static void initGlobal(ContextFactory factory)
    {
        if (factory == null) {
            throw new IllegalArgumentException();
        }
        if (hasCustomGlobal) {
            throw new IllegalStateException();
        }
        hasCustomGlobal = true;
        global = factory;
    }

    /**
     * Create new {@link Context} instance to be associated with the current
     * thread.
     * This is a callback method used by Rhino to create {@link Context}
     * instance when it is necessary to associate one with the current
     * execution thread. <tt>makeContext()</tt> is allowed to call
     * {@link Context#seal(Object)} on the result to prevent
     * {@link Context} changes by hostile scripts or applets.
     */
    protected Context makeContext()
    {
        return new Context();
    }

    /**
     * Implementation of {@link Context#hasFeature(int featureIndex)}.
     * This can be used to customize {@link Context} without introducing
     * additional subclasses.
     */
    protected boolean hasFeature(Context cx, int featureIndex)
    {
        int version;
        switch (featureIndex) {
          case Context.FEATURE_NON_ECMA_GET_YEAR:
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
            version = cx.getLanguageVersion();
            return (version == Context.VERSION_1_0
                    || version == Context.VERSION_1_1
                    || version == Context.VERSION_1_2);

          case Context.FEATURE_MEMBER_EXPR_AS_FUNCTION_NAME:
            return false;

          case Context.FEATURE_RESERVED_KEYWORD_AS_IDENTIFIER:
            return false;

          case Context.FEATURE_TO_STRING_AS_SOURCE:
            version = cx.getLanguageVersion();
            return version == Context.VERSION_1_2;

          case Context.FEATURE_PARENT_PROTO_PROPERTIES:
            return true;

          case Context.FEATURE_E4X:
            version = cx.getLanguageVersion();
            return (version == Context.VERSION_DEFAULT
                    || version >= Context.VERSION_1_6);

          case Context.FEATURE_DYNAMIC_SCOPE:
            return false;

          case Context.FEATURE_STRICT_VARS:
            return false;

          case Context.FEATURE_STRICT_EVAL:
            return false;
            
          case Context.FEATURE_LOCATION_INFORMATION_IN_ERROR:
            return false;
        }
        // It is a bug to call the method with unknown featureIndex
        throw new IllegalArgumentException(String.valueOf(featureIndex));
    }
	
	private boolean isDom3Present() {
		Class nodeClass = Kit.classOrNull("org.w3c.dom.Node");
		if (nodeClass == null) return false;
		//	Check to see whether DOM3 is present; use a new method defined in DOM3 that is vital to our implementation
		try {
			nodeClass.getMethod("getUserData", new Class[] { String.class });
			return true;
		} catch (NoSuchMethodException e) {
			return false;
		}
	}
	
	/**
		Provides a default {@link org.mozilla.javascript.xml.XMLLib.Factory XMLLib.Factory}
		to be used by the <code>Context</code> instances produced by this factory.
	 
		See {@link Context#getE4xImplementationFactory} for details.
	 */
	protected org.mozilla.javascript.xml.XMLLib.Factory getE4xImplementationFactory() {
		//	Must provide default implementation, rather than abstract method,
		//	so that past implementors of ContextFactory do not fail at runtime
		//	upon invocation of this method.
		
		//	Note that the default implementation "illegally" returns null if we
		//	neither have XMLBeans nor a DOM3 implementation present.
		//
		//	TODO	More thinking about what to do in the failure scenario
		
		//	For now, if XMLBeans is in the classpath, it will be the default.
		if (Kit.classOrNull("org.apache.xmlbeans.XmlCursor") != null) {
			return org.mozilla.javascript.xml.XMLLib.Factory.create(
				"org.mozilla.javascript.xml.impl.xmlbeans.XMLLibImpl"
			);
		} else if (isDom3Present()) {
			return org.mozilla.javascript.xml.XMLLib.Factory.create(
				"org.mozilla.javascript.xmlimpl.XMLLibImpl"
			);
		} else {
			//	Uh-oh -- results if FEATURE_E4X is true are unknown.
			return null;
		}
	}


    /**
     * Create class loader for generated classes.
     * This method creates an instance of the default implementation
     * of {@link GeneratedClassLoader}. Rhino uses this interface to load
     * generated JVM classes when no {@link SecurityController}
     * is installed.
     * Application can override the method to provide custom class loading.
     */
    protected GeneratedClassLoader createClassLoader(ClassLoader parent)
    {
        return new DefiningClassLoader(parent);
    }

    /**
     * Get ClassLoader to use when searching for Java classes.
     * Unless it was explicitly initialized with
     * {@link #initApplicationClassLoader(ClassLoader)} the method returns
     * null to indicate that Thread.getContextClassLoader() should be used.
     */
    public final ClassLoader getApplicationClassLoader()
    {
        return applicationClassLoader;
    }

    /**
     * Set explicit class loader to use when searching for Java classes.
     *
     * @see #getApplicationClassLoader()
     */
    public final void initApplicationClassLoader(ClassLoader loader)
    {
        if (loader == null)
            throw new IllegalArgumentException("loader is null");
        if (!Kit.testIfCanLoadRhinoClasses(loader))
            throw new IllegalArgumentException(
                "Loader can not resolve Rhino classes");

        if (this.applicationClassLoader != null)
            throw new IllegalStateException(
                "applicationClassLoader can only be set once");
        checkNotSealed();

        this.applicationClassLoader = loader;
    }

    /**
     * Execute top call to script or function.
     * When the runtime is about to execute a script or function that will
     * create the first stack frame with scriptable code, it calls this method
     * to perform the real call. In this way execution of any script
     * happens inside this function.
     */
    protected Object doTopCall(Callable callable,
                               Context cx, Scriptable scope,
                               Scriptable thisObj, Object[] args)
    {
        return callable.call(cx, scope, thisObj, args);
    }

    /**
     * Implementation of
     * {@link Context#observeInstructionCount(int instructionCount)}.
     * This can be used to customize {@link Context} without introducing
     * additional subclasses.
     */
    protected void observeInstructionCount(Context cx, int instructionCount)
    {
    }

    protected void onContextCreated(Context cx)
    {
        Object listeners = this.listeners;
        for (int i = 0; ; ++i) {
            Listener l = (Listener)Kit.getListener(listeners, i);
            if (l == null)
                break;
            l.contextCreated(cx);
        }
    }

    protected void onContextReleased(Context cx)
    {
        Object listeners = this.listeners;
        for (int i = 0; ; ++i) {
            Listener l = (Listener)Kit.getListener(listeners, i);
            if (l == null)
                break;
            l.contextReleased(cx);
        }
    }

    public final void addListener(Listener listener)
    {
        checkNotSealed();
        synchronized (listenersLock) {
            if (disabledListening) {
                throw new IllegalStateException();
            }
            listeners = Kit.addListener(listeners, listener);
        }
    }

    public final void removeListener(Listener listener)
    {
        checkNotSealed();
        synchronized (listenersLock) {
            if (disabledListening) {
                throw new IllegalStateException();
            }
            listeners = Kit.removeListener(listeners, listener);
        }
    }

    /**
     * The method is used only to imlement
     * Context.disableStaticContextListening()
     */
    final void disableContextListening()
    {
        checkNotSealed();
        synchronized (listenersLock) {
            disabledListening = true;
            listeners = null;
        }
    }

    /**
     * Checks if this is a sealed ContextFactory.
     * @see #seal()
     */
    public final boolean isSealed()
    {
        return sealed;
    }

    /**
     * Seal this ContextFactory so any attempt to modify it like to add or
     * remove its listeners will throw an exception.
     * @see #isSealed()
     */
    public final void seal()
    {
        checkNotSealed();
        sealed = true;
    }

    protected final void checkNotSealed()
    {
        if (sealed) throw new IllegalStateException();
    }

    /**
     * Call {@link ContextAction#run(Context cx)}
     * using the {@link Context} instance associated with the current thread.
     * If no Context is associated with the thread, then
     * {@link #makeContext()} will be called to construct
     * new Context instance. The instance will be temporary associated
     * with the thread during call to {@link ContextAction#run(Context)}.
     *
     * @see ContextFactory#call(ContextAction)
     * @see Context#call(ContextFactory factory, Callable callable,
     *                   Scriptable scope, Scriptable thisObj,
     *                   Object[] args)
     */
    public final Object call(ContextAction action)
    {
        return Context.call(this, action);
    }
    
    /**
     * Same as {@link Context#enter()} with the difference that if a new context
     * needs to be created, then this context factory is used to create it 
     * instead of the global context factory.
     * @return a Context associated with the current thread
     */
    public final Context enter()
    {
        return enter(null);
    }
    
    /**
     * Same as {@link Context#enter(Context)} with the difference that if a new 
     * context needs to be created, then this context factory is used to create 
     * it instead of the global context factory.
     * @return a Context associated with the current thread
     */
    public final Context enter(Context cx)
    {
        return Context.enter(cx, this);
    }

    /**
     * Same as {@link Context#exit()}, although if you used {@link #enter()} or
     * {@link #enter(Context)} methods on this object, you should use this exit
     * method instead of the static one in {@link Context}. 
     */
    public final void exit()
    {
        Context.exit(this);
    }
}

