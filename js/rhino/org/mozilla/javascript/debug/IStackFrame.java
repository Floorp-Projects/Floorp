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
 * This interface represents a stack frame in a stopped thread.
 * <p>
 * This interface is implemented by the debug system. Consumers of the debug
 * system should never need to create their own IStackFrame objects. The
 * debug system would certainly not recognize such objects.
 *
 * @author John Bandhauer
 * @see org.mozilla.javascript.debug.IThreadState
 * @see org.mozilla.javascript.debug.IPC
 */

public interface IStackFrame
{
    /**
    * Determine if the object is valid.
    * <p>
    * This IStackFrame will be set invalid when the underlying IThreadState
    * is continued.
    *
    * @return is the Object valid
    * @see org.mozilla.javascript.debug.IThreadState#isValid
    */
    public boolean isValid();

    /**
    * Get the calling stack frame.
    *
    * @return caller (null if this is the bottom of the stack)
    */
    public IStackFrame getCaller();

    /**
    * Get the program counter for this frame.
    *
    * @return the program counter.
    */
    public IPC getPC();
}
