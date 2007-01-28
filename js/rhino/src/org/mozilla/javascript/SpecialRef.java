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
 * Portions created by the Initial Developer are Copyright (C) 1997-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Igor Bukanov, igor@fastmail.fm
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

class SpecialRef extends Ref
{
    static final long serialVersionUID = -7521596632456797847L;

    private static final int SPECIAL_NONE = 0;
    private static final int SPECIAL_PROTO = 1;
    private static final int SPECIAL_PARENT = 2;

    private Scriptable target;
    private int type;
    private String name;

    private SpecialRef(Scriptable target, int type, String name)
    {
        this.target = target;
        this.type = type;
        this.name = name;
    }

    static Ref createSpecial(Context cx, Object object, String name)
    {
        Scriptable target = ScriptRuntime.toObjectOrNull(cx, object);
        if (target == null) {
            throw ScriptRuntime.undefReadError(object, name);
        }

        int type;
        if (name.equals("__proto__")) {
            type = SPECIAL_PROTO;
        } else if (name.equals("__parent__")) {
            type = SPECIAL_PARENT;
        } else {
            throw new IllegalArgumentException(name);
        }

        if (!cx.hasFeature(Context.FEATURE_PARENT_PROTO_PROPERTIES)) {
            // Clear special after checking for valid name!
            type = SPECIAL_NONE;
        }

        return new SpecialRef(target, type, name);
    }

    public Object get(Context cx)
    {
        switch (type) {
          case SPECIAL_NONE:
            return ScriptRuntime.getObjectProp(target, name, cx);
          case SPECIAL_PROTO:
            return target.getPrototype();
          case SPECIAL_PARENT:
            return target.getParentScope();
          default:
            throw Kit.codeBug();
        }
    }

    public Object set(Context cx, Object value)
    {
        switch (type) {
          case SPECIAL_NONE:
            return ScriptRuntime.setObjectProp(target, name, value, cx);
          case SPECIAL_PROTO:
          case SPECIAL_PARENT:
            {
                Scriptable obj = ScriptRuntime.toObjectOrNull(cx, value);
                if (obj != null) {
                    // Check that obj does not contain on its prototype/scope
                    // chain to prevent cycles
                    Scriptable search = obj;
                    do {
                        if (search == target) {
                            throw Context.reportRuntimeError1(
                                "msg.cyclic.value", name);
                        }
                        if (type == SPECIAL_PROTO) {
                            search = search.getPrototype();
                        } else {
                            search = search.getParentScope();
                        }
                    } while (search != null);
                }
                if (type == SPECIAL_PROTO) {
                    target.setPrototype(obj);
                } else {
                    target.setParentScope(obj);
                }
                return obj;
            }
          default:
            throw Kit.codeBug();
        }
    }

    public boolean has(Context cx)
    {
        if (type == SPECIAL_NONE) {
            return ScriptRuntime.hasObjectElem(target, name, cx);
        }
        return true;
    }

    public boolean delete(Context cx)
    {
        if (type == SPECIAL_NONE) {
            return ScriptRuntime.deleteObjectElem(target, name, cx);
        }
        return false;
    }
}

