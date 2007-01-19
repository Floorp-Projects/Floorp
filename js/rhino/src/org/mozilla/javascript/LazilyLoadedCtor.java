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
 *   Igor Bukanov
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

package org.mozilla.javascript;

import java.lang.reflect.*;

/**
 * Avoid loading classes unless they are used.
 *
 * <p> This improves startup time and average memory usage.
 */
public final class LazilyLoadedCtor implements java.io.Serializable {
	private static final long serialVersionUID = 1L;
	
    private static final int STATE_BEFORE_INIT = 0;
    private static final int STATE_INITIALIZING = 1;
    private static final int STATE_WITH_VALUE = 2;

    private final ScriptableObject scope;
    private final String propertyName;
    private final String className;
    private final boolean sealed;
    private Object initializedValue;
    private int state;

    public LazilyLoadedCtor(ScriptableObject scope, String propertyName,
                            String className, boolean sealed)
    {

        this.scope = scope;
        this.propertyName = propertyName;
        this.className = className;
        this.sealed = sealed;
        this.state = STATE_BEFORE_INIT;

        scope.addLazilyInitializedValue(propertyName, 0, this,
                                        ScriptableObject.DONTENUM);
    }

    void init()
    {
        synchronized (this) {
            if (state == STATE_INITIALIZING)
                throw new IllegalStateException(
                    "Recursive initialization for "+propertyName);
            if (state == STATE_BEFORE_INIT) {
                state = STATE_INITIALIZING;
                // Set value now to have something to set in finaly block if
                // buildValue throws.
                Object value = Undefined.instance;
                try {
                    value = buildValue();
                } finally {
                    initializedValue = value;
                    state = STATE_WITH_VALUE;
                }
            }
        }
    }

    Object getValue()
    {
        if (state != STATE_WITH_VALUE)
            throw new IllegalStateException(propertyName);
        return initializedValue;
    }

    private Object buildValue()
    {
        Class cl = Kit.classOrNull(className);
        if (cl != null) {
            try {
                Object value = ScriptableObject.buildClassCtor(scope, cl,
                                                               sealed, false);
                if (value == null) {
                    // cl has own static initializer which is expected
                    // to set the property on its own.
                    value = scope.get(propertyName, scope);
                    if (value != Scriptable.NOT_FOUND)
                        return value;
                }
            } catch (InvocationTargetException ex) {
                Throwable target = ex.getTargetException();
                if (target instanceof RuntimeException) {
                    throw (RuntimeException)target;
                }
            } catch (RhinoException ex) {
            } catch (InstantiationException ex) {
            } catch (IllegalAccessException ex) {
            } catch (SecurityException ex) {
            }
        }
        return Undefined.instance;
    }

}
