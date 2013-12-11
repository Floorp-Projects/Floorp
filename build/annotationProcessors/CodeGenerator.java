/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors;

import org.mozilla.gecko.annotationProcessors.classloader.AnnotatableEntity;
import org.mozilla.gecko.annotationProcessors.utils.Utils;

import java.lang.annotation.Annotation;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.HashMap;
import java.util.HashSet;

public class CodeGenerator {
    private static final Class<?>[] EMPTY_CLASS_ARRAY = new Class<?>[0];
    private static final Annotation[][] GETTER_ARGUMENT_ANNOTATIONS = new Annotation[0][0];
    private static final Annotation[][] SETTER_ARGUMENT_ANNOTATIONS = new Annotation[1][0];

    // Buffers holding the strings to ultimately be written to the output files.
    private final StringBuilder zeroingCode = new StringBuilder();
    private final StringBuilder wrapperStartupCode = new StringBuilder();
    private final StringBuilder wrapperMethodBodies = new StringBuilder();
    private final StringBuilder headerPublic = new StringBuilder();
    private final StringBuilder headerProtected = new StringBuilder();

    private final HashSet<String> seenClasses = new HashSet<String>();

    private final String mCClassName;

    private final Class<?> mClassToWrap;

    private boolean mHasEncounteredDefaultConstructor;

    // Used for creating unique names for method ID fields in the face of
    private final HashMap<Member, String> mMembersToIds = new HashMap<Member, String>();
    private final HashSet<String> mTakenMemberNames = new HashSet<String>();
    private int mNameMunger;

    public CodeGenerator(Class<?> aClass, String aGeneratedName) {
        mClassToWrap = aClass;
        mCClassName = aGeneratedName;

        // Write the file header things. Includes and so forth.
        // GeneratedJNIWrappers.cpp is generated as the concatenation of wrapperStartupCode with
        // wrapperMethodBodies. Similarly, GeneratedJNIWrappers.h is the concatenation of headerPublic
        // with headerProtected.
        wrapperStartupCode.append("void ").append(mCClassName).append("::InitStubs(JNIEnv *jEnv) {\n" +
                                  "    initInit();\n");

        // Now we write the various GetStaticMethodID calls here...
        headerPublic.append("class ").append(mCClassName).append(" : public AutoGlobalWrappedJavaObject {\n" +
                            "public:\n" +
                            "    static void InitStubs(JNIEnv *jEnv);\n");
        headerProtected.append("protected:");

        generateWrapperMethod();
    }

    /**
     * Emit a static method which takes an instance of the class being wrapped and returns an instance
     * of the C++ wrapper class backed by that object.
     */
    private void generateWrapperMethod() {
        headerPublic.append("    static ").append(mCClassName).append("* Wrap(jobject obj);\n" +
                "    ").append(mCClassName).append("(jobject obj, JNIEnv* env) : AutoGlobalWrappedJavaObject(obj, env) {};\n");

        wrapperMethodBodies.append("\n").append(mCClassName).append("* ").append(mCClassName).append("::Wrap(jobject obj) {\n" +
                "    JNIEnv *env = GetJNIForThread();\n\n" +
                "    if (!env) {\n" +
                "        ALOG_BRIDGE(\"Aborted: No env - %s\", __PRETTY_FUNCTION__);\n" +
                "        return NULL;\n" +
                "    }\n\n" +
                "    ").append(mCClassName).append("* ret = new ").append(mCClassName).append("(obj, env);\n" +
                "    env->DeleteLocalRef(obj);\n" +
                "    return ret;\n" +
                "}\n");
    }

    private void generateMemberCommon(Member theMethod, String aCMethodName, Class<?> aClass) {
        ensureClassHeaderAndStartup(aClass);
        writeMemberIdField(theMethod, aCMethodName);
        writeStartupCode(theMethod);
    }

