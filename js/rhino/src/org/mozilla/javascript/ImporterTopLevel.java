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
public class ImporterTopLevel extends GlobalScope {

    /**
     * @deprecated
     */
    public ImporterTopLevel() {
        init();
    }

    public ImporterTopLevel(Context cx) {
        this(cx, false);
    }

    public ImporterTopLevel(Context cx, boolean sealed) {
        cx.initStandardObjects(this, sealed);
        init();
    }

    private void init() {
        ImporterFunctions.setup(this);
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

    void importClass(Context cx, Scriptable thisObj, Object[] args)
    {
        for (int i=0; i<args.length; i++) {
            Object cl = args[i];
            if (!(cl instanceof NativeJavaClass)) {
                throw Context.reportRuntimeError1(
                    "msg.not.class", Context.toString(cl));
            }
            String s = ((NativeJavaClass) cl).getClassObject().getName();
            String n = s.substring(s.lastIndexOf('.')+1);
            Object val = thisObj.get(n, thisObj);
            if (val != NOT_FOUND && val != cl) {
                throw Context.reportRuntimeError1("msg.prop.defined", n);
            }
            //thisObj.defineProperty(n, cl, DONTENUM);
            thisObj.put(n,thisObj,cl);
        }
    }


    public void importPackage(Context cx, Scriptable thisObj, Object[] args,
                              Function funObj)
    {
        importPackage(cx, thisObj, args);
    }

    void importPackage(Context cx, Scriptable thisObj, Object[] args)
    {
        for (int i = 0; i != args.length; i++) {
            Object pkg = args[i];
            if (!(pkg instanceof NativeJavaPackage)) {
                throw Context.reportRuntimeError1(
                    "msg.not.pkg", Context.toString(pkg));
            }
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
    }

    private ObjArray importedPackages = new ObjArray();
}

final class ImporterFunctions extends JIFunction
{
    private ImporterFunctions(ImporterTopLevel importer, int id)
    {
        this.importer = importer;
        this.id = id;
        if (id == Id_importClass) {
            initNameArity("importClass", 1);
        } else {
            if (id != Id_importPackage) Kit.codeBug();
            initNameArity("importPackage", 1);
        }
        defineAsProperty(importer);
    }

    static void setup(ImporterTopLevel importer)
    {
        new ImporterFunctions(importer, Id_importClass);
        new ImporterFunctions(importer, Id_importPackage);
    }

    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
        throws JavaScriptException
    {
        if (id == Id_importClass) {
            importer.importClass(cx, thisObj, args);
        } else {
            if (id != Id_importPackage) Kit.codeBug();
            importer.importPackage(cx, thisObj, args);
        }
        return Undefined.instance;
    }

    private static final int
        Id_importClass    =  1,
        Id_importPackage  =  2;

    private ImporterTopLevel importer;
    private int id;
}
