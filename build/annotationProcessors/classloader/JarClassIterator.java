/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors.classloader;

import java.util.Iterator;

/**
 * Class for iterating over an IterableJarLoadingURLClassLoader's classes.
 */
public class JarClassIterator implements Iterator<Class<?>> {
    private IterableJarLoadingURLClassLoader mTarget;
    private Iterator<String> mTargetClassListIterator;

    public JarClassIterator(IterableJarLoadingURLClassLoader aTarget) {
        mTarget = aTarget;
        mTargetClassListIterator = aTarget.classNames.iterator();
    }

    @Override
    public boolean hasNext() {
        return mTargetClassListIterator.hasNext();
    }

    @Override
    public Class<?> next() {
        String className = mTargetClassListIterator.next();
        try {
            return mTarget.loadClass(className);
        } catch (ClassNotFoundException e) {
            System.err.println("Unable to enumerate class: " + className + ". Corrupted jar file?");
            e.printStackTrace();
            System.exit(2);
        }
        return null;
    }

    @Override
    public void remove() {
        throw new UnsupportedOperationException("Removal of classes from iterator not supported.");
    }
}
