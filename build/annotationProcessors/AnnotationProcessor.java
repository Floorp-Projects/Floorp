/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors;

import org.mozilla.gecko.annotationProcessors.classloader.IterableJarLoadingURLClassLoader;
import org.mozilla.gecko.annotationProcessors.utils.GeneratableEntryPointIterator;

import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Arrays;
import java.util.Iterator;

public class AnnotationProcessor {
    public static final String OUTFILE = "GeneratedJNIWrappers.cpp";
    public static final String HEADERFILE = "GeneratedJNIWrappers.h";

    public static void main(String[] args) {
        // We expect a list of jars on the commandline. If missing, whinge about it.
        if (args.length <= 1) {
            System.err.println("Usage: java AnnotationProcessor jarfiles ...");
            System.exit(1);
        }

        System.out.println("Processing annotations...");

        // We want to produce the same output as last time as often as possible. Ordering of
        // generated statements, therefore, needs to be consistent.
        Arrays.sort(args);

        // Start the clock!
        long s = System.currentTimeMillis();

        // Get an iterator over the classes in the jar files given...
        Iterator<Class<?>> jarClassIterator = IterableJarLoadingURLClassLoader.getIteratorOverJars(args);

        CodeGenerator generatorInstance = new CodeGenerator();

        while (jarClassIterator.hasNext()) {
            Class<?> aClass = jarClassIterator.next();

            // Get an iterator over the appropriately generated methods of this class
            Iterator<MethodWithAnnotationInfo> methodIterator = new GeneratableEntryPointIterator(aClass.getDeclaredMethods());

            // Iterate all annotated methods in this class..
            while (methodIterator.hasNext()) {
                MethodWithAnnotationInfo aMethodTuple = methodIterator.next();
                generatorInstance.generateMethod(aMethodTuple, aClass);
            }

        }

        writeOutputFiles(generatorInstance);
        long e = System.currentTimeMillis();
        System.out.println("Annotation processing complete in " + (e - s) + "ms");
    }

    private static void writeOutputFiles(CodeGenerator aGenerator) {
        try {
            FileOutputStream outStream = new FileOutputStream(OUTFILE);
            outStream.write(aGenerator.getWrapperFileContents());
        } catch (IOException e) {
            System.err.println("Unable to write " + OUTFILE + ". Perhaps a permissions issue?");
            e.printStackTrace(System.err);
        }

        try {
            FileOutputStream headerStream = new FileOutputStream(HEADERFILE);
            headerStream.write(aGenerator.getHeaderFileContents());
        } catch (IOException e) {
            System.err.println("Unable to write " + OUTFILE + ". Perhaps a permissions issue?");
            e.printStackTrace(System.err);
        }
    }
}