    /**
     * Append the appropriate generated code to the buffers for the method provided.
     *
     * @param aMethodTuple The Java method, plus annotation data.
     */
    public void generateMethod(AnnotatableEntity aMethodTuple) {
        // Unpack the tuple and extract some useful fields from the Method..
        Method theMethod = aMethodTuple.getMethod();

        String CMethodName = aMethodTuple.mAnnotationInfo.wrapperName;

        generateMemberCommon(theMethod, CMethodName, mClassToWrap);

        boolean isFieldStatic = Utils.isMemberStatic(theMethod);
        boolean shallGenerateStatic = isFieldStatic || aMethodTuple.mAnnotationInfo.isStatic;

        Class<?>[] parameterTypes = theMethod.getParameterTypes();
        Class<?> returnType = theMethod.getReturnType();

        // Get the C++ method signature for this method.
        String implementationSignature = Utils.getCImplementationMethodSignature(parameterTypes, returnType, CMethodName, mCClassName);
        String headerSignature = Utils.getCHeaderMethodSignature(parameterTypes, theMethod.getParameterAnnotations(), returnType, CMethodName, mCClassName, shallGenerateStatic);

        // Add the header signature to the header file.
        writeSignatureToHeader(headerSignature);

        // Use the implementation signature to generate the method body...
        writeMethodBody(implementationSignature, CMethodName, theMethod, mClassToWrap, aMethodTuple.mAnnotationInfo.isStatic, aMethodTuple.mAnnotationInfo.isMultithreaded);
    }

    private void generateGetterOrSetterBody(Class<?> aFieldType, String aFieldName, boolean aIsFieldStatic, boolean isSetter) {
        StringBuilder argumentContent = null;

        if (isSetter) {
            Class<?>[] setterArguments = new Class<?>[]{aFieldType};
            // Marshall the argument..
            argumentContent = getArgumentMarshalling(setterArguments);
        }

        boolean isObjectReturningMethod = Utils.isObjectType(aFieldType);
        wrapperMethodBodies.append("    ");
        if (isSetter) {
            wrapperMethodBodies.append("env->Set");
        } else {
            wrapperMethodBodies.append("return ");

            if (isObjectReturningMethod) {
                wrapperMethodBodies.append("static_cast<").append(Utils.getCReturnType(aFieldType)).append(">(");
            }

            wrapperMethodBodies.append("env->Get");
        }

        if (aIsFieldStatic) {
            wrapperMethodBodies.append("Static");
        }
        wrapperMethodBodies.append(Utils.getFieldType(aFieldType))
                           .append("Field(");

        // Static will require the class and the field id. Nonstatic, the object and the field id.
        if (aIsFieldStatic) {
            wrapperMethodBodies.append(Utils.getClassReferenceName(mClassToWrap));
        } else {
            wrapperMethodBodies.append("wrapped_obj");
        }
        wrapperMethodBodies.append(", j")
                           .append(aFieldName);
        if (isSetter) {
            wrapperMethodBodies.append(argumentContent);
        }

        if (!isSetter && isObjectReturningMethod) {
            wrapperMethodBodies.append(')');
        }
        wrapperMethodBodies.append(");\n" +
                               "}\n");
    }

    public void generateField(AnnotatableEntity aFieldTuple) {
        Field theField = aFieldTuple.getField();

        // Handles a peculiar case when dealing with enum types. We don't care about this field.
        // It just gets in the way and stops our code from compiling.
        if (theField.getName().equals("$VALUES")) {
            return;
        }

        String CFieldName = aFieldTuple.mAnnotationInfo.wrapperName;

        Class<?> fieldType = theField.getType();

        generateMemberCommon(theField, CFieldName, mClassToWrap);

        boolean isFieldStatic = Utils.isMemberStatic(theField);
        boolean isFieldFinal = Utils.isMemberFinal(theField);
        boolean shallGenerateStatic = isFieldStatic || aFieldTuple.mAnnotationInfo.isStatic;

        String getterName = "get" + CFieldName;
        String getterSignature = Utils.getCImplementationMethodSignature(EMPTY_CLASS_ARRAY, fieldType, getterName, mCClassName);
        String getterHeaderSignature = Utils.getCHeaderMethodSignature(EMPTY_CLASS_ARRAY, GETTER_ARGUMENT_ANNOTATIONS, fieldType, getterName, mCClassName, shallGenerateStatic);

        writeSignatureToHeader(getterHeaderSignature);

        writeFunctionStartupBoilerPlate(getterSignature, fieldType, isFieldStatic, true);

        generateGetterOrSetterBody(fieldType, CFieldName, isFieldStatic, false);

        // If field not final, also generate a setter function.
        if (!isFieldFinal) {
            String setterName = "set" + CFieldName;

            Class<?>[] setterArguments = new Class<?>[]{fieldType};

            String setterSignature = Utils.getCImplementationMethodSignature(setterArguments, Void.class, setterName, mCClassName);
            String setterHeaderSignature = Utils.getCHeaderMethodSignature(setterArguments, SETTER_ARGUMENT_ANNOTATIONS, Void.class, setterName, mCClassName, shallGenerateStatic);

            writeSignatureToHeader(setterHeaderSignature);

            writeFunctionStartupBoilerPlate(setterSignature, Void.class, isFieldStatic, true);

            generateGetterOrSetterBody(fieldType, CFieldName, isFieldStatic, true);
        }
    }

