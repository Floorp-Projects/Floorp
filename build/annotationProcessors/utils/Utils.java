/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors.utils;

import java.lang.annotation.Annotation;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.HashMap;

/**
 * A collection of utility methods used by CodeGenerator. Largely used for translating types.
 */
public class Utils {

    // A collection of lookup tables to simplify the functions to follow...
    private static final HashMap<String, String> sBasicCTypes = new HashMap<String, String>();

    static {
        sBasicCTypes.put("void", "void");
        sBasicCTypes.put("int", "int32_t");
        sBasicCTypes.put("boolean", "bool");
        sBasicCTypes.put("long", "int64_t");
        sBasicCTypes.put("double", "jdouble");
        sBasicCTypes.put("float", "jfloat");
        sBasicCTypes.put("char", "uint16_t");
        sBasicCTypes.put("byte", "int8_t");
        sBasicCTypes.put("short", "int16_t");
    }

    private static final HashMap<String, String> sArrayCTypes = new HashMap<String, String>();

    static {
        sArrayCTypes.put("int", "jintArray");
        sArrayCTypes.put("boolean", "jbooleanArray");
        sArrayCTypes.put("long", "jlongArray");
        sArrayCTypes.put("double", "jdoubleArray");
        sArrayCTypes.put("float", "jfloatArray");
        sArrayCTypes.put("char", "jcharArray");
        sArrayCTypes.put("byte", "jbyteArray");
        sArrayCTypes.put("short", "jshortArray");
    }

    private static final HashMap<String, String> sStaticCallTypes = new HashMap<String, String>();

    static {
        sStaticCallTypes.put("void", "CallStaticVoidMethod");
        sStaticCallTypes.put("int", "CallStaticIntMethod");
        sStaticCallTypes.put("boolean", "CallStaticBooleanMethod");
        sStaticCallTypes.put("long", "CallStaticLongMethod");
        sStaticCallTypes.put("double", "CallStaticDoubleMethod");
        sStaticCallTypes.put("float", "CallStaticFloatMethod");
        sStaticCallTypes.put("char", "CallStaticCharMethod");
        sStaticCallTypes.put("byte", "CallStaticByteMethod");
        sStaticCallTypes.put("short", "CallStaticShortMethod");
    }

    private static final HashMap<String, String> sInstanceCallTypes = new HashMap<String, String>();

    static {
        sInstanceCallTypes.put("void", "CallVoidMethod");
        sInstanceCallTypes.put("int", "CallIntMethod");
        sInstanceCallTypes.put("boolean", "CallBooleanMethod");
        sInstanceCallTypes.put("long", "CallLongMethod");
        sInstanceCallTypes.put("double", "CallDoubleMethod");
        sInstanceCallTypes.put("float", "CallFloatMethod");
        sInstanceCallTypes.put("char", "CallCharMethod");
        sInstanceCallTypes.put("byte", "CallByteMethod");
        sInstanceCallTypes.put("short", "CallShortMethod");
    }

    private static final HashMap<String, String> sFieldTypes = new HashMap<String, String>();

    static {
        sFieldTypes.put("int", "Int");
        sFieldTypes.put("boolean", "Boolean");
        sFieldTypes.put("long", "Long");
        sFieldTypes.put("double", "Double");
        sFieldTypes.put("float", "Float");
        sFieldTypes.put("char", "Char");
        sFieldTypes.put("byte", "Byte");
        sFieldTypes.put("short", "Short");
    }

    private static final HashMap<String, String> sFailureReturns = new HashMap<String, String>();

    static {
        sFailureReturns.put("java.lang.Void", "");
        sFailureReturns.put("void", "");
        sFailureReturns.put("int", " 0");
        sFailureReturns.put("boolean", " false");
        sFailureReturns.put("long", " 0");
        sFailureReturns.put("double", " 0.0");
        sFailureReturns.put("float", " 0.0");
        sFailureReturns.put("char", " 0");
        sFailureReturns.put("byte", " 0");
        sFailureReturns.put("short", " 0");
    }

    private static final HashMap<String, String> sCanonicalSignatureParts = new HashMap<String, String>();

    static {
        sCanonicalSignatureParts.put("java/lang/Void", "V");
        sCanonicalSignatureParts.put("void", "V");
        sCanonicalSignatureParts.put("int", "I");
        sCanonicalSignatureParts.put("boolean", "Z");
        sCanonicalSignatureParts.put("long", "J");
        sCanonicalSignatureParts.put("double", "D");
        sCanonicalSignatureParts.put("float", "F");
        sCanonicalSignatureParts.put("char", "C");
        sCanonicalSignatureParts.put("byte", "B");
        sCanonicalSignatureParts.put("short", "S");
    }


