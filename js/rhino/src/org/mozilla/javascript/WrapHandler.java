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
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Marshall Cline
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
 * Embeddings that wish to provide their own custom wrappings for Java
 * objects may implement this interface and call Context.setWrapHandler.
 * @see org.mozilla.javascript.Context#setWrapHandler
 */
public interface WrapHandler {

    /**
     * Wrap the object.
     * <p>
     * The value returned must be one of
     * <UL>
     * <LI>java.lang.Boolean</LI>
     * <LI>java.lang.String</LI>
     * <LI>java.lang.Number</LI>
     * <LI>org.mozilla.javascript.Scriptable objects</LI>
     * <LI>The value returned by Context.getUndefinedValue()</LI>
     * <LI>null</LI>
     * <p>
     * If null is returned, the value obj will be wrapped as if
     * no WrapHandler had been called.
     * </UL>
     * @param scope the scope of the executing script
     * @param obj the object to be wrapped
     * @staticType the static type of the object to be wrapped
     * @return the wrapped value.
     */
    public Object wrap(Scriptable scope, Object obj, Class staticType);
}