    public void generateConstructor(AnnotatableEntity aCtorTuple) {
        // Unpack the tuple and extract some useful fields from the Method..
        Constructor theCtor = aCtorTuple.getConstructor();
        String CMethodName = mCClassName;

        generateMemberCommon(theCtor, mCClassName, mClassToWrap);

        String implementationSignature = Utils.getCImplementationMethodSignature(theCtor.getParameterTypes(), Void.class, CMethodName, mCClassName);
        String headerSignature = Utils.getCHeaderMethodSignature(theCtor.getParameterTypes(), theCtor.getParameterAnnotations(), Void.class, CMethodName, mCClassName, false);

        // Slice off the "void " from the start of the constructor declaration.
        headerSignature = headerSignature.substring(5);
        implementationSignature = implementationSignature.substring(5);

        // Add the header signatures to the header file.
        writeSignatureToHeader(headerSignature);

        // Use the implementation signature to generate the method body...
        writeCtorBody(implementationSignature, theCtor, aCtorTuple.mAnnotationInfo.isMultithreaded);

        if (theCtor.getParameterTypes().length == 0) {
            mHasEncounteredDefaultConstructor = true;
        }
    }

    /**
     * Writes the appropriate header and startup code to ensure the existence of a reference to the
     * class specified. If this is already done, does nothing.
     *
     * @param aClass The target class.
     */
    private void ensureClassHeaderAndStartup(Class<?> aClass) {
        String className = aClass.getCanonicalName();
        if (seenClasses.contains(className)) {
            return;
        }

        zeroingCode.append("jclass ")
                   .append(mCClassName)
                   .append("::")
                   .append(Utils.getClassReferenceName(aClass))
                   .append(" = 0;\n");

        // Add a field to hold the reference...
        headerProtected.append("\n    static jclass ")
                       .append(Utils.getClassReferenceName(aClass))
                       .append(";\n");

        // Add startup code to populate it..
        wrapperStartupCode.append('\n')
                          .append(Utils.getStartupLineForClass(aClass));

        seenClasses.add(className);
    }

    /**
     * Write out the function startup boilerplate for the method described. Check for environment
     * existence,
     * @param methodSignature
     * @param returnType
     * @param aIsStatic
     * @param aIsThreaded
     */
    private void writeFunctionStartupBoilerPlate(String methodSignature, Class<?> returnType, boolean aIsStatic, boolean aIsThreaded) {
        // The start-of-function boilerplate. Does the bridge exist? Does the env exist? etc.
        wrapperMethodBodies.append('\n')
                           .append(methodSignature)
                           .append(" {\n");

        wrapperMethodBodies.append("    JNIEnv *env = ");
        if (!aIsThreaded) {
            wrapperMethodBodies.append("AndroidBridge::GetJNIEnv();\n");
        } else {
            wrapperMethodBodies.append("GetJNIForThread();\n");
        }
        wrapperMethodBodies.append("    if (!env) {\n" +
                                   "        ALOG_BRIDGE(\"Aborted: No env - %s\", __PRETTY_FUNCTION__);\n" +
                                   "        return").append(Utils.getFailureReturnForType(returnType)).append(";\n" +
                "    }\n\n");
    }