    private static final HashMap<String, String> sDefaultParameterValues = new HashMap<String, String>();

    static {
        sDefaultParameterValues.put("int", "0");
        sDefaultParameterValues.put("boolean", "false");
        sDefaultParameterValues.put("long", "0");
        sDefaultParameterValues.put("double", "0");
        sDefaultParameterValues.put("float", "0.0");
        sDefaultParameterValues.put("char", "0");
        sDefaultParameterValues.put("byte", "0");
        sDefaultParameterValues.put("short", "0");
    }

    /**
     * Get the C type corresponding to the provided type parameter. Used for generating argument
     * types for the wrapper method.
     *
     * @param type Class to determine the corresponding JNI type for.
     * @return true if the type an object type, false otherwise.
     */
    public static String getCParameterType(Class<?> type, boolean aNarrowChars) {
        String name = type.getCanonicalName();
        if (sBasicCTypes.containsKey(name)) {
            return sBasicCTypes.get(name);
        }
        // Are we dealing with an array type?
        int len = name.length();
        if (name.endsWith("[]")) {
            // Determine if it is a 2D array - these map to jobjectArrays
            name = name.substring(0, len - 2);
            if (name.endsWith("[]")) {
                return "jobjectArray";
            } else {
                // Which flavour of Array is it?
                if (sArrayCTypes.containsKey(name)) {
                    return sArrayCTypes.get(name);
                }
                return "jobjectArray";
            }
        }
        // Not an array type, check the remaining possibilities before we fall back to jobject

        // Check for CharSequences (Strings and things that are string-like)
        if (isCharSequence(type)) {
	    if (aNarrowChars) {
		return "const nsACString&";
	    }
            return "const nsAString&";
        }

        if (name.equals("java.lang.Class")) {
            // You're doing reflection on Java objects from inside C, returning Class objects
            // to C, generating the corresponding code using this Java program. Really?!
            return "jclass";
        }
        if (name.equals("java.lang.Throwable")) {
            return "jthrowable";
        }
        return "jobject";
    }

    /**
     * For a given Java type, get the corresponding C++ type if we're returning it from a function.
     *
     * @param type The Java return type.
     * @return A string representation of the C++ return type.
     */
     public static String getCReturnType(Class<?> type, boolean aNarrowChars) {
        if (type.getCanonicalName().equals("java.lang.Void")) {
            return "void";
        }
        String cParameterType = getCParameterType(type, aNarrowChars);
        if (cParameterType.equals("const nsAString&") || cParameterType.equals("const nsACString&")) {
            return "jstring";
        } else {
            return cParameterType;
        }
    }

    /**
     * Gets the type-specific part of the  JNI function to use to get or set a field of a given type.
     *
     * @param aFieldType The Java type of the field.
     * @return A string representation of the JNI call function substring to use.
     */
    public static String getFieldType(Class<?> aFieldType) {
        String name = aFieldType.getCanonicalName();

        if (sFieldTypes.containsKey(name)) {
            return sFieldTypes.get(name);
        }
        return "Object";
    }

    /**
     * Gets the appropriate JNI call function to use to invoke a Java method with the given return
     * type. This, plus a call postfix (Such as "A") forms a complete JNI call function name.
     *
     * @param aReturnType The Java return type of the method being generated.
     * @param isStatic Boolean indicating if the underlying Java method is declared static.
     * @return A string representation of the JNI call function prefix to use.
     */
    public static String getCallPrefix(Class<?> aReturnType, boolean isStatic) {
        String name = aReturnType.getCanonicalName();
        if (isStatic) {
            if (sStaticCallTypes.containsKey(name)) {
                return sStaticCallTypes.get(name);
            }
            return "CallStaticObjectMethod";
        } else {
            if (sInstanceCallTypes.containsKey(name)) {
                return sInstanceCallTypes.get(name);
            }
            return "CallObjectMethod";
        }
    }

    /**
     * On failure, the generated method returns a null-esque value. This helper method gets the
     * appropriate failure return value for a given Java return type, plus a leading space.
     *
     * @param type Java return type of method being generated
     * @return String representation of the failure return value to be used in the generated code.
     */
    public static String getFailureReturnForType(Class<?> type) {
        String name = type.getCanonicalName();
        if (sFailureReturns.containsKey(name)) {
            return sFailureReturns.get(name);
        }
        return " nullptr";
    }

