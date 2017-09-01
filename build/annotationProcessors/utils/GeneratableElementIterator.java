/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors.utils;

import org.mozilla.gecko.annotationProcessors.AnnotationInfo;
import org.mozilla.gecko.annotationProcessors.classloader.AnnotatableEntity;
import org.mozilla.gecko.annotationProcessors.classloader.ClassWithOptions;

import java.lang.annotation.Annotation;
import java.lang.reflect.AnnotatedElement;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Iterator;

/**
 * Iterator over the methods in a given method list which have the WrappedJNIMethod
 * annotation. Returns an object containing both the annotation (Which may contain interesting
 * parameters) and the argument.
 */
public class GeneratableElementIterator implements Iterator<AnnotatableEntity> {
    private final ClassWithOptions mClass;
    private final Member[] mObjects;
    private AnnotatableEntity mNextReturnValue;
    private int mElementIndex;
    private AnnotationInfo mClassInfo;

    private boolean mIterateEveryEntry;
    private boolean mSkipCurrentEntry;

    public GeneratableElementIterator(ClassWithOptions annotatedClass) {
        mClass = annotatedClass;

        final Class<?> aClass = annotatedClass.wrappedClass;
        // Get all the elements of this class as AccessibleObjects.
        Member[] aMethods = aClass.getDeclaredMethods();
        Member[] aFields = aClass.getDeclaredFields();
        Member[] aCtors = aClass.getDeclaredConstructors();

        // Shove them all into one buffer.
        Member[] objs = new Member[aMethods.length + aFields.length + aCtors.length];

        int offset = 0;
        System.arraycopy(aMethods, 0, objs, 0, aMethods.length);
        offset += aMethods.length;
        System.arraycopy(aFields, 0, objs, offset, aFields.length);
        offset += aFields.length;
        System.arraycopy(aCtors, 0, objs, offset, aCtors.length);

        // Sort the elements to ensure determinism.
        Arrays.sort(objs, new AlphabeticAnnotatableEntityComparator<Member>());
        mObjects = objs;

        // Check for "Wrap ALL the things" flag.
        for (Annotation annotation : aClass.getDeclaredAnnotations()) {
            mClassInfo = buildAnnotationInfo(aClass, annotation);
            if (mClassInfo != null) {
                mIterateEveryEntry = true;
                break;
            }
        }

        if (mSkipCurrentEntry) {
            throw new IllegalArgumentException("Cannot skip entire class");
        }

        findNextValue();
    }

    private Class<?>[] getFilteredInnerClasses() {
        // Go through all inner classes and see which ones we want to generate.
        final Class<?>[] candidates = mClass.wrappedClass.getDeclaredClasses();
        int count = 0;

        for (int i = 0; i < candidates.length; ++i) {
            final GeneratableElementIterator testIterator = new GeneratableElementIterator(
                    new ClassWithOptions(candidates[i], null, /* ifdef */ ""));
            if (testIterator.hasNext()
                    || testIterator.getFilteredInnerClasses() != null) {
                count++;
                continue;
            }
            // Clear out ones that don't match.
            candidates[i] = null;
        }
        return count > 0 ? candidates : null;
    }

    public ClassWithOptions[] getInnerClasses() {
        final Class<?>[] candidates = getFilteredInnerClasses();
        if (candidates == null) {
            return new ClassWithOptions[0];
        }

        int count = 0;
        for (Class<?> candidate : candidates) {
            if (candidate != null) {
                count++;
            }
        }

        final ClassWithOptions[] ret = new ClassWithOptions[count];
        count = 0;
        for (Class<?> candidate : candidates) {
            if (candidate != null) {
                ret[count++] = new ClassWithOptions(
                        candidate,
                        mClass.generatedName + "::" + candidate.getSimpleName(),
                        /* ifdef */ "");
            }
        }
        assert ret.length == count;

        Arrays.sort(ret, new Comparator<ClassWithOptions>() {
            @Override public int compare(ClassWithOptions lhs, ClassWithOptions rhs) {
                return lhs.generatedName.compareTo(rhs.generatedName);
            }
        });
        return ret;
    }

