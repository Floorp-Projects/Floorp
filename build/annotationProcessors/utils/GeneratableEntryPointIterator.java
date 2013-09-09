/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors.utils;

import org.mozilla.gecko.annotationProcessors.MethodWithAnnotationInfo;

import java.lang.annotation.Annotation;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;

/**
 * Iterator over the methods in a given method list which have the GeneratableAndroidBridgeTarget
 * annotation. Returns an object containing both the annotation (Which may contain interesting
 * parameters) and the argument.
 */
public class GeneratableEntryPointIterator implements Iterator<MethodWithAnnotationInfo> {
    private final Method[] mMethods;
    private MethodWithAnnotationInfo mNextReturnValue;
    private int mMethodIndex;

    public GeneratableEntryPointIterator(Method[] aMethods) {
        // Sort the methods into alphabetical order by name, to ensure we always iterate methods
        // in the same order..
        Arrays.sort(aMethods, new AlphabeticMethodComparator());
        mMethods = aMethods;

        findNextValue();
    }

    /**
     * Find and cache the next appropriately annotated method, plus the annotation parameter, if
     * one exists. Otherwise cache null, so hasNext returns false.
     */
    private void findNextValue() {
        while (mMethodIndex < mMethods.length) {
            Method candidateMethod = mMethods[mMethodIndex];
            mMethodIndex++;
            for (Annotation annotation : candidateMethod.getDeclaredAnnotations()) {
                // GeneratableAndroidBridgeTarget has a parameter. Use Reflection to obtain it.
                Class<? extends Annotation> annotationType = annotation.annotationType();
                final String annotationTypeName = annotationType.getName();

                if (annotationTypeName.equals("org.mozilla.gecko.mozglue.GeneratableAndroidBridgeTarget")) {
                    String stubName = null;
                    boolean isStaticStub = false;
                    boolean isMultithreadedStub = false;
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
                    } catch (NoSuchMethodException e) {
                        System.err.println("Unable to find expected field on GeneratableAndroidBridgeTarget annotation. Did the signature change?");
                        e.printStackTrace(System.err);
                        System.exit(3);
                    } catch (IllegalAccessException e) {
                        System.err.println("IllegalAccessException reading fields on GeneratableAndroidBridgeTarget annotation. Seems the semantics of Reflection have changed...");
                        e.printStackTrace(System.err);
                        System.exit(4);
                    } catch (InvocationTargetException e) {
                        System.err.println("InvocationTargetException reading fields on GeneratableAndroidBridgeTarget annotation. This really shouldn't happen.");
                        e.printStackTrace(System.err);
                        System.exit(5);
                    }
                    // If the method name was not explicitly given in the annotation generate one...
                    if (stubName.isEmpty()) {
                        String aMethodName = candidateMethod.getName();
                        stubName = aMethodName.substring(0, 1).toUpperCase() + aMethodName.substring(1);
                    }

                    mNextReturnValue = new MethodWithAnnotationInfo(candidateMethod, stubName, isStaticStub, isMultithreadedStub);
                    return;
                }
            }
        }
        mNextReturnValue = null;
    }

    @Override
    public boolean hasNext() {
        return mNextReturnValue != null;
    }

    @Override
    public MethodWithAnnotationInfo next() {
        MethodWithAnnotationInfo ret = mNextReturnValue;
        findNextValue();
        return ret;
    }

    @Override
    public void remove() {
        throw new UnsupportedOperationException("Removal of methods from GeneratableEntryPointIterator not supported.");
    }
}
