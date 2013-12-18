/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors.classloader;

import java.lang.annotation.Annotation;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Iterator;

/**
 * Class for iterating over an IterableJarLoadingURLClassLoader's classes.
 */
public class JarClassIterator implements Iterator<ClassWithOptions> {
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
    public ClassWithOptions next() {
        String className = mTargetClassListIterator.next();
        try {
            Class<?> ret = mTarget.loadClass(className);
            final String canonicalName;

            // Incremental builds can leave stale classfiles in the jar. Such classfiles will cause
            // an exception at this point. We can safely ignore these classes - they cannot possibly
            // ever be loaded as they conflict with their parent class and will be killed by Proguard
            // later on anyway.
            try {
                canonicalName = ret.getCanonicalName();
            } catch (IncompatibleClassChangeError e) {
                return next();
            }

            if (canonicalName == null || "null".equals(canonicalName)) {
                // Anonymous inner class - unsupported.
                return next();
            } else {
                String generateName = null;
                for (Annotation annotation : ret.getAnnotations()) {
                    Class<?> annotationType = annotation.annotationType();
                    if (annotationType.getCanonicalName().equals("org.mozilla.gecko.mozglue.generatorannotations.GeneratorOptions")) {
                        try {
                            // Determine the explicitly-given name of the stub to generate, if any.
                            final Method generateNameMethod = annotationType.getDeclaredMethod("generatedClassName");
                            generateNameMethod.setAccessible(true);
                            generateName = (String) generateNameMethod.invoke(annotation);
                            break;
                        } catch (NoSuchMethodException e) {
                            System.err.println("Unable to find expected field on GeneratorOptions annotation. Did the signature change?");
                            e.printStackTrace(System.err);
                            System.exit(3);
                        } catch (IllegalAccessException e) {
                            System.err.println("IllegalAccessException reading fields on GeneratorOptions annotation. Seems the semantics of Reflection have changed...");
                            e.printStackTrace(System.err);
                            System.exit(4);
                        } catch (InvocationTargetException e) {
                            System.err.println("InvocationTargetException reading fields on GeneratorOptions annotation. This really shouldn't happen.");
                            e.printStackTrace(System.err);
                            System.exit(5);
                        }
                    }
                }
                if (generateName == null) {
                    generateName = ret.getSimpleName();
                }
                return new ClassWithOptions(ret, generateName);
            }
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
