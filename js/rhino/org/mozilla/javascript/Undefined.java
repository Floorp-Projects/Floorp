/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package org.mozilla.javascript;

/**
 * This class implements the Undefined value in JavaScript.
 */
public class Undefined implements Scriptable {

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

    public boolean instanceOf(Scriptable prototype) {
        return false;
    }

    private RuntimeException reportError() {
        String message = Context.getMessage("msg.undefined", null);
        return Context.reportRuntimeError(message);
    }
}
