/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors.utils;

import org.mozilla.gecko.annotationProcessors.AnnotationInfo;
import org.mozilla.gecko.annotationProcessors.classloader.AnnotatableEntity;

import java.lang.annotation.Annotation;
import java.lang.reflect.AnnotatedElement;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.Iterator;

/**
 * Iterator over the methods in a given method list which have the WrappedJNIMethod
 * annotation. Returns an object containing both the annotation (Which may contain interesting
 * parameters) and the argument.
 */
public class GeneratableElementIterator implements Iterator<AnnotatableEntity> {
    private final Member[] mObjects;
    private AnnotatableEntity mNextReturnValue;
    private int mElementIndex;

    private boolean mIterateEveryEntry;

    public GeneratableElementIterator(Class<?> aClass) {
        // Get all the elements of this class as AccessibleObjects.
        Member[] aMethods = aClass.getDeclaredMethods();
        Member[] aFields = aClass.getDeclaredFields();
        Member[] aCtors = aClass.getConstructors();

        // Shove them all into one buffer.
        Member[] objs = new Member[aMethods.length + aFields.length + aCtors.length];

        int offset = 0;
        System.arraycopy(aMethods, 0, objs, 0, aMethods.length);
        offset += aMethods.length;
        System.arraycopy(aFields, 0, objs, offset, aFields.length);
        offset += aFields.length;
        System.arraycopy(aCtors, 0, objs, offset, aCtors.length);

        // Sort the elements to ensure determinism.
        Arrays.sort(objs, new AlphabeticAnnotatableEntityComparator());
        mObjects = objs;

        // Check for "Wrap ALL the things" flag.
        for (Annotation annotation : aClass.getDeclaredAnnotations()) {
            final String annotationTypeName = annotation.annotationType().getName();
            if (annotationTypeName.equals("org.mozilla.gecko.mozglue.generatorannotations.WrapEntireClassForJNI")) {
                mIterateEveryEntry = true;
                break;
            }
        }

        findNextValue();
    }

    /**
     * Find and cache the next appropriately annotated method, plus the annotation parameter, if
     * one exists. Otherwise cache null, so hasNext returns false.
     */
    private void findNextValue() {
        while (mElementIndex < mObjects.length) {
            Member candidateElement = mObjects[mElementIndex];
            mElementIndex++;
            for (Annotation annotation : ((AnnotatedElement) candidateElement).getDeclaredAnnotations()) {
                // WrappedJNIMethod has parameters. Use Reflection to obtain them.
                Class<? extends Annotation> annotationType = annotation.annotationType();
                final String annotationTypeName = annotationType.getName();
                if (annotationTypeName.equals("org.mozilla.gecko.mozglue.generatorannotations.WrapElementForJNI")) {
                    String stubName = null;
                    boolean isStaticStub = false;
                    boolean isMultithreadedStub = false;
                    boolean noThrow = false;
                    try {
                        // Determine the explicitly-given name of the stub to generate, if any.
                        final Method stubNameMethod = annotationType.getDeclaredMethod("stubName");
                        stubNameMethod.setAccessible(true);
                        stubName = (String) stubNameMethod.invoke(annotation);

                        // Detemine if the generated stub should be static.
                        final Method staticStubMethod = annotationType.getDeclaredMethod("generateStatic");
                        staticStubMethod.setAccessible(true);
                        isStaticStub = (Boolean) staticStubMethod.invoke(annotation);

                        // Determine if the generated stub is to allow calls from multiple threads.
                        final Method multithreadedStubMethod = annotationType.getDeclaredMethod("allowMultithread");
                        multithreadedStubMethod.setAccessible(true);
                        isMultithreadedStub = (Boolean) multithreadedStubMethod.invoke(annotation);

                        // Determine if ignoring exceptions
                        final Method noThrowMethod = annotationType.getDeclaredMethod("noThrow");
                        noThrowMethod.setAccessible(true);
                        noThrow = (Boolean) noThrowMethod.invoke(annotation);

                    } catch (NoSuchMethodException e) {
                        System.err.println("Unable to find expected field on WrapElementForJNI annotation. Did the signature change?");
                        e.printStackTrace(System.err);
                        System.exit(3);
                    } catch (IllegalAccessException e) {
                        System.err.println("IllegalAccessException reading fields on WrapElementForJNI annotation. Seems the semantics of Reflection have changed...");
                        e.printStackTrace(System.err);
                        System.exit(4);
                    } catch (InvocationTargetException e) {
                        System.err.println("InvocationTargetException reading fields on WrapElementForJNI annotation. This really shouldn't happen.");
                        e.printStackTrace(System.err);
                        System.exit(5);
                    }

                    // If the method name was not explicitly given in the annotation generate one...
                    if (stubName.isEmpty()) {
                        String aMethodName = candidateElement.getName();
                        stubName = aMethodName.substring(0, 1).toUpperCase() + aMethodName.substring(1);
                    }

                    AnnotationInfo annotationInfo = new AnnotationInfo(
                        stubName, isStaticStub, isMultithreadedStub, noThrow);
                    mNextReturnValue = new AnnotatableEntity(candidateElement, annotationInfo);
                    return;
                }
            }

            // If no annotation found, we might be expected to generate anyway using default arguments,
            // thanks to the "Generate everything" annotation.
            if (mIterateEveryEntry) {
                AnnotationInfo annotationInfo = new AnnotationInfo(
                    candidateElement.getName(), false, false, false);
                mNextReturnValue = new AnnotatableEntity(candidateElement, annotationInfo);
                return;
            }
        }
        mNextReturnValue = null;
    }

    @Override
    public boolean hasNext() {
        return mNextReturnValue != null;
    }

    @Override
    public AnnotatableEntity next() {
        AnnotatableEntity ret = mNextReturnValue;
        findNextValue();
        return ret;
    }

    @Override
    public void remove() {
        throw new UnsupportedOperationException("Removal of methods from GeneratableElementIterator not supported.");
    }
}
