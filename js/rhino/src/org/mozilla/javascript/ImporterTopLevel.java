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
        if (result == NOT_FOUND && importedPackages != null) {
            for (int i=0; i < importedPackages.size(); i++) {
                Object o = importedPackages.elementAt(i);
                NativeJavaPackage p = (NativeJavaPackage) o;
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
        }
        return result;          
    }
    
    public void importClass(Object cl) {
        if (!(cl instanceof NativeJavaClass)) {
            String[] args = { Context.toString(cl) };
            throw Context.reportRuntimeError(
                Context.getMessage("msg.not.class", args));
        }
        String s = ((NativeJavaClass) cl).getClassObject().getName();
        String n = s.substring(s.lastIndexOf('.')+1);
        Object val = this.get(n, this);
        if (val != NOT_FOUND && val != cl) {
            String[] args = { n };
            throw Context.reportRuntimeError(
                Context.getMessage("msg.prop.defined", args));
        }
        this.defineProperty(n, cl, DONTENUM);
    }
    
    public void importPackage(Object pkg) {
        if (importedPackages == null)
            importedPackages = new Vector();
        if (!(pkg instanceof NativeJavaPackage)) {
            String[] args = { Context.toString(pkg) };
            throw Context.reportRuntimeError(
                Context.getMessage("msg.not.pkg", args));
        }
        for (int i=0; i < importedPackages.size(); i++) {
            if (pkg == importedPackages.elementAt(i))
                return;     // allready in list
        }
        importedPackages.addElement(pkg);
    }
    
    private Vector importedPackages;
}
