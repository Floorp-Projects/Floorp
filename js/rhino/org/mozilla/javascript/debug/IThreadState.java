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

// DEBUG API class

package org.mozilla.javascript.debug;

/**
 * This interface represents the state of a stopped thread.
 * <p>
 * This interface is implemented by the debug system. Consumers of the debug
 * system should never need to create their own IThreadState objects. The 
 * debug system would certainly not recognize such objects.
 *
 * @author John Bandhauer
 * @see org.mozilla.javascript.debug.IDebugManager
 * @see org.mozilla.javascript.debug.IStackFrame
 * @see java.lang.IllegalStateException
 */

public interface IThreadState
{
    
    /**
    * Get the count of stack frames on this thread.
    *
    * @return count of frames
    * @see org.mozilla.javascript.debug.IStackFrame
    * @throws IllegalStateException if not valid
    */
    public int countStackFrames() throws IllegalStateException;

    /**
    * Get the 'top' frame; that is, the frame that was last executing code.
    *
    * @return current frame
    * @throws IllegalStateException if not valid
    * @see org.mozilla.javascript.debug.IStackFrame
    */
    public IStackFrame getCurrentFrame() throws IllegalStateException;

    /**
    * Get an array of the stack frames.
    * <p>
    * The current frame is at offset 'count-1'. The 'first' frame in 
    * the stack is at offset 0;
    *
    * @return stack frame array
    * @throws IllegalStateException if not valid
    * @see org.mozilla.javascript.debug.IStackFrame
    */
    public IStackFrame[] getStack() throws IllegalStateException;

    /**
    * Is the thread state valid?
    *
    * @return true if stopped, false if thread has been resumed
    * @see org.mozilla.javascript.debug.IStackFrame
    */
    public boolean isValid();


// XXX these are hold overs from the old netscape.debug.ThreadState
//    public Thread getThread();
//    public boolean isRunningHook();
//
//    /**
//     * partial list of thread states from sun.debug.ThreadInfo.
//     * XXX some of these don't apply.
//     */
//    public final static int THR_STATUS_UNKNOWN		= 0x01;
//    public final static int THR_STATUS_ZOMBIE		= 0x02;
//    public final static int THR_STATUS_RUNNING		= 0x03;
//    public final static int THR_STATUS_SLEEPING		= 0x04;
//    public final static int THR_STATUS_MONWAIT		= 0x05;
//    public final static int THR_STATUS_CONDWAIT		= 0x06;
//    public final static int THR_STATUS_SUSPENDED	= 0x07;
//    public final static int THR_STATUS_BREAK		= 0x08;
//
//    public int getStatus();
//    public void leaveSuspended();
//    public void resume();

    /**
     * if the continueState is DEAD, the thread cannot
     * be restarted. XXX not supported
     */
    public final static int DEBUG_STATE_DEAD		= 0x01;

    /**
     * if the continueState is RUN, the thread will
     * proceed to the next program counter value when it resumes.
     */
    public final static int DEBUG_STATE_RUN		= 0x02;

    /**
     * if the continueState is RETURN, the thread will
     * return from the current method with the value in getReturnValue()
     * when it resumes.
     */
    public final static int DEBUG_STATE_RETURN		= 0x03;

    /**
     * if the continueState is THROW, the thread will
     * throw an exception (accessible with getException()) when it
     * resumes.
     */
    public final static int DEBUG_STATE_THROW		= 0x04;

    /**
     * This gets the current continue state of the debug frame, which
     * will be one of the DEBUG_STATE_* values above.
     * @return current state
     * @throws IllegalStateException if not valid
     */
    public int getContinueState()  throws IllegalStateException;

    /**
     * This sets the current continue state of the debug frame, which
     * will be one of the DEBUG_STATE_* values above.
     * @return previous value
     * @throws IllegalStateException if not valid
     */
    public int setContinueState(int state)  throws IllegalStateException;

    /**
     * If the thread was stopped while in the process of returning
     * a value, this call returns an object representing that value.
     * non-Object values are wrapped as in the java.lang.reflect api.
     * This is only relevant if the continue state is RETURN, and the
     * method throws an IllegalStateException otherwise.
     * @return current return value
     * @throws IllegalStateException if not valid
     */
    public Object getReturnValue() throws IllegalStateException;

    /**
     * If the thread's continueState is set to (or already was) 
     * DEBUG_STATE_RETURN, then this sets the value that will be returned
     * when the thread is continued. Returns the old value.
     * @return previous value
     * @throws IllegalStateException if not valid
     */
    public Object setReturnValue(Object o) throws IllegalStateException;

    /** 
     * If the thread was stopped while in the process of throwing an
     * exception, this call returns that exception.  
     * This is only relevant if the continue state is THROW, and it
     * throws an IllegalStateException otherwise.
     * @return current exception
     * @throws IllegalStateException if not valid
     */
    public Throwable getException() throws IllegalStateException;

    /**
     * If the thread's continueState is set to (or already was) 
     * DEBUG_STATE_THROW, then this sets the value that will be throws
     * when the thread is continued. Returns the old value.
     * @return previous value
     * @throws IllegalStateException if not valid
     */
    public Throwable setException(Throwable t) throws IllegalStateException;
}    
