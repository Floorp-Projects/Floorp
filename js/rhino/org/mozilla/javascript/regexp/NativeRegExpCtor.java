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

package org.mozilla.javascript.regexp;

import org.mozilla.javascript.*;
import java.lang.reflect.Method;

/**
 * This class implements the RegExp constructor native object.
 *
 * Revision History:
 * Implementation in C by Brendan Eich
 * Initial port to Java by Norris Boyd from jsregexp.c version 1.36
 * Merged up to version 1.38, which included Unicode support.
 * Merged bug fixes in version 1.39.
 * Merged JSFUN13_BRANCH changes up to 1.32.2.11
 *
 * @author Brendan Eich
 * @author Norris Boyd
 */
public class NativeRegExpCtor extends NativeFunction {

    public static Scriptable init(Scriptable scope)
        throws PropertyException
    {
        NativeRegExpCtor ctor = new NativeRegExpCtor();

        String[] names = { "RegExp" };
        ctor.names = names;

        ctor.setPrototype(getClassPrototype(scope, "Function"));
        ctor.setParentScope(scope);

        String[] propNames = { "multiline", "input",        "lastMatch",
                               "lastParen", "leftContext",  "rightContext" };

        String[] aliases =   { "$*",        "$_",           "$&",
                               "$+",        "$`",           "$'" };

        for (int i=0; i < propNames.length; i++)
            ctor.defineProperty(propNames[i], NativeRegExpCtor.class,
                                ScriptableObject.DONTENUM);
        for (int i=0; i < aliases.length; i++) {
            StringBuffer buf = new StringBuffer("get");
            buf.append(propNames[i]);
            buf.setCharAt(3, Character.toUpperCase(propNames[i].charAt(0)));
            Method[] getter = FunctionObject.findMethods(
                                NativeRegExpCtor.class,
                                buf.toString());
            buf.setCharAt(0, 's');
            Method[] setter = FunctionObject.findMethods(
                                NativeRegExpCtor.class,
                                buf.toString());
            Method m = setter == null ? null : setter[0];
            ctor.defineProperty(aliases[i], null, getter[0], m,
                                ScriptableObject.DONTENUM);
        }

        // We know that scope is a Scriptable object since we
        // constrained the type on initStandardObjects.
        ScriptableObject global = (ScriptableObject) scope;
        global.defineProperty("RegExp", ctor, ScriptableObject.DONTENUM);

        return ctor;
    }

    public String getClassName() {
        return "Function";
    }

    private int getDollarNumber(String s) {
        // assumes s starts with '$'
        if (s.length() != 2)
            return 0;
        char c = s.charAt(1);
        if (c < '0' || '9' < c)
            return 0;
        return c - '0';
    }

    public boolean has(String id, Scriptable start) {
        if (id != null && id.length() > 1 && id.charAt(0) == '$') {
            if (getDollarNumber(id) != 0)
                return true;
        }
        return super.has(id, start);
    }

    public Object get(String id, Scriptable start) {
        if (id.length() > 1 && id.charAt(0) == '$') {
            int dollar = getDollarNumber(id);
            if (dollar != 0) {
                dollar--;
                RegExpImpl res = getImpl();
                return res.getParenSubString(dollar).toString();
            }
        }
        return super.get(id, start);
    }

    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
    {
        return construct(cx, parent, args);
    }

    public Scriptable construct(Context cx, Scriptable scope, Object[] args) {
        NativeRegExp re = new NativeRegExp();
        NativeRegExp.compile(cx, re, args, this);
        re.setPrototype(getClassPrototype(scope, "RegExp"));
        re.setParentScope(getParentScope());
        return re;
    }

    public static boolean getMultiline(ScriptableObject obj) {
        return getImpl().multiline;
    }

    public static void setMultiline(ScriptableObject obj, boolean b) {
        getImpl().multiline = b;
    }

    public static String getInput(ScriptableObject obj) {
        String s = getImpl().input;
        return s == null ? "" : s;
    }

    public static void setInput(ScriptableObject obj, String s) {
        getImpl().input = s;
    }

    public static String getLastMatch(ScriptableObject obj) {
        SubString ss = getImpl().lastMatch;
        return ss == null ? "" : ss.toString();
    }

    public static String getLastParen(ScriptableObject obj) {
        SubString ss = getImpl().lastParen;
        return ss == null ? "" : ss.toString();
    }

    public static String getLeftContext(ScriptableObject obj) {
        SubString ss = getImpl().leftContext;
        return ss == null ? "" : ss.toString();
    }

    public static String getRightContext(ScriptableObject obj) {
        SubString ss = getImpl().rightContext;
        return ss == null ? "" : ss.toString();
    }

    static RegExpImpl getImpl() {
        Context cx = Context.getCurrentContext();
        return (RegExpImpl) ScriptRuntime.getRegExpProxy(cx);

    }

}
