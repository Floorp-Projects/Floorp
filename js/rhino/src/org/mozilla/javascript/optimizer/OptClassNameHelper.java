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

    public synchronized void reset() {
        classNames = null;
    }

    public synchronized String getJavaScriptClassName(String functionName,
                                                      boolean primary)
    {
        StringBuffer s = new StringBuffer();
        if (packageName != null && packageName.length() > 0) {
            s.append(packageName);
            s.append('.');
        }
        s.append(initialName);
        if (appendFunctionName) {
            if (functionName != null) {
                s.append('$');
                if (functionName.length() > 0) {
                    s.append(functionName);
                } else {
                    s.append(++fserial);
                }
            } else if (!primary) {
                s.append(++serial);
            }
        } else {
            synchronized(defaultRepository) {
                s.append(globalSerial++);
            }
        }

        // We wish to produce unique class names between calls to reset()
        // we disregard case since we may write the class names to file
        // systems that are case insensitive
        String result = s.toString();
        String lowerResult = result.toLowerCase();
        String base = lowerResult;
        int count = 0;
        if (classNames == null)
            classNames = new Hashtable();
        while (classNames.get(lowerResult) != null) {
            lowerResult = base + ++count;
        }

        classNames.put(lowerResult, Boolean.TRUE);
        return count == 0 ? result : (result + count);
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
            appendFunctionName = true;
        } else {
            packageName = "org.mozilla.javascript.gen";
            this.initialName = "c";
            classRepository = defaultRepository;
            appendFunctionName = false;
        }
        serial = fserial = 0;
    }

    public ClassRepository getClassRepository() {
        return classRepository;
    }

    public void setClassRepository(ClassRepository classRepository) {
        this.classRepository = classRepository != null ? classRepository : defaultRepository;
    }

    private boolean appendFunctionName;
    private String packageName;
    private String initialName;
    private static int globalSerial=1;
    private int serial, fserial;
    private Class targetExtends;
    private Class[] targetImplements;
    private ClassRepository classRepository;
    private Hashtable classNames;

    static class DefaultRepository implements ClassRepository {
        public boolean storeClass(String name, byte[] bytes, boolean tl) {
            return true;
        }
    }

    private static ClassRepository defaultRepository = new DefaultRepository();
}
