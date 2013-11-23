/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors.classloader;

public class ClassWithOptions {
    public final Class<?> wrappedClass;
    public final String generatedName;

    public ClassWithOptions(Class<?> someClass, String name) {
        wrappedClass = someClass;
        generatedName = name;
    }
}
