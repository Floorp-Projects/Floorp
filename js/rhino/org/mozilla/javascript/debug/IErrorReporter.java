/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
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
