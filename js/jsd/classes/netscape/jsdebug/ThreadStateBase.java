/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package netscape.jsdebug;

/*  
*  jband - 03/19/97
*
*  This is an 'abstracted version of netscape.debug.ThreadState 
*
*  The methods that were 'native' there are 'abstract' here.
*  Changed 'private' data to 'protected' (though native access is immune)
*  Changed 'private' resume0() to 'protected'
*  Removed ThreadHook referneces
*
*/

/**
* When a hook is hit, the debugger records the state of the
* thread before the hook in a ThreadState object.  This object
* is then passed to any hook methods that are called, and can
* be used to change the state of the thread when it resumes from the
* hook.
*
* @author  John Bandhauer
* @author  Nick Thompson
* @version 1.0
* @since   1.0
*/
public abstract class ThreadStateBase {
    protected Thread thread;
    protected boolean valid;
    protected boolean runningHook;
    protected boolean resumeWhenDone;
    protected int status;
    protected int continueState;
    protected StackFrameInfo[] stack; /* jband - 03/19/97 - had no access modifier */
    protected Object returnValue;
    protected Throwable currentException;
    protected int currentFramePtr;  /* used internally */
    protected ThreadStateBase previous;

    /**
     * <B><font color="red">Not Implemented.</font></B> 
     * Always throws <code>InternalError("unimplemented")</code>
     */
    public static ThreadStateBase getThreadState(Thread t)
        throws InvalidInfoException
    {
        throw new InternalError("unimplemented");
    }

    /**
     * Get the Thread that this ThreadState came from.
     */
    public Thread getThread() {
        return thread;
    }

    /**
     * Return true if the Thread hasn't been resumed since this ThreadState
     * was made.
     */
    public boolean isValid() {
        return valid;
    }

    /**
     * Return true if the thread is currently running a hook
     * for this ThreadState
     */
    public boolean isRunningHook() {
        return runningHook;
    }

    /**
     * partial list of thread states from sun.debug.ThreadInfo.
     * XXX some of these don't apply.
     */
    public final static int THR_STATUS_UNKNOWN		= 0x01;
    public final static int THR_STATUS_ZOMBIE		= 0x02;
    public final static int THR_STATUS_RUNNING		= 0x03;
    public final static int THR_STATUS_SLEEPING		= 0x04;
    public final static int THR_STATUS_MONWAIT		= 0x05;
    public final static int THR_STATUS_CONDWAIT		= 0x06;
    public final static int THR_STATUS_SUSPENDED	= 0x07;
    public final static int THR_STATUS_BREAK		= 0x08;

    /** 
     * Get the state of the thread at the time it entered debug mode.
     * This can't be modified directly.
     */
    public int getStatus() {
        return status;
    }

    /** 
     * Get the count of the stackframes
     */
    public abstract int countStackFrames()
        throws InvalidInfoException;
    /** 
     * Get the 'top' stackframe; i.e. the one with the current instruction
     */
    public abstract StackFrameInfo getCurrentFrame()
        throws InvalidInfoException;

    /**
     * Get the thread's stack as an array.  stack[stack.length-1] is the
     * current frame, and stack[0] is the beginning of the stack.
     */
    public synchronized StackFrameInfo[] getStack()
        throws InvalidInfoException {
        // XXX check valid?
        if (stack == null) {
            stack = new StackFrameInfo[countStackFrames()];
        }

        if (stack.length == 0)
            return stack;

        StackFrameInfo frame = getCurrentFrame();
        stack[stack.length - 1] = frame;
        for (int i = stack.length - 2; i >= 0; i--) {
            frame = frame.getCaller();
            stack[i] = frame;
        }

        // should really be read-only for safety
        return stack;
    }

    /**
     * Leave the thread in a suspended state when the hook method(s)
     * finish.  This can be undone by calling resume().  This is the
     * default only if the break was the result of
     * DebugController.sendInterrupt().
     */
    public void leaveSuspended() {
        resumeWhenDone = false;
    }

    /**
     * Resume the thread.  This is the default unless the break was the result
     * of DebugController.sendInterrupt(). This method may be called from a
     * hook method, in which case the thread will resume when the
     * method returns.
     * Alternatively, if there is no active hook method and
     * the thread is suspended around in the DebugFrame, this resumes it
     * immediately.
     */
    public synchronized void resume() {
        if (runningHook)
            resumeWhenDone = true;
        else
            resume0();
    }

    protected abstract void resume0();

    /**
     * if the continueState is DEAD, the thread cannot
     * be restarted.
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
     */
    public int getContinueState() {
        return continueState;
    }

    public int setContinueState(int state) {
        int old = continueState;
        continueState = state;
        return old;
    }

    /**
     * If the thread was stopped while in the process of returning
     * a value, this call returns an object representing that value.
     * non-Object values are wrapped as in the java.lang.reflect api.
     * This is only relevant if the continue state is RETURN, and the
     * method throws an IllegalStateException otherwise.
     */
    public Object getReturnValue() throws IllegalStateException {
        if (continueState != DEBUG_STATE_RETURN)
            throw new IllegalStateException("no value being returned");
        return returnValue;
    }

    /** 
     * If the thread was stopped while in the process of throwing an
     * exception, this call returns that exception.  
     * This is only relevant if the continue state is THROW, and it
     * throws an IllegalStateException otherwise.
     */
    public Throwable getException() throws IllegalStateException {
        if (continueState != DEBUG_STATE_THROW)
            throw new IllegalStateException("no exception throw in progress");
        return currentException;
    }
}

