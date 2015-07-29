/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors;

import org.mozilla.gecko.annotationProcessors.classloader.AnnotatableEntity;
import org.mozilla.gecko.annotationProcessors.classloader.ClassWithOptions;
import org.mozilla.gecko.annotationProcessors.utils.Utils;

import java.lang.annotation.Annotation;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.HashSet;

public class CodeGenerator {
    private static final Class<?>[] EMPTY_CLASS_ARRAY = new Class<?>[0];

    // Buffers holding the strings to ultimately be written to the output files.
    private final StringBuilder cpp = new StringBuilder();
    private final StringBuilder header = new StringBuilder();
    private final StringBuilder natives = new StringBuilder();
    private final StringBuilder nativesInits = new StringBuilder();

    private final Class<?> cls;
    private final String clsName;

    private final HashSet<String> takenMethodNames = new HashSet<String>();

    public CodeGenerator(ClassWithOptions annotatedClass) {
        this.cls = annotatedClass.wrappedClass;
        this.clsName = annotatedClass.generatedName;

        header.append(
                "class " + clsName + " : public mozilla::jni::Class<" + clsName + ">\n" +
                "{\n" +
                "public:\n" +
                "    typedef mozilla::jni::Ref<" + clsName + "> Ref;\n" +
                "    typedef mozilla::jni::LocalRef<" + clsName + "> LocalRef;\n" +
                "    typedef mozilla::jni::GlobalRef<" + clsName + "> GlobalRef;\n" +
                "    typedef const mozilla::jni::Param<" + clsName + ">& Param;\n" +
                "\n" +
                "    static constexpr char name[] =\n" +
                "            \"" + cls.getName().replace('.', '/') + "\";\n" +
                "\n" +
                "protected:\n" +
                "    " + clsName + "(jobject instance) : Class(instance) {}\n" +
                "\n");

        cpp.append(
                "constexpr char " + clsName + "::name[];\n" +
                "\n");

        natives.append(
                "template<class Impl>\n" +
                "class " + clsName + "::Natives : " +
                        "public mozilla::jni::NativeImpl<" + clsName + ", Impl>\n" +
                "{\n");
    }

    private String getTraitsName(String uniqueName, boolean includeScope) {
        return (includeScope ? clsName + "::" : "") + uniqueName + "_t";
    }

    private String getNativeParameterType(Class<?> type, AnnotationInfo info) {
        if (type == cls) {
            return clsName + "::Param";
        }
        return Utils.getNativeParameterType(type, info);
    }

    private String getNativeReturnType(Class<?> type, AnnotationInfo info) {
        if (type == cls) {
            return clsName + "::LocalRef";
        }
        return Utils.getNativeReturnType(type, info);
    }

    private void generateMember(AnnotationInfo info, Member member,
                                String uniqueName, Class<?> type, Class<?>[] argTypes) {
        final StringBuilder args = new StringBuilder();
        for (Class<?> argType : argTypes) {
            args.append("\n                " + getNativeParameterType(argType, info) + ",");
        }
        if (args.length() > 0) {
            args.setLength(args.length() - 1);
        }

        header.append(
                "public:\n" +
                "    struct " + getTraitsName(uniqueName, /* includeScope */ false) + " {\n" +
                "        typedef " + clsName + " Owner;\n" +
                "        typedef " + getNativeReturnType(type, info) + " ReturnType;\n" +
                "        typedef " + getNativeParameterType(type, info) + " SetterType;\n" +
                "        typedef mozilla::jni::Args<" + args + "> Args;\n" +
                "        static constexpr char name[] = \"" +
                        Utils.getMemberName(member) + "\";\n" +
                "        static constexpr char signature[] =\n" +
                "                \"" + Utils.getSignature(member) + "\";\n" +
                "        static const bool isStatic = " + Utils.isStatic(member) + ";\n" +
                "        static const bool isMultithreaded = " + info.isMultithreaded + ";\n" +
                "        static const mozilla::jni::ExceptionMode exceptionMode =\n" +
                "                " + (
                        info.catchException ? "mozilla::jni::ExceptionMode::NSRESULT" :
                        info.noThrow ?        "mozilla::jni::ExceptionMode::IGNORE" :
                                              "mozilla::jni::ExceptionMode::ABORT") + ";\n" +
                "    };\n" +
                "\n");

        cpp.append(
                "constexpr char " + getTraitsName(uniqueName, /* includeScope */ true) +
                        "::name[];\n" +
                "constexpr char " + getTraitsName(uniqueName, /* includeScope */ true) +
                        "::signature[];\n" +
                "\n");
    }

