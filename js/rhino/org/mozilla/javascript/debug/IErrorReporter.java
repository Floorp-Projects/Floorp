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
 * This interface supports hooking the error reporter system.
 * <p>
 * This interface can be implemented and used to hook calls to
 * the error reporter. This hook can decide whether to pass
 * the error along to the normal error reporter, eat the error,
 * or force a debug stop.
 *
 * @see org.mozilla.javascript.ErrorReporter
 * @see org.mozilla.javascript.debug.IDebugManager#setErrorReporter
 * @see org.mozilla.javascript.debug.IDebugBreakHook
 * @author John Bandhauer
 */

public interface IErrorReporter
{
    /* keep these in sync with the numbers in jsdebug.h and jsdebugger.api */

    /**
    * Possible return values.
    */

    /**
    * Pass the error along to the normal error reporter.
    */
    public int PASS_ALONG = 0;

    /**
    * Eat the error.
    */
    public int RETURN     = 1;

    /**
    * Eat the error and force a debug break
    */
    public int DEBUG      = 2;

    /**
    * Method called when error occurs.
    *
    * @param msg the message that would be passed to the error reporter
    * @param filename filename of code with error
    * @param lineno line number where error happened
    * @param linebuf copy of the line of code with error (may be nul)
    * @param tokenOffset offset into linebuf where error detected
    * @return one of {PASS_ALONG | RETURN | DEBUG}
    */
    public int reportError(String msg,
                           String filename,
                           int    lineno,
                           String linebuf,
                           int    tokenOffset);
}