    /**
     * Write out the appropriate JNI frame pushing boilerplate for a call to the member provided (
     * which must be a constructor or method).
     *
     * @param aMethod A constructor/method being wrapped.
     * @param aIsObjectReturningMethod Does the method being wrapped return an object?
     */
    private void writeFramePushBoilerplate(Member aMethod, boolean aIsObjectReturningMethod) {
        if (aMethod instanceof Field) {
            throw new IllegalArgumentException("Tried to push frame for a FIELD?!");
        }

        Method m;
        Constructor c;

        Class<?> returnType;

        int localReferencesNeeded;
        if (aMethod instanceof Method) {
            m = (Method) aMethod;
            returnType = m.getReturnType();
            localReferencesNeeded = Utils.enumerateReferenceArguments(m.getParameterTypes());
        } else {
            c = (Constructor) aMethod;
            returnType = Void.class;
            localReferencesNeeded = Utils.enumerateReferenceArguments(c.getParameterTypes());
        }

        // Determine the number of local refs required for our local frame..
        // AutoLocalJNIFrame is not applicable here due to it's inability to handle return values.
        if (aIsObjectReturningMethod) {
            localReferencesNeeded++;
        }
        wrapperMethodBodies.append("    if (env->PushLocalFrame(").append(localReferencesNeeded).append(") != 0) {\n" +
                "        ALOG_BRIDGE(\"Exceptional exit of: %s\", __PRETTY_FUNCTION__);\n" +
                "        env->ExceptionDescribe();\n"+
                "        env->ExceptionClear();\n" +
                "        return").append(Utils.getFailureReturnForType(returnType)).append(";\n" +
                "    }\n\n");

    }

    private StringBuilder getArgumentMarshalling(Class<?>[] argumentTypes) {
        StringBuilder argumentContent = new StringBuilder();

        // If we have >2 arguments, use the jvalue[] calling approach.
        argumentContent.append(", ");
        if (argumentTypes.length > 2) {
            wrapperMethodBodies.append("    jvalue args[").append(argumentTypes.length).append("];\n");
            for (int aT = 0; aT < argumentTypes.length; aT++) {
                wrapperMethodBodies.append("    args[").append(aT).append("].")
                                   .append(Utils.getArrayArgumentMashallingLine(argumentTypes[aT], "a" + aT));
            }

            // The only argument is the array of arguments.
            argumentContent.append("args");
            wrapperMethodBodies.append('\n');
        } else {
            // Otherwise, use the vanilla calling approach.
            boolean needsNewline = false;
            for (int aT = 0; aT < argumentTypes.length; aT++) {
                // If the argument is a string-esque type, create a jstring from it, otherwise
                // it can be passed directly.
                if (Utils.isCharSequence(argumentTypes[aT])) {
                    wrapperMethodBodies.append("    jstring j").append(aT).append(" = AndroidBridge::NewJavaString(env, a").append(aT).append(");\n");
                    needsNewline = true;
                    // Ensure we refer to the newly constructed Java string - not to the original
                    // parameter to the wrapper function.
                    argumentContent.append('j').append(aT);
                } else {
                    argumentContent.append('a').append(aT);
                }
                if (aT != argumentTypes.length - 1) {
                    argumentContent.append(", ");
                }
            }
            if (needsNewline) {
                wrapperMethodBodies.append('\n');
            }
        }

        return argumentContent;
    }

    private void writeCtorBody(String implementationSignature, Constructor theCtor, boolean aIsThreaded) {
        Class<?>[] argumentTypes = theCtor.getParameterTypes();

        writeFunctionStartupBoilerPlate(implementationSignature, Void.class, false, aIsThreaded);

        writeFramePushBoilerplate(theCtor, false);

        // Marshall arguments for this constructor, if any...
        boolean hasArguments = argumentTypes.length != 0;

        StringBuilder argumentContent = new StringBuilder();
        if (hasArguments) {
            argumentContent = getArgumentMarshalling(argumentTypes);
        }

        // The call into Java
        wrapperMethodBodies.append("    Init(env->NewObject");
        if (argumentTypes.length > 2) {
            wrapperMethodBodies.append('A');
        }

        wrapperMethodBodies.append('(');


        // Call takes class id, method id of constructor method, then arguments.
        wrapperMethodBodies.append(Utils.getClassReferenceName(mClassToWrap)).append(", ");

        wrapperMethodBodies.append(mMembersToIds.get(theCtor))
        // Tack on the arguments, if any..
                           .append(argumentContent)
                           .append("), env);\n" +
                                   "    env->PopLocalFrame(NULL);\n" +
                                   "}\n");
    }