    private String getUniqueMethodName(String basename) {
        String newName = basename;
        int index = 1;

        while (takenMethodNames.contains(newName)) {
            newName = basename + (++index);
        }

        takenMethodNames.add(newName);
        return newName;
    }

    /**
     * Generate a method prototype that includes return and argument types,
     * without specifiers (static, const, etc.).
     */
    private String generatePrototype(String name, Class<?>[] argTypes,
                                     Class<?> returnType, AnnotationInfo info,
                                     boolean includeScope, boolean includeArgName) {

        final StringBuilder proto = new StringBuilder();
        int argIndex = 0;

        if (info.catchException) {
            proto.append("nsresult ");
        } else {
            proto.append(getNativeReturnType(returnType, info)).append(' ');
        }

        if (includeScope) {
            proto.append(clsName).append("::");
        }

        proto.append(name).append('(');

        for (Class<?> argType : argTypes) {
            proto.append(getNativeParameterType(argType, info));
            if (includeArgName) {
                proto.append(" a").append(argIndex++);
            }
            proto.append(", ");
        }

        if (info.catchException && returnType != void.class) {
            proto.append(getNativeReturnType(returnType, info)).append('*');
            if (includeArgName) {
                proto.append(" a").append(argIndex++);
            }
            proto.append(", ");
        }

        if (proto.substring(proto.length() - 2).equals(", ")) {
            proto.setLength(proto.length() - 2);
        }

        return proto.append(')').toString();
    }

    /**
     * Generate a method declaration that includes the prototype with specifiers,
     * but without the method body.
     */
    private String generateDeclaration(String name, Class<?>[] argTypes,
                                       Class<?> returnType, AnnotationInfo info,
                                       boolean isStatic) {

        return (isStatic ? "static " : "") +
            generatePrototype(name, argTypes, returnType, info,
                              /* includeScope */ false, /* includeArgName */ false) +
            (isStatic ? ";" : " const;");
    }

    /**
     * Generate a method definition that includes the prototype with specifiers,
     * and with the method body.
     */
    private String generateDefinition(String accessorName, String name, Class<?>[] argTypes,
                                      Class<?> returnType, AnnotationInfo info, boolean isStatic) {

        final StringBuilder def = new StringBuilder(
                generatePrototype(name, argTypes, returnType, info,
                                  /* includeScope */ true, /* includeArgName */ true));

        if (!isStatic) {
            def.append(" const");
        }
        def.append("\n{\n");


        // Generate code to handle the return value, if needed.
        // We initialize rv to NS_OK instead of NS_ERROR_* because loading NS_OK (0) uses
        // fewer instructions. We are guaranteed to set rv to the correct value later.

        if (info.catchException && returnType == void.class) {
            def.append(
                    "    nsresult rv = NS_OK;\n" +
                    "    ");

        } else if (info.catchException) {
            // Non-void return type
            final String resultArg = "a" + argTypes.length;
            def.append(
                    "    MOZ_ASSERT(" + resultArg + ");\n" +
                    "    nsresult rv = NS_OK;\n" +
                    "    *" + resultArg + " = ");

        } else {
            def.append(
                    "    return ");
        }


        // Generate a call, e.g., Method<Traits>::Call(a0, a1, a2);

        def.append(accessorName).append("(")
           .append(isStatic ? "nullptr" : "this");

        if (info.catchException) {
            def.append(", &rv");
        } else {
            def.append(", nullptr");
        }

        // Generate the call argument list.
        for (int argIndex = 0; argIndex < argTypes.length; argIndex++) {
            def.append(", a").append(argIndex);
        }

        def.append(");\n");


        if (info.catchException) {
            def.append("    return rv;\n");
        }

        return def.append("}").toString();
    }

