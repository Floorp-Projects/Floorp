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
* This is the JavaScript specific implementation of the thread state
*
* @author  John Bandhauer
* @version 1.0
* @since   1.0
*/
public final class JSThreadState extends ThreadStateBase
{
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
     * get the count of frames
     */
    public native int countStackFrames()
        throws InvalidInfoException;

    /**
     * get the 'top' frame
     */
    public native StackFrameInfo getCurrentFrame()
        throws InvalidInfoException;

    /**
     * <B><font color="red">Not Implemented.</font></B> 
     * Always throws <code>InternalError("unimplemented")</code>
     */
    public Thread getThread()
    {
        throw new InternalError("unimplemented");
    }

    /**
     * <B><font color="red">Not Implemented.</font></B> 
     * Always throws <code>InternalError("unimplemented")</code>
     */
    public void leaveSuspended()
    {
        throw new InternalError("unimplemented");
    }

    /**
     * <B><font color="red">Not Implemented.</font></B> 
     * Always throws <code>InternalError("unimplemented")</code>
     */
    protected void resume0()
    {
        throw new InternalError("unimplemented");
    }

    protected int nativeThreadState;  /* used internally */
}
