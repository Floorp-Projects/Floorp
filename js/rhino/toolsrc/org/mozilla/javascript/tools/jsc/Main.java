/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Christine Begle
 * Norris Boyd
 * Roger Lawrence
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

package org.mozilla.javascript.tools.jsc;

import java.io.*;
import java.util.*;
import java.lang.reflect.*;
import java.text.MessageFormat;
import org.mozilla.javascript.*;
import org.mozilla.javascript.tools.ToolErrorReporter;

/**
 * @author Norris Boyd
 */
public class Main {

    /**
     * Main entry point.
     *
     * Process arguments as would a normal Java program. Also
     * create a new Context and associate it with the current thread.
     * Then set up the execution environment and begin to
     * compile scripts.
     */
    public static void main(String args[]) {
        Context cx = Context.enter();

        reporter = new ToolErrorReporter(true);

        cx.setErrorReporter(reporter);

        args = processOptions(cx, args);

        if ( ! reporter.hasReportedError())
            processSource(cx, args);

        cx.exit();
    }

    /**
     * Parse arguments.
     *
     */
    public static String[] processOptions(Context cx, String args[]) {
        ClassNameHelper nameHelper = ClassNameHelper.get(cx);
        nameHelper.setTargetPackage("");        // default to no package
        cx.setGeneratingDebug(false);   // default to no symbols
        for (int i=0; i < args.length; i++) {
            String arg = args[i];
            if (!arg.startsWith("-")) {
                String[] result = new String[args.length - i];
                for (int j=i; j < args.length; j++)
                    result[j-i] = args[j];
                return result;
            }
            try {
                if (arg.equals("-version") && ++i < args.length) {
                    int version = Integer.parseInt(args[i]);
                    cx.setLanguageVersion(version);
                    continue;
                }
                if ((arg.equals("-opt") || arg.equals("-O"))  &&
                    ++i < args.length)
                {
                    int optLevel = Integer.parseInt(args[i]);
                    cx.setOptimizationLevel(optLevel);
                    continue;
                }
            }
            catch (NumberFormatException e) {
                cx.reportError( ToolErrorReporter.getMessage("msg.jsc.usage",
                                                             args[i] ));
                continue;
            }
            if (arg.equals("-nosource")) {
                cx.setGeneratingSource(false);
                continue;
            }
            if (arg.equals("-debug") || arg.equals("-g")) {
                cx.setGeneratingDebug(true);
                continue;
            }
            if (arg.equals("-o") && ++i < args.length) {
                String outFile = args[i];
                if (!Character.isJavaIdentifierStart(outFile.charAt(0))) {
                    cx.reportError(ToolErrorReporter.getMessage(
                        "msg.invalid.classfile.name",
                        outFile));
                    continue;
                }
                for ( int j = 1; j < outFile.length(); j++ ) {
                    if ( (!Character.isJavaIdentifierPart(outFile.charAt(j)) &&
                    outFile.charAt(j) != '.') || (outFile.charAt(j) == '.' &&
                    (!outFile.endsWith(".class") || j != outFile.length()-6 )) )
                    {
                        cx.reportError(ToolErrorReporter.getMessage(
                            "msg.invalid.classfile.name",
                            outFile ));
                        break;
                    }
                }
                nameHelper.setTargetClassFileName(outFile);
                hasOutOption = true;
                continue;
            }
            if (arg.equals("-package") && ++i < args.length) {
                String targetPackage = args[i];
                for ( int j = 0; j < targetPackage.length(); j++ ) {
                    if ( !Character.isJavaIdentifierStart(targetPackage.charAt(j))) {
                        cx.reportError(ToolErrorReporter.getMessage(
                            "msg.package.name",
                            targetPackage));
                        continue;
                    }
                    for ( int k = ++j; k< targetPackage.length(); k++, j++ ) {
                        if (targetPackage.charAt(k) == '.' &&
                        targetPackage.charAt(k-1) != '.' &&
                        k != targetPackage.length()-1)
                        {
                            continue;
                        }
                        if (!Character.isJavaIdentifierPart(
                        targetPackage.charAt(k)))
                        {
                            cx.reportError(ToolErrorReporter.getMessage(
                                "msg.package.name",
                                targetPackage));
                            continue;
                        }
                        continue;
                    }
                }
                nameHelper.setTargetPackage(targetPackage);
                continue;
            }
            if (arg.equals("-extends") && ++i < args.length) {
                String targetExtends = args[i];
                try {
                    nameHelper.setTargetExtends(Class.forName(targetExtends));
                } catch (ClassNotFoundException e) {
                    throw new Error(e.toString()); // TODO: better error
                }
                continue;
            }
            if (arg.equals("-implements") && ++i < args.length) {
                // TODO: allow for multiple comma-separated interfaces.
                String targetImplements = args[i];
                StringTokenizer st = new StringTokenizer(targetImplements,
                                                         ",");
                Vector v = new Vector();
                while (st.hasMoreTokens()) {
                    String className = st.nextToken();
                    try {
                        v.addElement(Class.forName(className));
                    } catch (ClassNotFoundException e) {
                        throw new Error(e.toString()); // TODO: better error
                    }
                }
                Class[] implementsClasses = new Class[v.size()];
                v.copyInto(implementsClasses);
                nameHelper.setTargetImplements(implementsClasses);
                continue;
            }
            usage(arg);
        }
        // no file name
        p(ToolErrorReporter.getMessage("msg.no.file"));
        System.exit(1);
        return null;
    }
    /**
     * Print a usage message.
     */
    public static void usage(String s) {
        p(ToolErrorReporter.getMessage("msg.jsc.usage", s));
        System.exit(1);
    }

