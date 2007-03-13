/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s):  Ed Burns <edburns@acm.org>
 *                  Ashutosh Kulkarni <ashuk@eng.sun.com>
 *      Jason Mawdsley <jason@macadamian.com>
 *      Louis-Philippe Gagnon <louisphilippe@macadamian.com>
 */

package org.mozilla.webclient.impl.wrapper_native;

import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.CountDownLatch;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;
import org.mozilla.util.ReturnRunnable;
import org.mozilla.util.RunnableRunner;

import org.mozilla.webclient.impl.WrapperFactory;
import org.w3c.dom.DOMException;

/**
 * <p>This is a singleton class.  All native events pass thru this class
 * by virtue of the {@link #pushRunnable} or {@link pushBlockingreturnRunnable}
 * methods.</p>
 */

public class NativeEventThread extends Thread implements RunnableRunner {

    //
    // Class variables
    //
    
    public static final String LOG = "org.mozilla.webclient.impl.wrapper_native.NativeEventThread";

    public static final Logger LOGGER = Log.getLogger(LOG);

    static NativeEventThread instance = null;
    
    //
    // Attribute ivars
    //

    //
    // Relationship ivars
    //
    
    private WrapperFactory wrapperFactory;
    private int nativeWrapperFactory;
    
    private Queue<ReturnRunnableCountDownLatch> blockingRunnables;
    private Queue<Runnable> runnables;
    
    
    //
    // Attribute ivars
    //
    
    //
    // Constructors
    //
    
    public NativeEventThread(String threadName, 
			     WrapperFactory yourFactory) {
	super(threadName);
	// Don't do this check for subclasses
	if (this.getClass() == NativeEventThread.class) {
	    instance = this;
	}
	ParameterCheck.nonNull(yourFactory);
	
	wrapperFactory = yourFactory;
	blockingRunnables = new ConcurrentLinkedQueue<ReturnRunnableCountDownLatch>();
	runnables = new ConcurrentLinkedQueue<Runnable>();
    }
    
    /**
     *
     * This is a very delicate method, and possibly subject to race
     * condition problems.  To combat this, our first step is to set our
     * wrapperFactory to null, within a synchronized block which
     * synchronizes on the same object used in the run() method's event
     * loop.  By setting the wrapperFactory ivar to null, we cause
     * the run method to return.
     */
    
    public void delete() {
	synchronized(this) {
	    // this has to be inside the synchronized block!
	    wrapperFactory = null;
	    try {
                if (LOGGER.isLoggable(Level.FINEST)) {
                    LOGGER.finest("About to wait during delete()");
                }
		wait();
                if (LOGGER.isLoggable(Level.FINEST)) {
                    LOGGER.finest("Returned from wait during delete()");
                }
	    }
	    catch (Exception e) {
                if (LOGGER.isLoggable(Level.SEVERE)) {
                    LOGGER.log(Level.SEVERE, 
                            "Interrupted while waiting for NativeEventThread " +
                            "to notifyAll() after destruction of initContext",e);
                }
	    }
	}
	wrapperFactory = null;
    }

    //
    // Methods from Thread
    //
    
/**

 * This method is the heart of webclient.  It is called indirectly from
 * {@link WrapperFactoryImpl#initialize}.  It calls nativeStartup, which
 * does the per-window initialization, including creating the native
 * event queue which corresponds to this instance, then enters into an
 * infinite loop where processes native events, then checks to see if
 * there are any listeners to add, and adds them if necessary.

 * @see nativeProcessEvents

 */

public void run()
{
    nativeWrapperFactory = wrapperFactory.loadNativeLibrariesIfNecessary();
    
    // our owner must have put an event in the queue 
    Assert.assert_it(!runnables.isEmpty());
    ((Runnable)runnables.poll()).run();
    synchronized (wrapperFactory) {
	try {
	    wrapperFactory.notifyAll();
	}
	catch (Exception e) {
            if (LOGGER.isLoggable(Level.SEVERE)) {
                LOGGER.log(Level.SEVERE, 
                        "Exception trying to send notifyAll() to WrapperFactoryImpl on startup", e);
            }
	}
    }

    //
    // Execute the event-loop.
    // 
    
    while (doEventLoopOnce()) {
    }
}

public void runUntilEventOfType(Class returnRunnableClass) {
    ReturnRunnable result = null;
    while (doEventLoopOnce(returnRunnableClass)) {
    }
}

/**
 * @return true if the event loop should continue to be executed, false otherwise
 */

