/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
