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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Norris Boyd
 * Igor Bukanov
 * Matthias Radestock
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

// API class

package org.mozilla.javascript;

import java.io.Serializable;

/**
 * Class ImporterTopLevel
 *
 * This class defines a ScriptableObject that can be instantiated
 * as a top-level ("global") object to provide functionality similar
 * to Java's "import" statement.
 * <p>
 * This class can be used to create a top-level scope using the following code:
 * <pre>
 *  Scriptable scope = new ImporterTopLevel(cx);
 * </pre>
 * Then JavaScript code will have access to the following methods:
 * <ul>
 * <li>importClass - will "import" a class by making its unqualified name
 *                   available as a property of the top-level scope
 * <li>importPackage - will "import" all the classes of the package by
 *                     searching for unqualified names as classes qualified
 *                     by the given package.
 * </ul>
 * The following code from the shell illustrates this use:
 * <pre>
 * js> importClass(java.io.File)
 * js> f = new File('help.txt')
 * help.txt
 * js> importPackage(java.util)
 * js> v = new Vector()
 * []
 *
 * @author Norris Boyd
 */
public class ImporterTopLevel extends IdScriptable
{
    public ImporterTopLevel() { }

    public ImporterTopLevel(Context cx) {
        this(cx, false);
    }

    public ImporterTopLevel(Context cx, boolean sealed)
    {
        initStandardObjects(cx, sealed);
    }

    public String getClassName()
    {
        return (topScopeFlag) ? "global" : "JavaImporter";
    }

    public static void init(Context cx, Scriptable scope, boolean sealed)
    {
        ImporterTopLevel obj = new ImporterTopLevel();
        obj.prototypeFlag = true;
        obj.addAsPrototype(MAX_PROTOTYPE_ID, cx, scope, sealed);
    }

    public void initStandardObjects(Context cx, boolean sealed)
    {
        cx.initStandardObjects(this, sealed);
        prototypeFlag = true;
        addAsPrototype(MAX_PROTOTYPE_ID, cx, this, sealed);
        // set the flag after IdScriptable.addAsPrototype() calls
        // getClassName() to get constructor name right.
        topScopeFlag = true;
    }

    public boolean has(String name, Scriptable start) {
        return super.has(name, start)
               || getPackageProperty(name, start) != NOT_FOUND;
    }

    public Object get(String name, Scriptable start) {
        Object result = super.get(name, start);
        if (result != NOT_FOUND)
            return result;
        result = getPackageProperty(name, start);
        return result;
    }

    private Object getPackageProperty(String name, Scriptable start) {
        Object result = NOT_FOUND;
        Object[] elements;
        synchronized (importedPackages) {
            elements = importedPackages.toArray();
        }
        for (int i=0; i < elements.length; i++) {
            NativeJavaPackage p = (NativeJavaPackage) elements[i];
            Object v = p.getPkgProperty(name, start, false);
            if (v != null && !(v instanceof NativeJavaPackage)) {
                if (result == NOT_FOUND) {
                    result = v;
                } else {
                    throw Context.reportRuntimeError2(
                        "msg.ambig.import", result.toString(), v.toString());
                }
            }
        }
        return result;
    }

    /**
     * @deprecated Kept only for compatibility.
     */
    public void importPackage(Context cx, Scriptable thisObj, Object[] args,
                              Function funObj)
    {
        js_importPackage(args);
    }

    private Object js_construct(Scriptable scope, Object[] args)
    {
        ImporterTopLevel result = new ImporterTopLevel();
        for (int i = 0; i != args.length; ++i) {
            Object arg = args[i];
            if (arg instanceof NativeJavaClass) {
                result.importClass((NativeJavaClass)arg);
            } else if (arg instanceof NativeJavaPackage) {
                result.importPackage((NativeJavaPackage)arg);
            } else {
                throw Context.reportRuntimeError1(
                    "msg.not.class.not.pkg", Context.toString(arg));
            }
        }
        // set explicitly prototype and scope
        // as otherwise in topScopeFlag mode BaseFunction.construct
        // would keep them set to null. It also allow to use
        // JavaImporter without new and still get properly
        // initialized object.
        result.setParentScope(scope);
        result.setPrototype(this);
        return result;
    }

