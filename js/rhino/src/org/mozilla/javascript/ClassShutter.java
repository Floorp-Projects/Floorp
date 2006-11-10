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
 *   Igor Bukanov
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

// API class

package org.mozilla.javascript;

/**
Embeddings that wish to filter Java classes that are visible to scripts
through the LiveConnect, should implement this interface.

@see Context#setClassShutter(ClassShutter)
@since 1.5 Release 4
@author Norris Boyd
*/

 public interface ClassShutter {

    /**
     * Return true iff the Java class with the given name should be exposed
     * to scripts.
     * <p>
     * An embedding may filter which Java classes are exposed through
     * LiveConnect to JavaScript scripts.
     * <p>
     * Due to the fact that there is no package reflection in Java,
     * this method will also be called with package names. There
     * is no way for Rhino to tell if "Packages.a.b" is a package name
     * or a class that doesn't exist. What Rhino does is attempt
     * to load each segment of "Packages.a.b.c": It first attempts to
     * load class "a", then attempts to load class "a.b", then
     * finally attempts to load class "a.b.c". On a Rhino installation
     * without any ClassShutter set, and without any of the
     * above classes, the expression "Packages.a.b.c" will result in
     * a [JavaPackage a.b.c] and not an error.
     * <p>
     * With ClassShutter supplied, Rhino will first call
     * visibleToScripts before attempting to look up the class name. If
     * visibleToScripts returns false, the class name lookup is not
     * performed and subsequent Rhino execution assumes the class is
     * not present. So for "java.lang.System.out.println" the lookup
     * of "java.lang.System" is skipped and thus Rhino assumes that
     * "java.lang.System" doesn't exist. So then for "java.lang.System.out",
     * Rhino attempts to load the class "java.lang.System.out" because
     * it assumes that "java.lang.System" is a package name.
     * <p>
     * @param fullClassName the full name of the class (including the package
     *                      name, with '.' as a delimiter). For example the
     *                      standard string class is "java.lang.String"
     * @return whether or not to reveal this class to scripts
     */
    public boolean visibleToScripts(String fullClassName);
}
