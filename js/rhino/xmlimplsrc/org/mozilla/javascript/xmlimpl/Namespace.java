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
 * Ethan Hugg
 * Terry Lucas
 * Milen Nankov
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

package org.mozilla.javascript.xmlimpl;

import org.mozilla.javascript.*;

/**
 * Class Namespace
 *
 */
class Namespace extends IdScriptable
{
    private static final Object NAMESPACE_TAG = new Object();

    private XMLLibImpl lib;
    private String prefix;
    private String uri;

    public Namespace(XMLLibImpl lib, String uri)
    {
        if (uri == null)
            throw new IllegalArgumentException();

        this.lib = lib;
        this.prefix = (uri.length() == 0) ? "" : null;
        this.uri = uri;
    }


    public Namespace(XMLLibImpl lib, String prefix, String uri)
    {
        if (uri == null)
            throw new IllegalArgumentException();
        if (uri.length() == 0) {
            // prefix should be "" for empty uri
            if (prefix == null)
                throw new IllegalArgumentException();
            if (prefix.length() != 0)
                throw new IllegalArgumentException();
        }

        this.lib = lib;
        this.prefix = prefix;
        this.uri = uri;
    }

    public void exportAsJSClass(boolean sealed)
    {
        exportAsJSClass(MAX_PROTOTYPE_ID, lib.globalScope(), sealed);
    }

    /**
     *
     * @return
     */
    public String uri()
    {
        return uri;
    }

    /**
     *
     * @return
     */
    public String prefix()
    {
        return prefix;
    }

    /**
     *
     * @return
     */
    public String toString ()
    {
        return uri();
    }

    /**
     *
     * @return
     */
    public String toLocaleString ()
    {
        return toString();
    }

    public boolean equals(Object obj)
    {
        if (!(obj instanceof Namespace)) return false;
        return equivalentValues(obj).booleanValue();
    }

    public Boolean equivalentValues(Object value)
    {
        if (!(value instanceof Namespace)) return null;
        Namespace n = (Namespace)value;
        return uri().equals(n.uri()) ? Boolean.TRUE : Boolean.FALSE;
    }

    /**
     *
     * @return
     */
    public String getClassName ()
    {
        return "Namespace";
    }

    /**
     *
     * @param hint
     * @return
     */
    public Object getDefaultValue (Class hint)
    {
        return uri();
    }

    protected Scriptable defaultPrototype()
    {
        Scriptable result = lib.namespacePrototype;
        if (result == this) {
            result = null;
        }
        return result;
    }

    protected Scriptable defaultParentScope()
    {
        return lib.globalScope();
    }

// #string_id_map#
    private static final int
        Id_prefix               = 1,
        Id_uri                  = 2,
        MAX_INSTANCE_ID         = 2;

// maxId for superclass
    private static int idBase = -1;

    {
        if (idBase < 0) idBase = getMaxInstanceId();
        setMaxInstanceId(idBase, idBase + MAX_INSTANCE_ID);
    }

    protected int findInstanceIdInfo(String s)
    {
        int id;
// #generated# Last update: 2004-07-20 19:50:50 CEST
        L0: { id = 0; String X = null;
            int s_length = s.length();
            if (s_length==3) { X="uri";id=Id_uri; }
            else if (s_length==6) { X="prefix";id=Id_prefix; }
            if (X!=null && X!=s && !X.equals(s)) id = 0;
        }
// #/generated#

        if (id == 0) return super.findInstanceIdInfo(s);

        int attr;
        switch (id) {
          case Id_prefix:
          case Id_uri:
            attr = PERMANENT | READONLY;
            break;
          default: throw new IllegalStateException();
        }
        return instanceIdInfo(attr, idBase + id);
    }
// #/string_id_map#

    protected String getInstanceIdName(int id)
    {
        switch (id - idBase) {
          case Id_prefix: return "prefix";
          case Id_uri: return "uri";
        }
        return super.getInstanceIdName(id);
    }

    protected Object getInstanceIdValue(int id)
    {
        switch (id - idBase) {
          case Id_prefix:
            if (prefix == null) return Undefined.instance;
            return prefix;
          case Id_uri:
            return uri;
        }
        return super.getInstanceIdValue(id);
    }


// #string_id_map#
    private static final int
        Id_constructor          = 1,
        Id_toString             = 2,
        MAX_PROTOTYPE_ID        = 2;

    protected int findPrototypeId(String s)
    {
        int id;
// #generated# Last update: 2004-07-20 19:45:27 CEST
        L0: { id = 0; String X = null;
            int s_length = s.length();
            if (s_length==8) { X="toString";id=Id_toString; }
            else if (s_length==11) { X="constructor";id=Id_constructor; }
            if (X!=null && X!=s && !X.equals(s)) id = 0;
        }
// #/generated#
        return id;
    }
// #/string_id_map#

    protected void initPrototypeId(int id)
    {
        String s;
        int arity;
        switch (id) {
          case Id_constructor: arity=2; s="constructor"; break;
          case Id_toString:    arity=0; s="toString";    break;
          default: throw new IllegalArgumentException(String.valueOf(id));
        }
        initPrototypeMethod(NAMESPACE_TAG, id, s, arity);
    }

    public Object execMethod(IdFunction f,
                             Context cx,
                             Scriptable scope,
                             Scriptable thisObj,
                             Object[] args)
        throws JavaScriptException
    {
        if (!f.hasTag(NAMESPACE_TAG)) {
            return super.execMethod(f, cx, scope, thisObj, args);
        }
        int id = f.methodId();
        if (id == Id_constructor) {
            return jsConstructor(cx, (thisObj == null), args);
        } else if(id == Id_toString) {
            return realThis(thisObj, f).jsFunction_toString();
        }
        throw new IllegalArgumentException(String.valueOf(id));
    }

    private Namespace realThis(Scriptable thisObj, IdFunction f)
    {
        if(!(thisObj instanceof Namespace))
            throw incompatibleCallError(f);
        return (Namespace)thisObj;
    }

    private Object jsConstructor(Context cx, boolean inNewExpr, Object[] args)
    {
        if (!inNewExpr && args.length == 1) {
            return lib.castToNamespace(cx, args[0]);
        }

        if (args.length == 0) {
            return lib.constructNamespace(cx);
        } else if (args.length == 1) {
            return lib.constructNamespace(cx, args[0]);
        } else {
            return lib.constructNamespace(cx, args[0], args[1]);
        }
    }

    private String jsFunction_toString()
    {
        return toString();
    }
}