    /**
     * Helper method to get the type signature for methods, given argument and return type.
     * Allows for the near-identical logic needed for constructors and methods to be shared.
     * (Alas, constructor does not extend method)
     *
     * @param arguments Argument types of the underlying method.
     * @param returnType Return type of the underlying method.
     * @return The canonical Java type string for the method. eg. (IIIIFZ)Lorg/mozilla/gecko/gfx/ViewTransform;
     */
    private static String getTypeSignatureInternal(Class<?>[] arguments, Class<?> returnType) {
        StringBuilder sb = new StringBuilder();
        sb.append('(');

        // For each argument, write its signature component to the buffer..
        for (int i = 0; i < arguments.length; i++) {
            writeTypeSignature(sb, arguments[i]);
        }
        sb.append(')');

        // Write the return value's signature..
        writeTypeSignature(sb, returnType);
        return sb.toString();
    }

    /**
     * Get the canonical JNI type signature for a Field.
     *
     * @param aField The field to generate a signature for.
     * @return The canonical JNI type signature for this method.
     */
    protected static String getTypeSignatureStringForField(Field aField) {
        StringBuilder sb = new StringBuilder();
        writeTypeSignature(sb, aField.getType());
        return sb.toString();
    }

    /**
     * Get the canonical JNI type signature for a method.
     *
     * @param aMethod The method to generate a signature for.
     * @return The canonical JNI type signature for this method.
     */
    protected static String getTypeSignatureStringForMethod(Method aMethod) {
        Class<?>[] arguments = aMethod.getParameterTypes();
        Class<?> returnType = aMethod.getReturnType();
        return getTypeSignatureInternal(arguments, returnType);
    }

    /**
     * Get the canonical JNI type signature for a Constructor.
     *
     * @param aConstructor The Constructor to generate a signature for.
     * @return The canonical JNI type signature for this method.
     */
    protected static String getTypeSignatureStringForConstructor(Constructor aConstructor) {
        Class<?>[] arguments = aConstructor.getParameterTypes();
        return getTypeSignatureInternal(arguments, Void.class);
    }

    public static String getTypeSignatureStringForMember(Member aMember) {
        if (aMember instanceof Method) {
            return getTypeSignatureStringForMethod((Method) aMember);
        } else if (aMember instanceof Field) {
            return getTypeSignatureStringForField((Field) aMember);
        } else {
            return getTypeSignatureStringForConstructor((Constructor) aMember);
        }
    }

    public static String getTypeSignatureString(Constructor aConstructor) {
        Class<?>[] arguments = aConstructor.getParameterTypes();
        StringBuilder sb = new StringBuilder();
        sb.append('(');

        // For each argument, write its signature component to the buffer..
        for (int i = 0; i < arguments.length; i++) {
            writeTypeSignature(sb, arguments[i]);
        }

        // Constructors always return Void.
        sb.append(")V");
        return sb.toString();
    }

    /**
     * Helper method used by getTypeSignatureStringForMethod to build the signature. Write the subsignature
     * of a given type into the buffer.
     *
     * @param sb The buffer to write into.
     * @param c  The type of the element to write the subsignature of.
     */
    private static void writeTypeSignature(StringBuilder sb, Class<?> c) {
        String name = c.getCanonicalName().replaceAll("\\.", "/");

        // Determine if this is an array type and, if so, peel away the array operators..
        int len = name.length();
        while (name.endsWith("[]")) {
            sb.append('[');
            name = name.substring(0, len - 2);
            len = len - 2;
        }

        if (c.isArray()) {
            c = c.getComponentType();
        }

        Class<?> containerClass = c.getDeclaringClass();
        if (containerClass != null) {
            // Is an inner class. Add the $ symbol.
            final int lastSlash = name.lastIndexOf('/');
            name = name.substring(0, lastSlash) + '$' + name.substring(lastSlash+1);
        }

        // Look in the hashmap for the remainder...
        if (sCanonicalSignatureParts.containsKey(name)) {
            // It was a primitive type, so lookup was a success.
            sb.append(sCanonicalSignatureParts.get(name));
        } else {
            // It was a reference type - generate.
            sb.append('L');
            sb.append(name);
            sb.append(';');
        }
    }