    /**
     * Append the appropriate generated code to the buffers for the method provided.
     *
     * @param annotatedMethod The Java method, plus annotation data.
     */
    public void generateMethod(AnnotatableEntity annotatedMethod) {
        // Unpack the tuple and extract some useful fields from the Method..
        final Method method = annotatedMethod.getMethod();
        final AnnotationInfo info = annotatedMethod.mAnnotationInfo;
        final String uniqueName = getUniqueMethodName(info.wrapperName);
        final Class<?>[] argTypes = method.getParameterTypes();
        final Class<?> returnType = method.getReturnType();

        if (method.isSynthetic()) {
            return;
        }

        generateMember(info, method, uniqueName, returnType, argTypes);

        final boolean isStatic = Utils.isStatic(method);

        header.append(
                "    " + generateDeclaration(info.wrapperName, argTypes,
                                             returnType, info, isStatic) + "\n" +
                "\n");

        cpp.append(
                generateDefinition(
                        "mozilla::jni::Method<" +
                                getTraitsName(uniqueName, /* includeScope */ false) + ">::Call",
                        info.wrapperName, argTypes, returnType, info, isStatic) + "\n" +
                "\n");
    }

    /**
     * Append the appropriate generated code to the buffers for the native method provided.
     *
     * @param annotatedMethod The Java native method, plus annotation data.
     */
    public void generateNative(AnnotatableEntity annotatedMethod) {
        // Unpack the tuple and extract some useful fields from the Method..
        final Method method = annotatedMethod.getMethod();
        final AnnotationInfo info = annotatedMethod.mAnnotationInfo;
        final String uniqueName = getUniqueMethodName(info.wrapperName);
        final Class<?>[] argTypes = method.getParameterTypes();
        final Class<?> returnType = method.getReturnType();

        generateMember(info, method, uniqueName, returnType, argTypes);

        final String traits = getTraitsName(uniqueName, /* includeScope */ true);

        if (nativesInits.length() > 0) {
            nativesInits.append(',');
        }

        nativesInits.append(
                "\n" +
                "\n" +
                "        mozilla::jni::MakeNativeMethod<" + traits + ">(\n" +
                "                mozilla::jni::NativeStub<" + traits + ", Impl>\n" +
                "                ::template Wrap<&Impl::" + info.wrapperName + ">)");
    }

    private String getLiteral(Object val, AnnotationInfo info) {
        final Class<?> type = val.getClass();

        if (type == char.class || type == Character.class) {
            final char c = (char) val;
            if (c >= 0x20 && c < 0x7F) {
                return "'" + c + '\'';
            }
            return "u'\\u" + Integer.toHexString(0x10000 | (int) c).substring(1) + '\'';

        } else if (type == CharSequence.class || type == String.class) {
            final CharSequence str = (CharSequence) val;
            final StringBuilder out = new StringBuilder(info.narrowChars ? "u8\"" : "u\"");
            for (int i = 0; i < str.length(); i++) {
                final char c = str.charAt(i);
                if (c >= 0x20 && c < 0x7F) {
                    out.append(c);
                } else {
                    out.append("\\u").append(Integer.toHexString(0x10000 | (int) c).substring(1));
                }
            }
            return out.append('"').toString();
        }

        return String.valueOf(val);
    }

    public void generateField(AnnotatableEntity annotatedField) {
        final Field field = annotatedField.getField();
        final AnnotationInfo info = annotatedField.mAnnotationInfo;
        final String uniqueName = info.wrapperName;
        final Class<?> type = field.getType();

        // Handles a peculiar case when dealing with enum types. We don't care about this field.
        // It just gets in the way and stops our code from compiling.
        if (field.isSynthetic() || field.getName().equals("$VALUES")) {
            return;
        }

        final boolean isStatic = Utils.isStatic(field);
        final boolean isFinal = Utils.isFinal(field);

        if (isStatic && isFinal && (type.isPrimitive() || type == String.class)) {
            Object val = null;
            try {
                val = field.get(null);
            } catch (final IllegalAccessException e) {
            }

            if (val != null && type.isPrimitive()) {
                // For static final primitive fields, we can use a "static const" declaration.
                header.append(
                    "public:\n" +
                    "    static const " + Utils.getNativeReturnType(type, info) +
                            ' ' + info.wrapperName + " = " + getLiteral(val, info) + ";\n" +
                    "\n");
                return;

            } else if (val != null && type == String.class) {
                final String nativeType = info.narrowChars ? "char" : "char16_t";

                header.append(
                    "public:\n" +
                    "    static const " + nativeType + ' ' + info.wrapperName + "[];\n" +
                    "\n");

                cpp.append(
                    "const " + nativeType + ' ' + clsName + "::" + info.wrapperName +
                            "[] = " + getLiteral(val, info) + ";\n" +
                    "\n");
                return;
            }

            // Fall back to using accessors if we encounter an exception.
        }

        generateMember(info, field, uniqueName, type, EMPTY_CLASS_ARRAY);

        final Class<?>[] getterArgs = EMPTY_CLASS_ARRAY;

        header.append(
                "    " + generateDeclaration(info.wrapperName, getterArgs,
                                             type, info, isStatic) + "\n" +
                "\n");

        cpp.append(
                generateDefinition(
                        "mozilla::jni::Field<" +
                                getTraitsName(uniqueName, /* includeScope */ false) + ">::Get",
                        info.wrapperName, getterArgs, type, info, isStatic) + "\n" +
                "\n");

        if (isFinal) {
            return;
        }

        final Class<?>[] setterArgs = new Class<?>[] { type };

        header.append(
                "    " + generateDeclaration(info.wrapperName, setterArgs,
                                             void.class, info, isStatic) + "\n" +
                "\n");

        cpp.append(
                generateDefinition(
                        "mozilla::jni::Field<" +
                                getTraitsName(uniqueName, /* includeScope */ false) + ">::Set",
                        info.wrapperName, setterArgs, void.class, info, isStatic) + "\n" +
                "\n");
    }

