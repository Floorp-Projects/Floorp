/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0
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
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Norris Boyd
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 or later (the "GPL"), in which
 * case the provisions of the GPL are applicable instead of those above. If
 * you wish to allow use of your version of this file only under the terms of
 * the GPL and not to allow others to use your version of this file under the
 * MPL, indicate your decision by deleting the provisions above and replacing
 * them with the notice and other provisions required by the GPL. If you do
 * not delete the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * ***** END LICENSE BLOCK ***** */

// API class

package org.mozilla.javascript.debug;

import org.mozilla.javascript.Context;
import org.mozilla.javascript.Scriptable;

/**
Interface to implement if the application is interested in receiving debug
information during execution of a particular script or function.
*/
public interface DebugFrame {

/**
Called when execution is ready to start bytecode interpretation for entered a particular function or script.

@param cx current Context for this thread
@param activation the activation scope for the function or script.
@param thisObj value of the JavaScript <code>this</code> object
@param args the array of arguments
*/
    public void onEnter(Context cx, Scriptable activation,
                        Scriptable thisObj, Object[] args);
/**
Called when executed code reaches new line in the source.
@param cx current Context for this thread
@param lineNumber current line number in the script source
*/
    public void onLineChange(Context cx, int lineNumber);

/**
Called when thrown exception is handled by the function or script.
@param cx current Context for this thread
@param ex exception object
*/
    public void onExceptionThrown(Context cx, Throwable ex);

/**
Called when the function or script for this frame is about to return.
@param cx current Context for this thread
@param byThrow if true function will leave by throwing exception, otherwise it
       will execute normal return
@param resultOrException function result in case of normal return or
       exception object if about to throw exception
*/
    public void onExit(Context cx, boolean byThrow, Object resultOrException);

}
