/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors;

/**
 * Object holding annotation data. Used by GeneratableElementIterator.
 */
public class AnnotationInfo {
    public final String wrapperName;
    public final boolean isMultithreaded;
    public final boolean noThrow;
    public final boolean narrowChars;
    public final boolean catchException;

    public AnnotationInfo(String aWrapperName, boolean aIsMultithreaded,
                          boolean aNoThrow, boolean aNarrowChars, boolean aCatchException) {
        wrapperName = aWrapperName;
        isMultithreaded = aIsMultithreaded;
        noThrow = aNoThrow;
        narrowChars = aNarrowChars;
        catchException = aCatchException;

        if (!noThrow && catchException) {
            // It doesn't make sense to have these together
            throw new IllegalArgumentException("noThrow and catchException are not allowed together");
        }
    }
}
