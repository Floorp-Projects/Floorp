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

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

import java.util.Vector;
import java.util.Enumeration;

import java.util.Stack;

import org.mozilla.webclient.UnimplementedException;

import org.mozilla.webclient.impl.WrapperFactory;

/**
 * <p>This is a singleton class.  All native events pass thru this class
 * by virtue of the {@link #pushRunnable} or {@link pushBlockingWCRunnable}
 * methods.</p>
 */

public class NativeEventThread extends Thread {

    //
    // Class variables
    //

    static NativeEventThread instance = null;
    
    //
    // Attribute ivars
    //

    private Object blockingResult;

    private Exception blockingException;

    //
    // Relationship ivars
    //
    
    private WrapperFactory wrapperFactory;
    private int nativeWrapperFactory;
    
    private Stack blockingRunnables;
    private Stack runnables;
    
    
    //
    // Attribute ivars
    //
    
    //
    // Constructors
    //
    
    public NativeEventThread(String threadName, 
			     WrapperFactory yourFactory,
			     int yourNativeWrapperFactory) {
	super(threadName);
	Assert.assert_it(null == instance);
	instance = this;
	ParameterCheck.nonNull(yourFactory);
	
	wrapperFactory = yourFactory;
	nativeWrapperFactory = yourNativeWrapperFactory;
	
	blockingRunnables = new Stack();
	runnables = new Stack();
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
		wait();
	    }
	    catch (Exception e) {
		System.out.println("NativeEventThread.delete: interrupted while waiting\n\t for NativeEventThread to notify() after destruction of initContext: " + e +
				   " " + e.getMessage());
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
    // our owner must have put an event in the queue 
    Assert.assert_it(!runnables.empty());
    ((Runnable)runnables.pop()).run();
    synchronized (wrapperFactory) {
	try {
	    wrapperFactory.notify();
	}
	catch (Exception e) {
	    System.out.println("NativeEventThread.run: exception trying to send notify() to WrapperFactoryImpl on startup:" + e + " " + e.getMessage());
	}
    }

    //
    // Execute the event-loop.
    // 
    
    while (true) {
        try {
            Thread.sleep(1);
        }
        catch (Exception e) {
            System.out.println("NativeEventThread.run(): Exception: " + e +
                               " while sleeping: " + e.getMessage());
        }
        synchronized (this) {
	    // if we are have been told to delete ourselves
            if (null == this.wrapperFactory) {
                try {
                    notify();
                }
                catch (Exception e) {
                    System.out.println("NativeEventThread.run: Exception: trying to send notify() to this during delete: " + e + " " + e.getMessage());
                }
                return;
            }
	    
	    if (!runnables.empty()) {
		((Runnable)runnables.pop()).run();
	    }
	    if (!blockingRunnables.empty()) {
		try {
		    blockingException = null;
		    blockingResult = 
			((WCRunnable)blockingRunnables.pop()).run();
		}
		catch (RuntimeException e) {
		    blockingException = e;
		}
		// notify the pushBlockingWCRunnable() method.
		try {
		    notify();
		}
		catch (Exception e) {
		    System.out.println("NativeEventThread.run: Exception: trying to notify for blocking result:" + e + " " + e.getMessage());
		}
		// wait for the result to be grabbed.  This prevents the
		// results from getting mixed up.
		try {
		    wait();
		}
		catch (Exception e) {
		    System.out.println("NativeEventThread.run: Exception: trying to waiting for pushBlockingWCRunnable:" + e + " " + e.getMessage());
		}

		
	    }
	    nativeProcessEvents(nativeWrapperFactory);
        }
    }
}

//
// private methods
//

//
// Package methods
//

    void pushRunnable(Runnable toInvoke) {
	synchronized (this) {
	    runnables.push(toInvoke);
	}
    }
    
    Object pushBlockingWCRunnable(WCRunnable toInvoke) {
	Object result = null;
	RuntimeException e = null;
	synchronized (this) {
	    blockingRunnables.push(toInvoke);
	    try {
		wait();
	    }
	    catch (Exception se) {
		System.out.println("NativeEventThread.pushBlockingWCRunnable: Exception: while waiting for blocking result: " + se + "  " + se.getMessage());
	    }
	    result = blockingResult;
	    if (null != blockingException) {
		e = new RuntimeException(blockingException);
	    }
	    try {
		notify();
	    }
	    catch (Exception se) {
		System.out.println("NativeEventThread.pushBlockingWCRunnable: Exception: trying to send notify() to NativeEventThread: " + se + "  " + se.getMessage());
	    }
	}
	if (null != e) {
	    throw e;
	}
	return result;
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
