/*
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


package org.mozilla.javascript.optimizer;

import org.mozilla.javascript.*;
import java.io.*;
import java.util.Hashtable;

public class OptClassNameHelper extends ClassNameHelper {

    public OptClassNameHelper() {
        setClassName(null);
    }

    synchronized String getScriptClassName(boolean primary)
    {
        StringBuffer s = new StringBuffer();
        if (packageName != null && packageName.length() > 0) {
            s.append(packageName);
            s.append('.');
        }
        s.append(initialName);
        if (explicitInitialName) {
            if (!primary) {
                s.append(++serial);
            }
        } else {
            s.append(globalSerial++);
        }

        return s.toString();
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
            setClassRepository(new FileClassRepository(this, classFileName));
        } else {
            setClassName(null);
        }
    }

    public String getTargetPackage() {
        return packageName;
    }

    public void setTargetPackage(String targetPackage) {
        this.packageName = targetPackage;
    }

    public Class getTargetExtends() {
        return targetExtends;
    }

    public void setTargetExtends(Class extendsClass) {
        targetExtends = extendsClass;
    }

    public Class[] getTargetImplements() {
        return targetImplements;
    }

    public void setTargetImplements(Class[] implementsClasses) {
        targetImplements = implementsClasses;
    }

    public String getClassName() {
        return initialName;
    }

    public void setClassName(String initialName) {
        if (initialName != null) {
            this.initialName = initialName;
            explicitInitialName = true;
        } else {
            packageName = "org.mozilla.javascript.gen";
            this.initialName = "c";
            classRepository = null;
            explicitInitialName = false;
        }
        serial = 0;
    }

    public ClassRepository getClassRepository() {
        return classRepository;
    }

    public void setClassRepository(ClassRepository classRepository) {
        this.classRepository = classRepository;
    }

    private boolean explicitInitialName;
    private String packageName;
    private String initialName;
    private int globalSerial=1;
    private int serial;
    private Class targetExtends;
    private Class[] targetImplements;
    private ClassRepository classRepository;
}

// Implement class file saving here instead of inside codegen.
class FileClassRepository implements ClassRepository
{
    FileClassRepository(OptClassNameHelper nameHelper, String classFileName) {
        this.nameHelper = nameHelper;
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
        nameHelper.setClassName(initialName);
    }

    public boolean storeClass(String className, byte[] bytes, boolean tl)
        throws IOException
    {
        // no "elegant" way of getting file name from fully
        // qualified class name.
        String targetPackage = nameHelper.getTargetPackage();
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

    OptClassNameHelper nameHelper;
    String generatingDirectory;
}

