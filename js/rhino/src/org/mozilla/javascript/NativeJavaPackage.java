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
 *   Norris Boyd
 *   Frank Mitchell
 *   Mike Shaver
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

/**
 * This class reflects Java packages into the JavaScript environment.  We
 * lazily reflect classes and subpackages, and use a caching/sharing
 * system to ensure that members reflected into one JavaPackage appear
 * in all other references to the same package (as with Packages.java.lang
 * and java.lang).
 *
 * @author Mike Shaver
 * @see NativeJavaArray
 * @see NativeJavaObject
 * @see NativeJavaClass
 */

public class NativeJavaPackage extends ScriptableObject
{
    static final long serialVersionUID = 7445054382212031523L;

    NativeJavaPackage(boolean internalUsage,
                      String packageName, ClassLoader classLoader)
    {
        this.packageName = packageName;
        this.classLoader = classLoader;
    }

    /**
     * @deprecated NativeJavaPackage is an internal class, do not use
     * it directly.
     */
    public NativeJavaPackage(String packageName, ClassLoader classLoader) {
        this(false, packageName, classLoader);
    }

    /**
     * @deprecated NativeJavaPackage is an internal class, do not use
     * it directly.
     */
    public NativeJavaPackage(String packageName) {
        this(false, packageName,
             Context.getCurrentContext().getApplicationClassLoader());
    }

    public String getClassName() {
        return "JavaPackage";
    }

    public boolean has(String id, Scriptable start) {
        return true;
    }

    public boolean has(int index, Scriptable start) {
        return false;
    }

    public void put(String id, Scriptable start, Object value) {
        // Can't add properties to Java packages.  Sorry.
    }

    public void put(int index, Scriptable start, Object value) {
        throw Context.reportRuntimeError0("msg.pkg.int");
    }

    public Object get(String id, Scriptable start) {
        return getPkgProperty(id, start, true);
    }

    public Object get(int index, Scriptable start) {
        return NOT_FOUND;
    }

    // set up a name which is known to be a package so we don't
    // need to look for a class by that name
    void forcePackage(String name, Scriptable scope)
    {
        NativeJavaPackage pkg;
        int end = name.indexOf('.');
        if (end == -1) {
            end = name.length();
        }

        String id = name.substring(0, end);
        Object cached = super.get(id, this);
        if (cached != null && cached instanceof NativeJavaPackage) {
            pkg = (NativeJavaPackage) cached;
        } else {
            String newPackage = packageName.length() == 0
                                ? id
                                : packageName + "." + id;
            pkg = new NativeJavaPackage(true, newPackage, classLoader);
            ScriptRuntime.setObjectProtoAndParent(pkg, scope);
            super.put(id, this, pkg);
        }
        if (end < name.length()) {
            pkg.forcePackage(name.substring(end+1), scope);
        }
    }

    synchronized Object getPkgProperty(String name, Scriptable start,
                                       boolean createPkg)
    {
        Object cached = super.get(name, start);
        if (cached != NOT_FOUND)
            return cached;

        String className = (packageName.length() == 0)
                               ? name : packageName + '.' + name;
        Context cx = Context.getContext();
        ClassShutter shutter = cx.getClassShutter();
        Scriptable newValue = null;
        if (shutter == null || shutter.visibleToScripts(className)) {
            Class cl = null;
            if (classLoader != null) {
                cl = Kit.classOrNull(classLoader, className);
            } else {
                cl = Kit.classOrNull(className);
            }
            if (cl != null) {
                newValue = new NativeJavaClass(getTopLevelScope(this), cl);
                newValue.setPrototype(getPrototype());
            }
        }
        if (newValue == null && createPkg) {
            NativeJavaPackage pkg;
            pkg = new NativeJavaPackage(true, className, classLoader);
            ScriptRuntime.setObjectProtoAndParent(pkg, getParentScope());
            newValue = pkg;
        }
        if (newValue != null) {
            // Make it available for fast lookup and sharing of
            // lazily-reflected constructors and static members.
            super.put(name, start, newValue);
        }
        return newValue;
    }

    public Object getDefaultValue(Class ignored) {
        return toString();
    }

    public String toString() {
        return "[JavaPackage " + packageName + "]";
    }

    public boolean equals(Object obj) {
        if(obj instanceof NativeJavaPackage) {
            NativeJavaPackage njp = (NativeJavaPackage)obj;
            return packageName.equals(njp.packageName) && classLoader == njp.classLoader;
        }
        return false;
    }
    
    public int hashCode() {
        return packageName.hashCode() ^ (classLoader == null ? 0 : classLoader.hashCode());
    }

    private String packageName;
    private ClassLoader classLoader;
}