    /**
     * Generates the method body of the C++ wrapper function for the Java method indicated.
     *
     * @param methodSignature The previously-generated C++ method signature for the method to be
     *                        generated.
     * @param aCMethodName    The C++ method name for the method to be generated.
     * @param aMethod         The Java method to be wrapped by the C++ method being generated.
     * @param aClass          The Java class to which the method belongs.
     */
    private void writeMethodBody(String methodSignature, String aCMethodName, Method aMethod, Class<?> aClass, boolean aIsStaticBridgeMethod, boolean aIsMultithreaded) {
        Class<?>[] argumentTypes = aMethod.getParameterTypes();
        Class<?> returnType = aMethod.getReturnType();

        writeFunctionStartupBoilerPlate(methodSignature, returnType, aIsStaticBridgeMethod, aIsMultithreaded);

        boolean isObjectReturningMethod = !returnType.getCanonicalName().equals("void") && Utils.isObjectType(returnType);

        writeFramePushBoilerplate(aMethod, isObjectReturningMethod);

        // Marshall arguments, if we have any.
        boolean hasArguments = argumentTypes.length != 0;

        // We buffer the arguments to the call separately to avoid needing to repeatedly iterate the
        // argument list while building this line. In the coming code block, we simultaneously
        // construct any argument marshalling code (Creation of jstrings, placement of arguments
        // into an argument array, etc. and the actual argument list passed to the function (in
        // argumentContent).
        StringBuilder argumentContent = new StringBuilder();
        if (hasArguments) {
            argumentContent = getArgumentMarshalling(argumentTypes);
        }

        // Allocate a temporary variable to hold the return type from Java.
        wrapperMethodBodies.append("    ");
        if (!returnType.getCanonicalName().equals("void")) {
            if (isObjectReturningMethod) {
                wrapperMethodBodies.append("jobject");
            } else {
                wrapperMethodBodies.append(Utils.getCReturnType(returnType));
            }
            wrapperMethodBodies.append(" temp = ");
        }

        boolean isStaticJavaMethod = Utils.isMemberStatic(aMethod);

        // The call into Java
        wrapperMethodBodies.append("env->")
                           .append(Utils.getCallPrefix(returnType, isStaticJavaMethod));
        if (argumentTypes.length > 2) {
            wrapperMethodBodies.append('A');
        }

        wrapperMethodBodies.append('(');
        // If the underlying Java method is nonstatic, we provide the target object to the JNI.
        if (!isStaticJavaMethod) {
            wrapperMethodBodies.append("wrapped_obj, ");
        } else {
            // If this is a static underlying Java method, we need to use the class reference in our
            // call.
            wrapperMethodBodies.append(Utils.getClassReferenceName(aClass)).append(", ");
        }

        wrapperMethodBodies.append(mMembersToIds.get(aMethod));

        // Tack on the arguments, if any..
        wrapperMethodBodies.append(argumentContent)
                           .append(");\n\n");

        // Check for exception and return the failure value..
        wrapperMethodBodies.append("    if (env->ExceptionCheck()) {\n" +
                                   "        ALOG_BRIDGE(\"Exceptional exit of: %s\", __PRETTY_FUNCTION__);\n" +
                                   "        env->ExceptionDescribe();\n" +
                                   "        env->ExceptionClear();\n" +
                                   "        env->PopLocalFrame(NULL);\n" +
                                   "        return").append(Utils.getFailureReturnForType(returnType)).append(";\n" +
                                   "    }\n\n");

        // If we're returning an object, pop the callee's stack frame extracting our ref as the return
        // value.
        if (isObjectReturningMethod) {
            wrapperMethodBodies.append("    ")
                               .append(Utils.getCReturnType(returnType))
                               .append(" ret = static_cast<").append(Utils.getCReturnType(returnType)).append(">(env->PopLocalFrame(temp));\n" +
                                       "    return ret;\n");
        } else if (!returnType.getCanonicalName().equals("void")) {
            // If we're a primitive-returning function, just return the directly-obtained primative
            // from the call to Java.
            wrapperMethodBodies.append("    env->PopLocalFrame(NULL);\n" +
                                       "    return temp;\n");
        } else {
            // If we don't return anything, just pop the stack frame and move on with life.
            wrapperMethodBodies.append("    env->PopLocalFrame(NULL);\n");
        }
        wrapperMethodBodies.append("}\n");
    }

