/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors.utils;

import org.mozilla.gecko.annotationProcessors.AnnotationInfo;

import java.lang.reflect.AnnotatedElement;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.nio.ByteBuffer;
import java.util.HashMap;

/**
 * A collection of utility methods used by CodeGenerator. Largely used for translating types.
 */
public class Utils {

    // A collection of lookup tables to simplify the functions to follow...
    private static final HashMap<String, String> NATIVE_TYPES = new HashMap<String, String>();

    static {
        NATIVE_TYPES.put("void", "void");
        NATIVE_TYPES.put("boolean", "bool");
        NATIVE_TYPES.put("byte", "int8_t");
        NATIVE_TYPES.put("char", "char16_t");
        NATIVE_TYPES.put("short", "int16_t");
        NATIVE_TYPES.put("int", "int32_t");
        NATIVE_TYPES.put("long", "int64_t");
        NATIVE_TYPES.put("float", "float");
        NATIVE_TYPES.put("double", "double");
    }

    private static final HashMap<String, String> NATIVE_ARRAY_TYPES = new HashMap<String, String>();

    static {
        NATIVE_ARRAY_TYPES.put("boolean", "mozilla::jni::BooleanArray");
        NATIVE_ARRAY_TYPES.put("byte", "mozilla::jni::ByteArray");
        NATIVE_ARRAY_TYPES.put("char", "mozilla::jni::CharArray");
        NATIVE_ARRAY_TYPES.put("short", "mozilla::jni::ShortArray");
        NATIVE_ARRAY_TYPES.put("int", "mozilla::jni::IntArray");
        NATIVE_ARRAY_TYPES.put("long", "mozilla::jni::LongArray");
        NATIVE_ARRAY_TYPES.put("float", "mozilla::jni::FloatArray");
        NATIVE_ARRAY_TYPES.put("double", "mozilla::jni::DoubleArray");
    }

    private static final HashMap<String, String> CLASS_DESCRIPTORS = new HashMap<String, String>();

    static {
        CLASS_DESCRIPTORS.put("void", "V");
        CLASS_DESCRIPTORS.put("boolean", "Z");
        CLASS_DESCRIPTORS.put("byte", "B");
        CLASS_DESCRIPTORS.put("char", "C");
        CLASS_DESCRIPTORS.put("short", "S");
        CLASS_DESCRIPTORS.put("int", "I");
        CLASS_DESCRIPTORS.put("long", "J");
        CLASS_DESCRIPTORS.put("float", "F");
        CLASS_DESCRIPTORS.put("double", "D");
    }

    /**
     * Get the C++ type corresponding to the provided type parameter.
     *
     * @param type Class to determine the corresponding JNI type for.
     * @return C++ type as a String
     */
    public static String getNativeParameterType(Class<?> type, AnnotationInfo info) {
        final String name = type.getName().replace('.', '/');

        if (NATIVE_TYPES.containsKey(name)) {
            return NATIVE_TYPES.get(name);
        }

        if (type.isArray()) {
            final String compName = type.getComponentType().getName();
            if (NATIVE_ARRAY_TYPES.containsKey(compName)) {
                return NATIVE_ARRAY_TYPES.get(compName) + "::Param";
            }
            return "mozilla::jni::ObjectArray::Param";
        }

        if (type.equals(String.class) || type.equals(CharSequence.class)) {
            return "mozilla::jni::String::Param";
        }

        if (type.equals(Class.class)) {
            // You're doing reflection on Java objects from inside C, returning Class objects
            // to C, generating the corresponding code using this Java program. Really?!
            return "mozilla::jni::Class::Param";
        }

        if (type.equals(Throwable.class)) {
            return "mozilla::jni::Throwable::Param";
        }

        if (type.equals(ByteBuffer.class)) {
            return "mozilla::jni::ByteBuffer::Param";
        }

        return "mozilla::jni::Object::Param";
    }

