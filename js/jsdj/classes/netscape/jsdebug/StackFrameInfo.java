/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package netscape.jsdebug;

/**
* This interface provides access to the execution stack of a thread.
* It has several subclasses to distinguish between different kinds of
* stack frames: these currently include activations of Java methods
* or JavaScript functions.
* It is possible that synchronize blocks and try blocks deserve their own
* stack frames - to allow for later extensions a debugger should skip over
* stack frames it doesn't understand.
* Note that this appears very Java-specific.  However, multiple threads and
* exceptions are relevant to JavaScript as well because of its
* interoperation with Java.
*
* @author  John Bandhauer
* @author  Nick Thompson
* @version 1.0
* @since   1.0
*/
public abstract class StackFrameInfo {
    ThreadStateBase threadState;
    StackFrameInfo caller;

    protected StackFrameInfo(ThreadStateBase threadState) {
        this.threadState = threadState;
    }

    /**
     * Return true if this stack frame is still valid.  Stack frames
     * may become invalid when the thread is resumed (this is more
     * conservative than invalidating the frame only when it actually
     * returns).
     */
    public boolean isValid() {
        return threadState.isValid();
    }

    /**
     * Get the stack frame where this one was built, or null for the
     * start of the stack.
     */
    public synchronized StackFrameInfo getCaller()
        throws InvalidInfoException {
        if (caller == null)
            caller = getCaller0();
        if (!isValid())
            throw new InvalidInfoException("invalid StackFrameInfo");
        return caller;
    }

    protected abstract StackFrameInfo getCaller0()
        throws InvalidInfoException;

    /**
     * Get the thread that this stack frame is a part of.
     */
    public Thread getThread() {
        return threadState.getThread();
    }

    /**
     * Get the 'enclosing' thread state
     */
    public ThreadStateBase getThreadState() {
        return threadState;
    }


    /**
     * Get the current program counter value.  Note that the
     * class JavaPC supports the getMethod() operation.
     */
    public abstract PC getPC()
        throws InvalidInfoException;
}

