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
 * May 6, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
        Environment.java

        Wraps java.lang.System properties.

        by Patrick C. Beard <beard@netscape.com>
 */

package org.mozilla.javascript.tools.shell;

import org.mozilla.javascript.Scriptable;
import org.mozilla.javascript.ScriptRuntime;
import org.mozilla.javascript.ScriptableObject;

import java.util.Vector;
import java.util.Enumeration;
import java.util.Properties;

/**
 * Environment, intended to be instantiated at global scope, provides
 * a natural way to access System properties from JavaScript.
 *
 * @author Patrick C. Beard
 */
public class Environment extends ScriptableObject
{
    static final long serialVersionUID = -430727378460177065L;

    private Environment thePrototypeInstance = null;

    public static void defineClass(ScriptableObject scope) {
        try {
            ScriptableObject.defineClass(scope, Environment.class);
        } catch (Exception e) {
            throw new Error(e.getMessage());
        }
    }

    public String getClassName() {
        return "Environment";
    }

    public Environment() {
        if (thePrototypeInstance == null)
            thePrototypeInstance = this;
    }

    public Environment(ScriptableObject scope) {
        setParentScope(scope);
        Object ctor = ScriptRuntime.getTopLevelProp(scope, "Environment");
        if (ctor != null && ctor instanceof Scriptable) {
            Scriptable s = (Scriptable) ctor;
            setPrototype((Scriptable) s.get("prototype", s));
        }
    }

    public boolean has(String name, Scriptable start) {
        if (this == thePrototypeInstance)
            return super.has(name, start);

        return (System.getProperty(name) != null);
    }

    public Object get(String name, Scriptable start) {
        if (this == thePrototypeInstance)
            return super.get(name, start);

        String result = System.getProperty(name);
        if (result != null)
            return ScriptRuntime.toObject(getParentScope(), result);
        else
            return Scriptable.NOT_FOUND;
    }

    public void put(String name, Scriptable start, Object value) {
        if (this == thePrototypeInstance)
            super.put(name, start, value);
        else
            System.getProperties().put(name, ScriptRuntime.toString(value));
    }

    private Object[] collectIds() {
        Properties props = System.getProperties();
        Enumeration names = props.propertyNames();
        Vector keys = new Vector();
        while (names.hasMoreElements())
            keys.addElement(names.nextElement());
        Object[] ids = new Object[keys.size()];
        keys.copyInto(ids);
        return ids;
    }

    public Object[] getIds() {
        if (this == thePrototypeInstance)
            return super.getIds();
        return collectIds();
    }

    public Object[] getAllIds() {
        if (this == thePrototypeInstance)
            return super.getAllIds();
        return collectIds();
    }
}