    public void generateConstructor(AnnotatableEntity annotatedConstructor) {
        // Unpack the tuple and extract some useful fields from the Method..
        final Constructor<?> method = annotatedConstructor.getConstructor();
        final AnnotationInfo info = annotatedConstructor.mAnnotationInfo;
        final String wrapperName = "New";
        final String uniqueName = getUniqueMethodName(wrapperName);
        final Class<?>[] argTypes = method.getParameterTypes();
        final Class<?> returnType = cls;

        if (method.isSynthetic()) {
            return;
        }

        generateMember(info, method, uniqueName, returnType, argTypes);

        header.append(
                "    " + generateDeclaration(wrapperName, argTypes,
                                             returnType, info, /* isStatic */ true) + "\n" +
                "\n");

        cpp.append(
                generateDefinition(
                        "mozilla::jni::Constructor<" +
                                getTraitsName(uniqueName, /* includeScope */ false) + ">::Call",
                        wrapperName, argTypes, returnType, info, /* isStatic */ true) + "\n" +
                "\n");
    }

    public void generateMembers(Member[] members) {
        for (Member m : members) {
            if (!Modifier.isPublic(m.getModifiers())) {
                continue;
            }

            String name = Utils.getMemberName(m);
            name = name.substring(0, 1).toUpperCase() + name.substring(1);

            final AnnotationInfo info = new AnnotationInfo(name,
                    /* multithread */ true, /* nothrow */ false,
                    /* narrow */ false, /* catchException */ true);
            final AnnotatableEntity entity = new AnnotatableEntity(m, info);

            if (m instanceof Constructor) {
                generateConstructor(entity);
            } else if (m instanceof Method) {
                generateMethod(entity);
            } else if (m instanceof Field) {
                generateField(entity);
            } else {
                throw new IllegalArgumentException(
                        "expected member to be Constructor, Method, or Field");
            }
        }
    }

    /**
     * Get the finalised bytes to go into the generated wrappers file.
     *
     * @return The bytes to be written to the wrappers file.
     */
    public String getWrapperFileContents() {
        return cpp.toString();
    }

    /**
     * Get the finalised bytes to go into the generated header file.
     *
     * @return The bytes to be written to the header file.
     */
    public String getHeaderFileContents() {
        if (nativesInits.length() > 0) {
            header.append(
                    "public:\n" +
                    "    template<class Impl> class Natives;\n");
        }
        header.append(
                "};\n" +
                "\n");
        return header.toString();
    }

    /**
     * Get the finalised bytes to go into the generated natives header file.
     *
     * @return The bytes to be written to the header file.
     */
    public String getNativesFileContents() {
        if (nativesInits.length() == 0) {
            return "";
        }
        natives.append(
                "public:\n" +
                "    static constexpr JNINativeMethod methods[] = {" + nativesInits + '\n' +
                "    };\n" +
                "};\n" +
                "\n" +
                "template<class Impl>\n" +
                "constexpr JNINativeMethod " + clsName + "::Natives<Impl>::methods[];\n" +
                "\n");
        return natives.toString();
    }
}
