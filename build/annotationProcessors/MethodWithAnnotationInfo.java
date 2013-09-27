/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors;

import java.lang.reflect.Method;

/**
 * Object holding method and annotation. Used by GeneratableEntryPointIterator.
 */
public class MethodWithAnnotationInfo {
    public final Method method;
    public final String wrapperName;
    public final boolean isStatic;
    public final boolean isMultithreaded;

    public MethodWithAnnotationInfo(Method aMethod, String aWrapperName, boolean aIsStatic, boolean aIsMultithreaded) {
        method = aMethod;
        wrapperName = aWrapperName;
        isStatic = aIsStatic;
        isMultithreaded = aIsMultithreaded;
    }
}
