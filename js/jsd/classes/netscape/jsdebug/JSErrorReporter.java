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