    /**
     * Generates the code to get the id of the given member on startup.
     *
     * @param aMember         The Java member being wrapped.
     */
    private void writeStartupCode(Member aMember) {
        wrapperStartupCode.append("    ")
                          .append(mMembersToIds.get(aMember))
                          .append(" = get");
        if (Utils.isMemberStatic(aMember)) {
            wrapperStartupCode.append("Static");
        }

        boolean isField = aMember instanceof Field;
        if (isField) {
            wrapperStartupCode.append("Field(\"");
        } else {
            wrapperStartupCode.append("Method(\"");
        }
        if (aMember instanceof Constructor) {
            wrapperStartupCode.append("<init>");
        } else {
            wrapperStartupCode.append(aMember.getName());
        }

        wrapperStartupCode.append("\", \"")
                          .append(Utils.getTypeSignatureStringForMember(aMember))
                          .append("\");\n");
    }

    private void writeZeroingFor(Member aMember, final String aMemberName) {
        if (aMember instanceof Field) {
            zeroingCode.append("jfieldID ");
        } else {
            zeroingCode.append("jmethodID ");
        }
        zeroingCode.append(mCClassName)
                   .append("::")
                   .append(aMemberName)
                   .append(" = 0;\n");
    }

    /**
     * Write the field declaration for the C++ id field of the given member.
     *
     * @param aMember Member for which an id field needs to be generated.
     */
    private void writeMemberIdField(Member aMember, final String aCMethodName) {
        String memberName = 'j'+ aCMethodName;

        if (aMember instanceof Field) {
            headerProtected.append("    static jfieldID ");
        } else {
            headerProtected.append("    static jmethodID ");
        }

        while(mTakenMemberNames.contains(memberName)) {
            memberName = 'j' + aCMethodName + mNameMunger;
            mNameMunger++;
        }

        writeZeroingFor(aMember, memberName);
        mMembersToIds.put(aMember, memberName);
        mTakenMemberNames.add(memberName);

        headerProtected.append(memberName)
                       .append(";\n");
    }

    /**
     * Helper function to add a provided method signature to the public section of the generated header.
     *
     * @param aSignature The header to add.
     */
    private void writeSignatureToHeader(String aSignature) {
        headerPublic.append("    ")
                    .append(aSignature)
                    .append(";\n");
    }

    /**
     * Get the finalised bytes to go into the generated wrappers file.
     *
     * @return The bytes to be written to the wrappers file.
     */
    public String getWrapperFileContents() {
        wrapperStartupCode.append("}\n");
        zeroingCode.append(wrapperStartupCode)
                   .append(wrapperMethodBodies);
        wrapperMethodBodies.setLength(0);
        wrapperStartupCode.setLength(0);
        return zeroingCode.toString();
    }

    /**
     * Get the finalised bytes to go into the generated header file.
     *
     * @return The bytes to be written to the header file.
     */
    public String getHeaderFileContents() {
        if (!mHasEncounteredDefaultConstructor) {
            headerPublic.append("    ").append(mCClassName).append("() : AutoGlobalWrappedJavaObject() {};\n");
        }
        headerProtected.append("};\n\n");
        headerPublic.append(headerProtected);
        headerProtected.setLength(0);
        return headerPublic.toString();
    }
}