    /**
     * Compile JavaScript source.
     *
     */
    public static void processSource(Context cx, String[] filenames) {
        ClassNameHelper nameHelper = ClassNameHelper.get(cx);
        if (hasOutOption && filenames.length > 1) {
            cx.reportError(ToolErrorReporter.getMessage(
                "msg.multiple.js.to.file",
                nameHelper.getTargetClassFileName()));
        }
        for (int i=0; i < filenames.length; i++) {
            String filename = filenames[i];
            File f = new File(filename);

            if (!f.exists()) {
                cx.reportError(ToolErrorReporter.getMessage(
                    "msg.jsfile.not.found",
                    filename));
                return;
            }
            if (!filename.endsWith(".js")) {
                cx.reportError(ToolErrorReporter.getMessage(
                    "msg.extension.not.js",
                    filename));
                return;
            }
            if (!hasOutOption) {
                String name = f.getName();
                String nojs = name.substring(0, name.length() - 3);
                String className = getClassName(nojs) + ".class";
                String out = f.getParent() == null ? className : f.getParent() +
                             File.separator + className;
                nameHelper.setTargetClassFileName(out);
            }
            if (nameHelper.getTargetClassFileName() == null) {
                cx.reportError(ToolErrorReporter.getMessage("msg.no-opt"));
            }
            try {
                Reader in = new FileReader(filename);
                cx.compileReader(null, in, filename, 1, null);
            }
            catch (FileNotFoundException ex) {
                cx.reportError(ToolErrorReporter.getMessage(
                    "msg.couldnt.open",
                    filename));
                return;
            }
            catch (IOException ioe) {
                cx.reportError(ioe.toString());
            }
        }
    }

    /**
     * Verify that class file names are legal Java identifiers.  Substitute
     * illegal characters with underscores, and prepend the name with an
     * underscore if the file name does not begin with a JavaLetter.
     */

    static String getClassName(String name) {
        char[] s = new char[name.length()+1];
        char c;
        int j = 0;

        if (!Character.isJavaIdentifierStart(name.charAt(0))) {
            s[j++] = '_';
        }
        for (int i=0; i < name.length(); i++, j++) {
            c = name.charAt(i);
            if ( Character.isJavaIdentifierPart(c) ) {
                s[j] = c;
            } else {
                s[j] = '_';
            }
        }
        return (new String(s)).trim();
     }

    private static void p(String s) {
        System.out.println(s);
    }

    private static boolean hasOutOption = false;
    private static ToolErrorReporter reporter;
}

