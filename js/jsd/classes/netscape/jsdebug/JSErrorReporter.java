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
* This is a special kind of hook to respond to JavaScript errors
*
* @author  John Bandhauer
* @version 1.0
* @since   1.0
*/
public interface JSErrorReporter
{
    /* keep these in sync with the numbers in jsdebug.h */

    /**
    * returned by <code>reportError()</code> to indicate that the error
    * should be passed along to the error reporter that would have been 
    * called had the debugger not been running
    */
    public static final int PASS_ALONG = 0;
    /**
    * returned by <code>reportError()</code> to indicate that the 
    * normal error reporter should not be called and that the JavaScript
    * engine should do whatever it would normally do after calling the
    * error reporter.
    */
    public static final int RETURN     = 1;
    /**
    * returned by <code>reportError()</code> to indicate that the 
    * 'debug break' hook should be called to allow the debugger to 
    * investigate the state of the process when the error occured
    */
    public static final int DEBUG      = 2;

    /**
    * This hook is called when a JavaScript error (compile or runtime) occurs
    * <p> 
    * One of the codes above should be returned to tell the engine how to 
    * proceed.
    * @param msg error message passed through from the JavaScript engine
    * @param filename filename (or url) of the code with the error
    * @param lineno line number where error was detected
    * @param linebuf a copy of the line where the error was detected
    * @param tokenOffset the offset into <i>linebuf</i> where the error 
    * was detected
    * @returns one of the codes above
    */
    public int reportError( String msg,
                            String filename,
                            int    lineno,
                            String linebuf,
                            int    tokenOffset );
}    
