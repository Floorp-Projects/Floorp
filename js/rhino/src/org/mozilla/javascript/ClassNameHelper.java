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
            try {
                Class nameHelperClass = Class.forName(
                    "org.mozilla.javascript.optimizer.OptClassNameHelper");
                helper = (ClassNameHelper)nameHelperClass.newInstance();
            } catch (ClassNotFoundException x) {
                // ...must be running lite, that's ok
            } catch (IllegalAccessException x) {
            } catch (InstantiationException x) {
            }
            if (helper != null) {
                savedNameHelper = helper;
            } else {
                helperNotAvailable = true;
            }
        }
        return helper;
    }

    static void clearCache() {
        ClassNameHelper helper = savedNameHelper;
        if (helper != null) {
            helper.reset();
        }
    }

    /**
     * Get the current target class file name.
     * <p>
     * If nonnull, requests to compile source will result in one or
     * more class files being generated.
     *
     * @since 1.5 Release 4
     */
    public String getTargetClassFileName() {
        ClassRepository repository = getClassRepository();
        if (repository instanceof FileClassRepository) {
            return ((FileClassRepository)repository).
                getTargetClassFileName(getClassName());
        }
        return null;
    }

    /**
     * Set the current target class file name.
     * <p>
     * If nonnull, requests to compile source will result in one or
     * more class files being generated. If null, classes will only
     * be generated in memory.
     *
     * @since 1.5 Release 4
     */
    public void setTargetClassFileName(String classFileName) {
        if (classFileName != null) {
            setClassRepository(new FileClassRepository(classFileName));
        } else {
            setClassName(null);
        }
    }

    /**
     * @deprecated Application should use {@link ClassRepository} instead of
     * {@link ClassOutput}.
     *
     * @see #getClassRepository
     */
    public final ClassOutput getClassOutput() {
        ClassRepository repository = getClassRepository();
        if (repository instanceof ClassOutputWrapper) {
            return ((ClassOutputWrapper)repository).classOutput;
        }
        return null;
    }

    /**
     * @deprecated Application should use {@link ClassRepository} instead of
     * {@link ClassOutput}.
     *
     * @see #setClassRepository
     */
    public void setClassOutput(ClassOutput classOutput) {
        if (classOutput != null) {
            setClassRepository(new ClassOutputWrapper(classOutput));
        } else {
            setClassRepository(null);
        }
    }

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

    public abstract void reset();

    // Implement class file saving here instead of inside codegen.
    private class FileClassRepository implements ClassRepository {

        FileClassRepository(String classFileName) {
            int lastSeparator = classFileName.lastIndexOf(File.separatorChar);
            String initialName;
            if (lastSeparator == -1) {
                generatingDirectory = null;
                initialName = classFileName;
            } else {
                generatingDirectory = classFileName.substring(0, lastSeparator);
                initialName = classFileName.substring(lastSeparator+1);
            }
            if (initialName.endsWith(".class"))
                initialName = initialName.substring(0, initialName.length()-6);
            setClassName(initialName);
        }

        public boolean storeClass(String className, byte[] bytes, boolean tl)
            throws IOException
        {
            // no "elegant" way of getting file name from fully
            // qualified class name.
            String targetPackage = getTargetPackage();
            if ((targetPackage != null) && (targetPackage.length()>0) &&
                className.startsWith(targetPackage+"."))
            {
                className = className.substring(targetPackage.length()+1);
            }

            FileOutputStream out = new FileOutputStream(getTargetClassFileName(className));
            out.write(bytes);
            out.close();

            return false;
        }

        String getTargetClassFileName(String className) {
            StringBuffer sb = new StringBuffer();
            if (generatingDirectory != null) {
                sb.append(generatingDirectory);
                sb.append(File.separator);
            }
            sb.append(className);
            sb.append(".class");
            return sb.toString();
        }

        String generatingDirectory;
    };

    private static class ClassOutputWrapper implements ClassRepository {

        ClassOutputWrapper(ClassOutput classOutput) {
            this.classOutput = classOutput;
        }

        public boolean storeClass(String name, byte[] bytes, boolean tl)
            throws IOException
        {
            OutputStream out = classOutput.getOutputStream(name, tl);
            out.write(bytes);
            out.close();

            return true;
        }

        ClassOutput classOutput;
    }

    private static ClassNameHelper savedNameHelper;
    private static boolean helperNotAvailable;
}
