
package com.netscape.jsdebugging.remote.corba;
 
/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

public final class IJSThreadState
{
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


    public IJSStackFrameInfo[]  stack;
    public int                  continueState;
    public String               returnValue;
    public int                  status;
    public int                  jsdthreadstate;
    public int                  id;  // used for referencing this object
}    

