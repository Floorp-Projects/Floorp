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
 * Igor Bukanov
 * David C. Navas
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

package org.mozilla.javascript;

import java.lang.reflect.Method;

/**
 * Avoid cost of java.lang.reflect.Method.invoke() by compiling a class to
 * perform the method call directly.
 */
public abstract class Invoker {

    public abstract Object invoke(Object that, Object [] args);

    /** Factory method to get invoker for given method */
    public Invoker createInvoker(ClassCache cache,
                                 Method method, Class[] types)
    {
        // should not be called unless master
        throw new IllegalStateException();
    }

    /** Factory method to clear internal cache if any */
    public void clearMasterCaches()
    {
        // should not be called unless master
        throw new IllegalStateException();
    }

    public static Invoker makeMaster()
    {
        if (implClass == null)
            return null;

        Invoker master = (Invoker)Kit.newInstanceOrNull(implClass);
        if (master == null)
            implClass = null;

        return master;
    }

    private static Class implClass = Kit.classOrNull(
        "org.mozilla.javascript.optimizer.InvokerImpl");

}
