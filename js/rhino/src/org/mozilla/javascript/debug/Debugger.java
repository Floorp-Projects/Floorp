/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Norris Boyd
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

// API class

package org.mozilla.javascript.debug;

import org.mozilla.javascript.Context;
import org.mozilla.javascript.Scriptable;

/**
Interface to implement if the application is interested in receiving debug
information.
*/
public interface Debugger {

/**
Called when compilation of a particular function or script into internal
bytecode is done.

@param cx current Context for this thread
@param fnOrScript object describing the function or script
@param source the function or script source
*/
    void handleCompilationDone(Context cx, DebuggableScript fnOrScript,
                               StringBuffer source);

/**
Called when execution entered a particular function or script.

@param cx current Context for this thread
@param scope the scope to execute the function or script relative to.
@param thisObj value of the JavaScript <code>this</code> object
@param args the array of arguments
@param fnOrScript object describing the function or script
@return implementation of DebugFrame which receives debug information during
        the function or script execution or null otherwise
*/
    DebugFrame enterFrame(Context cx, Scriptable scope,
                          Scriptable thisObj, Object[] args,
                          DebuggableScript fnOrScript);
}
