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

package org.mozilla.javascript;

import java.io.Serializable;

/**
 * This class implements the Undefined value in JavaScript.
 */
public class Undefined implements Scriptable, Serializable
{
    static final public Scriptable instance = new Undefined();

    public String getClassName() {
        return "undefined";
    }

    public boolean has(String id, Scriptable start) {
        return false;
    }

    public boolean has(int index, Scriptable start) {
        return false;
    }

    public Object get(String id, Scriptable start) {
        throw reportError();
    }

    public Object get(int index, Scriptable start) {
        throw reportError();
    }

    public void put(String id,Scriptable start, Object value) {
        throw reportError();
    }

    public void put(int index, Scriptable start, Object value) {
        throw reportError();
    }

    public void delete(String id) {
        throw reportError();
    }

    public void delete(int index) {
        throw reportError();
    }

    public short getAttributes(String id, Scriptable start) {
        throw reportError();
    }

    public short getAttributes(int index, Scriptable start) {
        throw reportError();
    }

    public void setAttributes(String id, Scriptable start, short attributes) {
        throw reportError();
    }

    public void setAttributes(int index, Scriptable start, short attributes) {
        throw reportError();
    }

    public Scriptable getPrototype() {
        throw reportError();
    }

    public void setPrototype(Scriptable prototype) {
        throw reportError();
    }

    public Scriptable getParentScope() {
        throw reportError();
    }

    public void setParentScope(Scriptable parent) {
        throw reportError();
    }

    public Object[] getIds() {
        throw reportError();
    }

    public Object getDefaultValue(Class cl) {
        if (cl == ScriptRuntime.StringClass)
            return "undefined";
        if (cl == ScriptRuntime.NumberClass)
            return ScriptRuntime.NaNobj;
        if (cl == ScriptRuntime.BooleanClass)
            return Boolean.FALSE;
        return this;
    }

    public boolean hasInstance(Scriptable value) {
        throw reportError();
    }

    private RuntimeException reportError() {
        return Context.reportRuntimeError0("msg.undefined");
    }

    public Object readResolve() {
        return Undefined.instance;
    }
}
