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

public final class Continuation extends IdScriptableObject implements Function
{
    private static final Object FTAG = new Object();

    Object data;

    static void init(Context cx, Scriptable scope, boolean sealed)
    {
        Continuation obj = new Continuation();
        obj.exportAsJSClass(MAX_PROTOTYPE_ID, scope, sealed);
    }

    public String getClassName()
    {
        return "Continuation";
    }

    public Scriptable construct(Context cx, Scriptable scope, Object[] args)
    {
        throw Context.reportRuntimeError("Direct call is not supported");
    }

    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
    {
        return Interpreter.restartContinuation(this, cx, scope, args);
    }

    static boolean isContinuationConstructor(IdFunctionObject f)
    {
        if (f.hasTag(FTAG) && f.methodId() == Id_constructor) {
            return true;
        }
        return false;
    }

    protected void initPrototypeId(int id)
    {
        String s;
        int arity;
        switch (id) {
          case Id_constructor: arity=0; s="constructor"; break;
          default: throw new IllegalArgumentException(String.valueOf(id));
        }
        initPrototypeMethod(FTAG, id, s, arity);
    }

    public Object execIdCall(IdFunctionObject f, Context cx, Scriptable scope,
                             Scriptable thisObj, Object[] args)
    {
        if (!f.hasTag(FTAG)) {
            return super.execIdCall(f, cx, scope, thisObj, args);
        }
        int id = f.methodId();
        switch (id) {
          case Id_constructor:
            throw Context.reportRuntimeError("Direct call is not supported");
        }
        throw new IllegalArgumentException(String.valueOf(id));
    }

// #string_id_map#

    protected int findPrototypeId(String s)
    {
        int id;
// #generated# Last update: 2004-09-08 12:02:41 CEST
        L0: { id = 0; String X = null;
            if (s.length()==11) { X="constructor";id=Id_constructor; }
            if (X!=null && X!=s && !X.equals(s)) id = 0;
        }
// #/generated#
        return id;
    }

    private static final int
        Id_constructor          = 1,
        MAX_PROTOTYPE_ID        = 1;

// #/string_id_map#
}
