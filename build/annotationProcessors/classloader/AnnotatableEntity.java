/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors.classloader;

import org.mozilla.gecko.annotationProcessors.AnnotationInfo;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Member;
import java.lang.reflect.Method;

/**
 * Union type to hold either a method, field, or ctor. Allows us to iterate "The generatable stuff", despite
 * the fact that such things can be of either flavour.
 */
public class AnnotatableEntity {
    public enum ENTITY_TYPE {METHOD, FIELD, CONSTRUCTOR}

    private final Member mMember;
    public final ENTITY_TYPE mEntityType;

    public final AnnotationInfo mAnnotationInfo;

    public AnnotatableEntity(Member aObject, AnnotationInfo aAnnotationInfo) {
        mMember = aObject;
        mAnnotationInfo = aAnnotationInfo;

        if (aObject instanceof Method) {
            mEntityType = ENTITY_TYPE.METHOD;
        } else if (aObject instanceof Field) {
            mEntityType = ENTITY_TYPE.FIELD;
        } else {
            mEntityType = ENTITY_TYPE.CONSTRUCTOR;
        }
    }

    public Method getMethod() {
        if (mEntityType != ENTITY_TYPE.METHOD) {
            throw new UnsupportedOperationException("Attempt to cast to unsupported member type.");
        }
        return (Method) mMember;
    }
    public Field getField() {
        if (mEntityType != ENTITY_TYPE.FIELD) {
            throw new UnsupportedOperationException("Attempt to cast to unsupported member type.");
        }
        return (Field) mMember;
    }
    public Constructor<?> getConstructor() {
        if (mEntityType != ENTITY_TYPE.CONSTRUCTOR) {
            throw new UnsupportedOperationException("Attempt to cast to unsupported member type.");
        }
        return (Constructor<?>) mMember;
    }
}
