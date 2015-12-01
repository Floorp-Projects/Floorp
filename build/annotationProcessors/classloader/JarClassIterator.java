/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors.classloader;

import java.util.Iterator;

/**
 * Class for iterating over an IterableJarLoadingURLClassLoader's classes.
 *
 * This class is not thread safe: use it only from a single thread.
 */
public class JarClassIterator implements Iterator<ClassWithOptions> {
    private IterableJarLoadingURLClassLoader mTarget;
    private Iterator<String> mTargetClassListIterator;

    private ClassWithOptions lookAhead;

    public JarClassIterator(IterableJarLoadingURLClassLoader aTarget) {
        mTarget = aTarget;
        mTargetClassListIterator = aTarget.classNames.iterator();
    }

    @Override
    public boolean hasNext() {
        return fillLookAheadIfPossible();
    }

    @Override
    public ClassWithOptions next() {
        if (!fillLookAheadIfPossible()) {
            throw new IllegalStateException("Failed to look ahead in next()!");
        }
        ClassWithOptions next = lookAhead;
        lookAhead = null;
        return next;
    }

    private boolean fillLookAheadIfPossible() {
        if (lookAhead != null) {
            return true;
        }

        if (!mTargetClassListIterator.hasNext()) {
            return false;
        }

        String className = mTargetClassListIterator.next();
        try {
            Class<?> ret = mTarget.loadClass(className);

            // Incremental builds can leave stale classfiles in the jar. Such classfiles will cause
            // an exception at this point. We can safely ignore these classes - they cannot possibly
            // ever be loaded as they conflict with their parent class and will be killed by
            // Proguard later on anyway.
            final Class<?> enclosingClass;
            try {
                enclosingClass = ret.getEnclosingClass();
            } catch (IncompatibleClassChangeError e) {
                return fillLookAheadIfPossible();
            }

            if (enclosingClass != null) {
                // Anonymous inner class - unsupported.
                // Or named inner class, which will be processed when we process the outer class.
                return fillLookAheadIfPossible();
            }

            lookAhead = new ClassWithOptions(ret, ret.getSimpleName());
            return true;
        } catch (ClassNotFoundException e) {
            System.err.println("Unable to enumerate class: " + className + ". Corrupted jar file?");
            e.printStackTrace();
            System.exit(2);
        }
        return false;
    }

    @Override
    public void remove() {
        throw new UnsupportedOperationException("Removal of classes from iterator not supported.");
    }
}
