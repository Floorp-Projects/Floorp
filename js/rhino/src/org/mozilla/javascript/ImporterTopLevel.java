/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
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

import java.util.Vector;

/**
 * Class ImporterTopLevel
 * 
 * This class defines a ScriptableObject that can be instantiated 
 * as a top-level ("global") object to provide functionality similar
 * to Java's "import" statement.
 * <p>
 * This class can be used to create a top-level scope using the following code: 
 * <pre>
 *  Scriptable scope = cx.initStandardObjects(new ImporterTopLevel());
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
public class ImporterTopLevel extends ScriptableObject {
    
    public ImporterTopLevel() {
        String[] names = { "importClass", "importPackage" };

        try {
            this.defineFunctionProperties(names, ImporterTopLevel.class,
                                          ScriptableObject.DONTENUM);
        } catch (PropertyException e) {
            throw new Error();  // should never happen
        }
    }
    
    public String getClassName() { 
        return "global";
    }
    
    public Object get(String name, Scriptable start) {
        Object result = super.get(name, start);
        if (result != NOT_FOUND) 
            return result;
        if (name.equals("_packages_")) 
            return result;
        Object plist = ScriptableObject.getProperty(start,"_packages_");
        if (plist == NOT_FOUND) 
            return result;
        Context cx = Context.enter();
        Object[] elements = cx.getElements((Scriptable)plist);
        Context.exit();
        for (int i=0; i < elements.length; i++) {
            NativeJavaPackage p = (NativeJavaPackage) elements[i];
            Object v = p.getPkgProperty(name, start, false);
            if (v != null && !(v instanceof NativeJavaPackage)) {
                if (result == NOT_FOUND) {
                    result = v;
                } else {
                    String[] args = { result.toString(), v.toString() };
                    throw Context.reportRuntimeError(
                        Context.getMessage("msg.ambig.import", 
                        args));
                }
            }
        }
        return result;
    }
    
    public static void importClass(Context cx, Scriptable thisObj,
                                   Object[] args, Function funObj) {
        for (int i=0; i<args.length; i++) {
            Object cl = args[i];
            if (!(cl instanceof NativeJavaClass)) {
                String[] eargs = { Context.toString(cl) };
                throw Context.reportRuntimeError(Context.getMessage("msg.not.class", eargs));
            }
            String s = ((NativeJavaClass) cl).getClassObject().getName();
            String n = s.substring(s.lastIndexOf('.')+1);
            Object val = thisObj.get(n, thisObj);
            if (val != NOT_FOUND && val != cl) {
                String[] eargs = { n };
                throw Context.reportRuntimeError(Context.getMessage("msg.prop.defined", eargs));
            }
            //thisObj.defineProperty(n, cl, DONTENUM);
            thisObj.put(n,thisObj,cl);
        }
    }
    
    public static void importPackage(Context cx, Scriptable thisObj,
                                   Object[] args, Function funObj) {
        Scriptable importedPackages;
        Object plist = thisObj.get("_packages_", thisObj);
        if (plist == NOT_FOUND) {
            importedPackages = cx.newArray(thisObj,0);
            thisObj.put("_packages_", thisObj, importedPackages);
        }
        else {
            importedPackages = (Scriptable)plist;
        }
        for (int i=0; i<args.length; i++) {
            Object pkg = args[i];
            if (!(pkg instanceof NativeJavaPackage)) {
                String[] eargs = { Context.toString(pkg) };
                throw Context.reportRuntimeError(Context.getMessage("msg.not.pkg", eargs));
            }
            Object[] elements = cx.getElements(importedPackages);
            for (int j=0; j < elements.length; j++) {
                if (pkg == elements[j]) {
                    pkg = null;
                    break;
                }
            }
            if (pkg != null)
                importedPackages.put(elements.length,importedPackages,pkg);
        }
    }
}
