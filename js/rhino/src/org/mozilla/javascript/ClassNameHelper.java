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
 * Norris Boyd
 * Roger Lawrence
 * Andi Vajda
 * Kemal Bayram
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

import java.io.*;

public abstract class ClassNameHelper {

    public static ClassNameHelper get(Context cx) {
        ClassNameHelper helper = savedNameHelper;
        if (helper == null && !helperNotAvailable) {
            Class nameHelperClass = Kit.classOrNull(
                "org.mozilla.javascript.optimizer.OptClassNameHelper");
            // nameHelperClass == null if running lite
            if (nameHelperClass != null) {
                helper = (ClassNameHelper)Kit.newInstanceOrNull(
                                              nameHelperClass);
            }
            if (helper != null) {
                savedNameHelper = helper;
            } else {
                helperNotAvailable = true;
            }
        }
        return helper;
    }

    /**
     * Get the current target class file name.
     * <p>
     * If nonnull, requests to compile source will result in one or
     * more class files being generated.
     *
     * @since 1.5 Release 4
     */
    public abstract String getTargetClassFileName();

    /**
     * Set the current target class file name.
     * <p>
     * If nonnull, requests to compile source will result in one or
     * more class files being generated. If null, classes will only
     * be generated in memory.
     *
     * @since 1.5 Release 4
     */
    public abstract void setTargetClassFileName(String classFileName);

    /**
     * Get the current package to generate classes into.
     */
    public abstract String getTargetPackage();

    /**
     * Set the package to generate classes into.
     */
    public abstract void setTargetPackage(String targetPackage);

    /**
     * Set the class that the generated target will extend.
     *
     * @param extendsClass the class it extends
     */
    public abstract void setTargetExtends(Class extendsClass);

    /**
     * Set the interfaces that the generated target will implement.
     *
     * @param implementsClasses an array of Class objects, one for each
     *                          interface the target will extend
     */
    public abstract void setTargetImplements(Class[] implementsClasses);

    /**
     * Get the current class repository.
     *
     * @see ClassRepository
     * @since 30/10/01 tip + patch (Kemal Bayram)
     */
    public abstract ClassRepository getClassRepository();

    /**
     * Set the current class repository.
     *
     * @see ClassRepository
     * @since 30/10/01 tip + patch (Kemal Bayram)
     */
    public abstract void setClassRepository(ClassRepository repository);

    /**
     * Get the current class name.
     *
     * @since 30/10/01 tip + patch (Kemal Bayram)
     */
    public abstract String getClassName();

    /**
     * Set the current class name.
     *
     * @since 30/10/01 tip + patch (Kemal Bayram)
     */
    public abstract void setClassName(String initialName);

    private static ClassNameHelper savedNameHelper;
    private static boolean helperNotAvailable;
}