    /**
     * Produces a C method signature, sans semicolon, for the given Java Method. Useful for both
     * generating header files and method bodies.
     *
     * @param aArgumentTypes Argument types of the Java method being wrapped.
     * @param aReturnType Return type of the Java method being wrapped.
     * @param aCMethodName Name of the method to generate in the C++ class.
     * @param aCClassName Name of the C++ class into which the method is declared.
     * @return The C++ method implementation signature for the method described.
     */
    public static String getCImplementationMethodSignature(Class<?>[] aArgumentTypes, Class<?> aReturnType, String aCMethodName, String aCClassName, boolean aNarrowChars) {
        StringBuilder retBuffer = new StringBuilder();

        retBuffer.append(getCReturnType(aReturnType, aNarrowChars));
        retBuffer.append(' ');
        retBuffer.append(aCClassName);
        retBuffer.append("::");
        retBuffer.append(aCMethodName);
        retBuffer.append('(');

        // Write argument types...
        for (int aT = 0; aT < aArgumentTypes.length; aT++) {
            retBuffer.append(getCParameterType(aArgumentTypes[aT], aNarrowChars));
            retBuffer.append(" a");
            // We, imaginatively, call our arguments a1, a2, a3...
            // The only way to preserve the names from Java would be to parse the
            // Java source, which would be computationally hard.
            retBuffer.append(aT);
            if (aT != aArgumentTypes.length - 1) {
                retBuffer.append(", ");
            }
        }
        retBuffer.append(')');
        return retBuffer.toString();
    }

    /**
     * Produces a C method signature, sans semicolon, for the given Java Method. Useful for both
     * generating header files and method bodies.
     *
     * @param aArgumentTypes Argument types of the Java method being wrapped.
     * @param aArgumentAnnotations The annotations on the Java method arguments. Used to specify
     *                             default values etc.
     * @param aReturnType Return type of the Java method being wrapped.
     * @param aCMethodName Name of the method to generate in the C++ class.
     * @param aCClassName Name of the C++ class into which the method is declared.e
     * @param aIsStaticStub true if the generated C++ method should be static, false otherwise.
     * @return The generated C++ header method signature for the method described.
     */
    public static String getCHeaderMethodSignature(Class<?>[] aArgumentTypes, Annotation[][] aArgumentAnnotations, Class<?> aReturnType, String aCMethodName, String aCClassName, boolean aIsStaticStub, boolean aNarrowChars) {
        StringBuilder retBuffer = new StringBuilder();

        // Add the static keyword, if applicable.
        if (aIsStaticStub) {
            retBuffer.append("static ");
        }

        // Write return type..
        retBuffer.append(getCReturnType(aReturnType, aNarrowChars));
        retBuffer.append(' ');
        retBuffer.append(aCMethodName);
        retBuffer.append('(');

        // Write argument types...
        for (int aT = 0; aT < aArgumentTypes.length; aT++) {
            retBuffer.append(getCParameterType(aArgumentTypes[aT], aNarrowChars));
            retBuffer.append(" a");
            // We, imaginatively, call our arguments a1, a2, a3...
            // The only way to preserve the names from Java would be to parse the
            // Java source, which would be computationally hard.
            retBuffer.append(aT);

            // Append the default value, if there is one..
            retBuffer.append(getDefaultValueString(aArgumentTypes[aT], aArgumentAnnotations[aT]));

            if (aT != aArgumentTypes.length - 1) {
                retBuffer.append(", ");
            }
        }
        retBuffer.append(')');
        return retBuffer.toString();
    }

    /**
     * If the given Annotation[] contains an OptionalGeneratedParameter annotation then return a
     * string assigning an argument of type aArgumentType to the default value for that type.
     * Otherwise, return the empty string.
     *
     * @param aArgumentType        The type of the argument to consider.
     * @param aArgumentAnnotations The annotations on the argument to consider.
     * @return An appropriate string to append to the signature of this argument assigning it to a
     *         default value (Or not, as applicable).
     */
    public static String getDefaultValueString(Class<?> aArgumentType, Annotation[] aArgumentAnnotations) {
        for (int i = 0; i < aArgumentAnnotations.length; i++) {
            Class<? extends Annotation> annotationType = aArgumentAnnotations[i].annotationType();
            final String annotationTypeName = annotationType.getName();
            if (annotationTypeName.equals("org.mozilla.gecko.mozglue.generatorannotations.OptionalGeneratedParameter")) {
                return " = " + getDefaultParameterValueForType(aArgumentType);
            }
        }
        return "";
    }

    /**
     * Helper method to return an appropriate default parameter value for an argument of a given type.
     * The lookup table contains values for primitive types and strings. All other object types default
     * to null pointers.
     *
     * @param aArgumentType The parameter type for which a default value is desired.
     * @return An appropriate string representation of the default value selected, for use in generated
     *         C++ code.
     */
    private static String getDefaultParameterValueForType(Class<?> aArgumentType) {
        String typeName = aArgumentType.getCanonicalName();
        if (sDefaultParameterValues.containsKey(typeName)) {
            return sDefaultParameterValues.get(typeName);
        } else if (isCharSequence(aArgumentType)) {
            return "EmptyString()";
        } else {
            return "nullptr";
        }
    }

