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
 * Igor Bukanov
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

import java.io.*;
import java.util.Hashtable;

/**
 * Object to use as a top-level scope for the standard library objects.
 * @since 1.5 Release 5
 */
public class GlobalScope extends ScriptableObject
{
    public String getClassName()
    {
        return "global";
    }

    public static GlobalScope get(Scriptable scope)
    {
        if (scope instanceof GlobalScope) {
            return (GlobalScope)scope;
        }
        scope = ScriptableObject.getTopLevelScope(scope);
        Scriptable obj = scope;
        do {
            if (obj instanceof GlobalScope) {
                return (GlobalScope)obj;
            }
            obj = obj.getPrototype();
        } while (obj != null);
        Object x = ScriptableObject.getProperty(scope, "__globalScope");
        if (x instanceof GlobalScope) {
            return (GlobalScope)x;
        }
        return new GlobalScope();
    }

     /**
     * Set whether to cache some values.
     * <p>
     * By default, the engine will cache some values
     * (reflected Java classes, for instance). This can speed
     * execution dramatically, but increases the memory footprint.
     * Also, with caching enabled, references may be held to
     * objects past the lifetime of any real usage.
     * <p>
     * If caching is enabled and this method is called with a
     * <code>false</code> argument, the caches will be emptied.
     * So one strategy could be to clear the caches at times
     * appropriate to the application.
     * <p>
     * Caching is enabled by default.
     *
     * @param cachingEnabled if true, caching is enabled
     */
    public void setCachingEnabled(boolean cachingEnabled)
    {
        if (isCachingEnabled && !cachingEnabled) {
            // Caching is being turned off. Empty caches.
            classTable.clear();
            // For compatibility disable invoker optimization as well
            setInvokerOptimizationEnabled(false);
        }
        isCachingEnabled = cachingEnabled;
    }

    public void setInvokerOptimizationEnabled(boolean enabled)
    {
        if (invokerOptimization == enabled) {
            return;
        }
        if (!enabled) {
            invokerMaster = null;
        }
        invokerOptimization = enabled;
    }

    static void embed(ScriptableObject top)
    {
        if (top instanceof GlobalScope) Context.codeBug();
        GlobalScope global = new GlobalScope();
        top.defineProperty("__globalScope", global,
                               ScriptableObject.PERMANENT
                               | ScriptableObject.READONLY
                               | ScriptableObject.DONTENUM);
    }

    boolean isCachingEnabled = true;
    transient Hashtable classTable = new Hashtable();

    boolean invokerOptimization = true;
    transient Object invokerMaster;

}