    private Object js_importClass(Object[] args)
    {
        for (int i = 0; i != args.length; i++) {
            Object arg = args[i];
            if (!(arg instanceof NativeJavaClass)) {
                throw Context.reportRuntimeError1(
                    "msg.not.class", Context.toString(arg));
            }
            importClass((NativeJavaClass)arg);
        }
        return Undefined.instance;
    }

    private Object js_importPackage(Object[] args)
    {
        for (int i = 0; i != args.length; i++) {
            Object arg = args[i];
            if (!(arg instanceof NativeJavaPackage)) {
                throw Context.reportRuntimeError1(
                    "msg.not.pkg", Context.toString(arg));
            }
            importPackage((NativeJavaPackage)arg);
        }
        return Undefined.instance;
    }

    private void importPackage(NativeJavaPackage pkg)
    {
        synchronized (importedPackages) {
            for (int j = 0; j != importedPackages.size(); j++) {
                if (pkg == importedPackages.get(j)) {
                    pkg = null;
                    break;
                }
            }
            if (pkg != null) {
                importedPackages.add(pkg);
            }
        }
    }

    private void importClass(NativeJavaClass cl)
    {
        String s = cl.getClassObject().getName();
        String n = s.substring(s.lastIndexOf('.')+1);
        Object val = get(n, this);
        if (val != NOT_FOUND && val != cl) {
            throw Context.reportRuntimeError1("msg.prop.defined", n);
        }
        //defineProperty(n, cl, DONTENUM);
        put(n, this, cl);
    }

    public Object execMethod(IdFunction f, Context cx, Scriptable scope,
	                         Scriptable thisObj, Object[] args)
    {
        if (prototypeFlag) {
            switch (f.methodId) {
              case Id_constructor:
                return js_construct(scope, args);

              case Id_importClass:
                return realThis(thisObj, f).js_importClass(args);

              case Id_importPackage:
                return realThis(thisObj, f).js_importPackage(args);
            }
        }
        return super.execMethod(f, cx, scope, thisObj, args);
    }

    private static ImporterTopLevel realThis(Scriptable thisObj, IdFunction f)
    {
        if (!(thisObj instanceof ImporterTopLevel))
            throw incompatibleCallError(f);
        return (ImporterTopLevel)thisObj;
    }

    protected String getIdName(int id)
    {
        if (prototypeFlag) {
            switch (id) {
              case Id_constructor:   return "constructor";
              case Id_importClass:   return "importClass";
              case Id_importPackage: return "importPackage";
            }
        }
        return super.getIdName(id);
    }

    public int methodArity(int methodId) {
        if (prototypeFlag) {
            switch (methodId) {
              case Id_constructor:   return 0;
              case Id_importClass:   return 1;
              case Id_importPackage: return 1;
            }
        }
        return super.methodArity(methodId);
    }

// #string_id_map#

    protected int mapNameToId(String s) {
        if (!prototypeFlag) { return 0; }
        int id;
// #generated# Last update: 2004-06-08 02:03:11 CEST
        L0: { id = 0; String X = null; int c;
            int s_length = s.length();
            if (s_length==11) {
                c=s.charAt(0);
                if (c=='c') { X="constructor";id=Id_constructor; }
                else if (c=='i') { X="importClass";id=Id_importClass; }
            }
            else if (s_length==13) { X="importPackage";id=Id_importPackage; }
            if (X!=null && X!=s && !X.equals(s)) id = 0;
        }
// #/generated#
        return id;
    }

    private static final int
        Id_constructor          = 1,
        Id_importClass          = 2,
        Id_importPackage        = 3,
        MAX_PROTOTYPE_ID        = 3;

// #/string_id_map#

    private ObjArray importedPackages = new ObjArray();

    private boolean prototypeFlag;
    private boolean topScopeFlag;
}