    private boolean doEventLoopOnce(Class... returnRunnableClass) {
        Runnable runnable;
        ReturnRunnableCountDownLatch latch;
        ReturnRunnable returnRunnable;
        boolean result = true;
        try {
            Thread.sleep(1);
        }
        catch (Exception e) {
            System.out.println("NativeEventThread.run(): Exception: " + e +
                               " while sleeping: " + e.getMessage());
        }
        runnable = runnables.poll();
        latch = blockingRunnables.poll();
        synchronized (this) {
	    // if we are have been told to delete ourselves
            if (null == this.wrapperFactory) {
                try {
                    notifyAll();
                }
                catch (Exception e) {
                    System.out.println("NativeEventThread.run: Exception: trying to send notifyAll() to this during delete: " + e + " " + e.getMessage());
                }
                return false;
            }
	    
	    if (null!= runnable) {
                if (LOGGER.isLoggable(Level.FINEST)) {
                    LOGGER.finest("About to run " + runnable.toString());
                }
		runnable.run();
                if (LOGGER.isLoggable(Level.FINEST)) {
                    LOGGER.finest("Return from run " + runnable.toString());
                }
	    }
	    if (null != latch && null != latch.runnable) {
                returnRunnable = latch.runnable;
                if (LOGGER.isLoggable(Level.FINEST)) {
                    LOGGER.finest("About to run " + returnRunnable.toString());
                }
                try {
                    returnRunnable.setResult(returnRunnable.run());
                }
                catch (Throwable e) {
                    returnRunnable.setResult(e);
                }
                if (LOGGER.isLoggable(Level.FINEST)) {
                    LOGGER.finest("Return from run " + returnRunnable.toString());
                }
		// notify the pushBlockingReturnRunnable() method.
                if (LOGGER.isLoggable(Level.FINEST)) {
                    LOGGER.finest("About to notify User thread that " +
                            returnRunnable.toString() + " has returned.");
                }
                latch.latch.countDown();
                if (LOGGER.isLoggable(Level.FINEST)) {
                    LOGGER.finest("Successfully notified User thread that " +
                            returnRunnable.toString() + " has returned.");
                }
                if (0 < returnRunnableClass.length &&
                    returnRunnable.getClass() == returnRunnableClass[0]) {
                    result = false;
                }
	    }
	    nativeProcessEvents(nativeWrapperFactory);
        }
        return result;
    }

//
// private methods
//

//
// Package methods
//

    public void pushRunnable(Runnable toInvoke) {
	synchronized (this) {
	    runnables.add(toInvoke);
	}
    }
    
    public Object pushBlockingReturnRunnable(ReturnRunnable toInvoke) throws RuntimeException {
	Object result = null;

	if (Thread.currentThread().getName().equals(instance.getName())){
	    toInvoke.setResult(toInvoke.run());
            result = toInvoke.getResult();
            if (result instanceof RuntimeException) {
                throw ((RuntimeException) result);
            }
	    return result;
	}

        ReturnRunnableCountDownLatch latch = new ReturnRunnableCountDownLatch(toInvoke);
        blockingRunnables.add(latch);
        if (LOGGER.isLoggable(Level.FINEST)) {
            LOGGER.finest("User thread: About to wait for " + 
                    toInvoke.toString() + " to complete.");
        }
        try {
            latch.latch.await();
        } catch (InterruptedException ex) {
            if (LOGGER.isLoggable(Level.SEVERE)) {
                LOGGER.log(Level.SEVERE, "User thread: Interrupted while waiting for " + 
                        latch.runnable.toString() + " to complete.", ex);
            }
        }
        if (LOGGER.isLoggable(Level.FINEST)) {
            LOGGER.finest("User thread: " + toInvoke.toString() + " returned.");
        }
        result = toInvoke.getResult();
        if (result instanceof RuntimeException) {
            throw ((RuntimeException) result);
        }

	return result;
    }
    
    private class ReturnRunnableCountDownLatch {
        ReturnRunnable runnable = null;
        CountDownLatch latch = null;
        ReturnRunnableCountDownLatch(ReturnRunnable runnable) {
            this.runnable = runnable;
            this.latch = new CountDownLatch(1);
        }
    }

//
// Native methods
//


/**

 * Called from java to allow the native code to process any pending
 * events, such as: painting the UI, processing mouse and key events,
 * etc.

 */

public native void nativeProcessEvents(int webShellPtr);

} // end of class NativeEventThread
