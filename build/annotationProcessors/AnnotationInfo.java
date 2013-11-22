/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors;

/**
 * Object holding annotation data. Used by GeneratableElementIterator.
 */
public class AnnotationInfo {
    public final String wrapperName;
    public final boolean isStatic;
    public final boolean isMultithreaded;

    public AnnotationInfo(String aWrapperName, boolean aIsStatic, boolean aIsMultithreaded) {
        wrapperName = aWrapperName;
        isStatic = aIsStatic;
        isMultithreaded = aIsMultithreaded;
    }
}