    private AnnotationInfo buildAnnotationInfo(AnnotatedElement element, Annotation annotation) {
        Class<? extends Annotation> annotationType = annotation.annotationType();
        final String annotationTypeName = annotationType.getName();
        if (!annotationTypeName.equals("org.mozilla.gecko.annotation.WrapForJNI")) {
            return null;
        }

        String stubName = null;
        AnnotationInfo.ExceptionMode exceptionMode = null;
        AnnotationInfo.CallingThread callingThread = null;
        AnnotationInfo.DispatchTarget dispatchTarget = null;
        boolean noLiteral = false;

        try {
            final Method skipMethod = annotationType.getDeclaredMethod("skip");
            skipMethod.setAccessible(true);
            if ((Boolean) skipMethod.invoke(annotation)) {
                mSkipCurrentEntry = true;
                return null;
            }

            // Determine the explicitly-given name of the stub to generate, if any.
            final Method stubNameMethod = annotationType.getDeclaredMethod("stubName");
            stubNameMethod.setAccessible(true);
            stubName = (String) stubNameMethod.invoke(annotation);

            final Method exceptionModeMethod = annotationType.getDeclaredMethod("exceptionMode");
            exceptionModeMethod.setAccessible(true);
            exceptionMode = Utils.getEnumValue(
                    AnnotationInfo.ExceptionMode.class,
                    (String) exceptionModeMethod.invoke(annotation));

            final Method calledFromMethod = annotationType.getDeclaredMethod("calledFrom");
            calledFromMethod.setAccessible(true);
            callingThread = Utils.getEnumValue(
                    AnnotationInfo.CallingThread.class,
                    (String) calledFromMethod.invoke(annotation));

            final Method dispatchToMethod = annotationType.getDeclaredMethod("dispatchTo");
            dispatchToMethod.setAccessible(true);
            dispatchTarget = Utils.getEnumValue(
                    AnnotationInfo.DispatchTarget.class,
                    (String) dispatchToMethod.invoke(annotation));

            final Method noLiteralMethod = annotationType.getDeclaredMethod("noLiteral");
            noLiteralMethod.setAccessible(true);
            noLiteral = (Boolean) noLiteralMethod.invoke(annotation);

        } catch (NoSuchMethodException e) {
            System.err.println("Unable to find expected field on WrapForJNI annotation. Did the signature change?");
            e.printStackTrace(System.err);
            System.exit(3);
        } catch (IllegalAccessException e) {
            System.err.println("IllegalAccessException reading fields on WrapForJNI annotation. Seems the semantics of Reflection have changed...");
            e.printStackTrace(System.err);
            System.exit(4);
        } catch (InvocationTargetException e) {
            System.err.println("InvocationTargetException reading fields on WrapForJNI annotation. This really shouldn't happen.");
            e.printStackTrace(System.err);
            System.exit(5);
        }

        // If the method name was not explicitly given in the annotation generate one...
        if (stubName.isEmpty()) {
            stubName = Utils.getNativeName(element);
        }

        return new AnnotationInfo(stubName, exceptionMode, callingThread, dispatchTarget,
                                  noLiteral);
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
                AnnotationInfo info = buildAnnotationInfo((AnnotatedElement)candidateElement, annotation);
                if (info != null) {
                    mNextReturnValue = new AnnotatableEntity(candidateElement, info);
                    return;
                }
            }

            if (mSkipCurrentEntry) {
                mSkipCurrentEntry = false;
                continue;
            }

            // If no annotation found, we might be expected to generate anyway
            // using default arguments, thanks to the "Generate everything" annotation.
            if (mIterateEveryEntry) {
                AnnotationInfo annotationInfo = new AnnotationInfo(
                    Utils.getNativeName(candidateElement),
                    mClassInfo.exceptionMode,
                    mClassInfo.callingThread,
                    mClassInfo.dispatchTarget,
                    mClassInfo.noLiteral);
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