    public static String getNativeReturnType(Class<?> type, AnnotationInfo info) {
        final String name = type.getName().replace('.', '/');

        if (NATIVE_TYPES.containsKey(name)) {
            return NATIVE_TYPES.get(name);
        }

        if (type.isArray()) {
            final String compName = type.getComponentType().getName();
            if (NATIVE_ARRAY_TYPES.containsKey(compName)) {
                return NATIVE_ARRAY_TYPES.get(compName) + "::LocalRef";
            }
            return "mozilla::jni::ObjectArray::LocalRef";
        }

        if (type.equals(String.class)) {
            return "mozilla::jni::String::LocalRef";
        }

        if (type.equals(Class.class)) {
            // You're doing reflection on Java objects from inside C, returning Class objects
            // to C, generating the corresponding code using this Java program. Really?!
            return "mozilla::jni::Class::LocalRef";
        }

        if (type.equals(Throwable.class)) {
            return "mozilla::jni::Throwable::LocalRef";
        }

        if (type.equals(ByteBuffer.class)) {
            return "mozilla::jni::ByteBuffer::LocalRef";
        }

        return "mozilla::jni::Object::LocalRef";
    }

    /**
     * Get the JNI class descriptor corresponding to the provided type parameter.
     *
     * @param type Class to determine the corresponding JNI descriptor for.
     * @return Class descripor as a String
     */
    public static String getClassDescriptor(Class<?> type) {
        final String name = type.getName().replace('.', '/');

        if (CLASS_DESCRIPTORS.containsKey(name)) {
            return CLASS_DESCRIPTORS.get(name);
        }

        if (type.isArray()) {
            // Array names are already in class descriptor form.
            return name;
        }

        return "L" + name + ';';
    }

    /**
     * Get the JNI signaure for a member.
     *
     * @param member Member to get the signature for.
     * @return JNI signature as a string
     */
    public static String getSignature(Member member) {
        return member instanceof Field ?  getSignature((Field) member) :
               member instanceof Method ? getSignature((Method) member) :
                                          getSignature((Constructor<?>) member);
    }

    /**
     * Get the JNI signaure for a field.
     *
     * @param member Field to get the signature for.
     * @return JNI signature as a string
     */
    public static String getSignature(Field member) {
        return getClassDescriptor(member.getType());
    }

    private static String getSignature(Class<?>[] args, Class<?> ret) {
        final StringBuilder sig = new StringBuilder("(");
        for (int i = 0; i < args.length; i++) {
            sig.append(getClassDescriptor(args[i]));
        }
        return sig.append(')').append(getClassDescriptor(ret)).toString();
    }

    /**
     * Get the JNI signaure for a method.
     *
     * @param member Method to get the signature for.
     * @return JNI signature as a string
     */
    public static String getSignature(Method member) {
        return getSignature(member.getParameterTypes(), member.getReturnType());
    }

    /**
     * Get the JNI signaure for a constructor.
     *
     * @param member Constructor to get the signature for.
     * @return JNI signature as a string
     */
    public static String getSignature(Constructor<?> member) {
        return getSignature(member.getParameterTypes(), void.class);
    }

    /**
     * Get the C++ name for a member.
     *
     * @param member Member to get the name for.
     * @return JNI name as a string
     */
    public static String getNativeName(Member member) {
        final String name = getMemberName(member);
        return name.substring(0, 1).toUpperCase() + name.substring(1);
    }

    /**
     * Get the C++ name for a member.
     *
     * @param member Member to get the name for.
     * @return JNI name as a string
     */
    public static String getNativeName(Class<?> clz) {
        final String name = clz.getName();
        return name.substring(0, 1).toUpperCase() + name.substring(1);
    }

    /**
     * Get the C++ name for a member.
     *
     * @param member Member to get the name for.
     * @return JNI name as a string
     */
    public static String getNativeName(AnnotatedElement element) {
        if (element instanceof Class<?>) {
            return getNativeName((Class<?>)element);
        } else if (element instanceof Member) {
            return getNativeName((Member)element);
        } else {
            return null;
        }
    }

    /**
     * Get the JNI name for a member.
     *
     * @param member Member to get the name for.
     * @return JNI name as a string
     */
    public static String getMemberName(Member member) {
        if (member instanceof Constructor) {
            return "<init>";
        }
        return member.getName();
    }

    public static String getUnqualifiedName(String name) {
        return name.substring(name.lastIndexOf(':') + 1);
    }

    /**
     * Determine if a member is declared static.
     *
     * @param member The Member to check.
     * @return true if the member is declared static, false otherwise.
     */
    public static boolean isStatic(final Member member) {
        return Modifier.isStatic(member.getModifiers());
    }

    /**
     * Determine if a member is declared final.
     *
     * @param member The Member to check.
     * @return true if the member is declared final, false otherwise.
     */
    public static boolean isFinal(final Member member) {
        return Modifier.isFinal(member.getModifiers());
    }
}