    /**
     * Helper method that returns the number of reference types in the arguments of m.
     *
     * @param aArgs The method arguments to consider.
     * @return How many of the arguments of m are nonprimitive.
     */
    public static int enumerateReferenceArguments(Class<?>[] aArgs) {
        int ret = 0;
        for (int i = 0; i < aArgs.length; i++) {
            String name = aArgs[i].getCanonicalName();
            if (!sBasicCTypes.containsKey(name)) {
                ret++;
            }
        }
        return ret;
    }

    /**
     * Helper method that returns true iff the given method has a string argument.
     *
     * @param m The method to consider.
     * @return True if the given method has a string argument, false otherwise.
     */
    public static boolean hasStringArgument(Method m) {
        Class<?>[] args = m.getParameterTypes();
        for (int i = 0; i < args.length; i++) {
            if (isCharSequence(args[i])) {
                return true;
            }
        }
        return false;
    }

    /**
     * Write the argument array assignment line for the given argument type. Does not support array
     * types.
     *
     * @param type    Type of this argument according to the target Java method's signature.
     * @param argName Wrapper function argument name corresponding to this argument.
     */
    public static String getArrayArgumentMashallingLine(Class<?> type, String argName) {
        StringBuilder sb = new StringBuilder();

        String name = type.getCanonicalName();
        if (sCanonicalSignatureParts.containsKey(name)) {
            sb.append(sCanonicalSignatureParts.get(name).toLowerCase());
            sb.append(" = ").append(argName).append(";\n");
        } else {
            if (isCharSequence(type)) {
                sb.append("l = AndroidBridge::NewJavaString(env, ").append(argName).append(");\n");
            } else {
                sb.append("l = ").append(argName).append(";\n");
            }
        }

        return sb.toString();
    }

    /**
     * Returns true if the type provided is an object type. Returns false otherwise
     *
     * @param aType The type to consider.
     * @return true if the method provided is an object type, false otherwise.
     */
    public static boolean isObjectType(Class<?> aType) {
        return !sBasicCTypes.containsKey(aType.getCanonicalName());
    }

    /**
     * For a given Java class, get the name of the value in C++ which holds a reference to it.
     *
     * @param aClass Target Java class.
     * @return The name of the C++ jclass entity referencing the given class.
     */
    public static String getClassReferenceName(Class<?> aClass) {
        String className = aClass.getSimpleName();
        return 'm' + className + "Class";
    }

    /**
     * Generate a line to get a global reference to the Java class given.
     *
     * @param aClass The target Java class.
     * @return The generated code to populate the reference to the class.
     */
    public static String getStartupLineForClass(Class<?> aClass) {
        StringBuilder sb = new StringBuilder();
        sb.append("    ");
        sb.append(getClassReferenceName(aClass));
        sb.append(" = getClassGlobalRef(\"");

        String name = aClass.getCanonicalName().replaceAll("\\.", "/");
        Class<?> containerClass = aClass.getDeclaringClass();
        if (containerClass != null) {
            // Is an inner class. Add the $ symbol.
            final int lastSlash = name.lastIndexOf('/');
            name = name.substring(0, lastSlash) + '$' + name.substring(lastSlash+1);
        }

        sb.append(name);
        sb.append("\");\n");
        return sb.toString();
    }

    /**
     * Helper method to determine if this object implements CharSequence
     * @param aClass Class to check for CharSequence-esqueness
     * @return True if the given class implements CharSequence, false otherwise.
     */
    public static boolean isCharSequence(Class<?> aClass) {
        if (aClass.getCanonicalName().equals("java.lang.CharSequence")) {
            return true;
        }
        Class<?>[] interfaces = aClass.getInterfaces();
        for (Class<?> c : interfaces) {
            if (c.getCanonicalName().equals("java.lang.CharSequence")) {
                return true;
            }
        }
        return false;
    }

    /**
     * Helper method to read the modifier bits of the given method to determine if it is static.
     * @param aMember The Member to check.
     * @return true of the method is declared static, false otherwise.
     */
    public static boolean isMemberStatic(Member aMember) {
        int aMethodModifiers = aMember.getModifiers();
        return Modifier.isStatic(aMethodModifiers);
    }

    /**
     * Helper method to read the modifier bits of the given method to determine if it is static.
     * @param aMember The Member to check.
     * @return true of the method is declared static, false otherwise.
     */
    public static boolean isMemberFinal(Member aMember) {
        int aMethodModifiers = aMember.getModifiers();
        return Modifier.isFinal(aMethodModifiers);
    }
}
