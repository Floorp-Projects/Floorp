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
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
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

package org.mozilla.javascript;

/**
 * All compiled scripts implement this interface.
 * <p>
 * This class encapsulates script execution relative to an
 * object scope.
 * @since 1.3
 * @author Norris Boyd
 */

public interface Script {

    /**
     * Execute the script.
     * <p>
     * The script is executed in a particular runtime Context, which
     * must be associated with the current thread.
     * The script is executed relative to a scope--definitions and
     * uses of global top-level variables and functions will access
     * properties of the scope object. For compliant ECMA
     * programs, the scope must be an object that has been initialized
     * as a global object using <code>Context.initStandardObjects</code>.
     * <p>
     *
     * @param cx the Context associated with the current thread
     * @param scope the scope to execute relative to
     * @return the result of executing the script
     * @see org.mozilla.javascript.Context#initStandardObjects
     */
    public Object exec(Context cx, Scriptable scope);

}
