/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Mike McCabe
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

/*
 * A quick and dirty driver for testing and timing the scanner, parser
 * and optimizer.
 */

package org.mozilla.javascript;

import java.io.*;
import java.lang.reflect.*;

class TestScan
{
    public static void main(String[] args) throws IOException {
        String file = null;
        String opt = "nt";
        int level = 0;

        if (args.length == 1) {
            file = args[0];
            opt = "-nt";
        }
        else if (args.length == 2) {
            file = args[1];
            opt = args[0];
        }
        else if (args.length == 3 && args[0].equals("-opt")) {
            level = Integer.parseInt(args[1]);
            file = args[2];
            opt = "-opt";
        }
        else if (args.length > 1 && args[0].equals("-compile")) {
            opt = "-compile";
            if (args.length == 3) {
                file = args[2];
                level = Integer.parseInt(args[1]);
            } else if (args.length == 2) {
                file = args[1];
            } else {
                usage();
                System.exit(1);
            }
        }
        else {
            usage();
            System.exit(1);
        }
        scan(opt, file, level);
    }

    public static void scan(String opt, String file, int level)
        throws IOException
    {
        FileReader in = null;

        // We might call scan from js, and so might not get an interned string.
        opt = opt.intern();
        try {
            in = new FileReader(file);
        }
        catch (FileNotFoundException ex) {
            System.out.println("couldn't open file " + file);
            System.exit(1);
        }

        Context cx = new Context();
        try {
            cx.enter();
        }
        catch (Throwable t) {
        }

        if (opt == "-compile") {
            cx.setOptimizationLevel(level);
            cx.compileReader(null, in, file, 1, null);
            return;
        }

        TokenStream ts = new TokenStream(in, null, file, 1);
        if (opt == "-scan") {
            int foo;
            while ((foo = ts.getToken()) != ts.EOF) {
                if (Context.printTrees)
                    System.out.println(ts.tokenToString(foo));
            }
        }
        else if (opt == "-ir") {
            IRFactory nf = new IRFactory(ts);
            Parser p = new Parser(nf);
            Node parsetree = (Node) p.parse(ts);
            System.out.print(parsetree.toStringTree());
        }
        else if (opt == "-nt") {
            IRFactory nf = new IRFactory(ts);
            NodeTransformer nt = new NodeTransformer();
            Parser p = new Parser(nf);
            Node parsetree = (Node) p.parse(ts);
            nt.transform(parsetree, null, ts);
            System.out.print(parsetree.toStringTree());
        }
        else if (opt == "-opt") {
            IRFactory nf = new IRFactory(ts);
            NodeTransformer nt = new NodeTransformer();
            Parser p = new Parser(nf);
            Node parsetree = (Node) p.parse(ts);
            nt.transform(parsetree, null, ts);
            cx.setOptimizationLevel(level);
            try {
                Class optimizerClass 
                    = Class.forName("org.mozilla.javascript.optimizer.Optimizer");
                Object theOptimizer = optimizerClass.newInstance();
                Class parameterTypes[] = { Node.class, Integer.class };
                Method runOptimizer = 
                        optimizerClass.getDeclaredMethod("optimize", parameterTypes);
                Object a[] = { parsetree, new Integer(level) };
                runOptimizer.invoke(theOptimizer, a);
                System.out.print(parsetree.toStringTree());
            }
            catch (Exception x) {
                System.out.println(x);
            }
        }            

        else {
            usage();
            return;
        }
    }

    static void usage() {
        System.err.println("usage: TestScan    " +
                           "[-compile [level] | -scan | -ir | -nt | -opt " +
                           "[level] ] filename.js");
        System.err.println("\t-compile [level] " +
                           "- compile the given file and return, for profiling");
        System.err.println("\t-scan            " +
                           "- list tokens");
        System.err.println("\t-ir              " +
                           "- print Internal Representation parse tree");
        System.err.println("\t-nt              " +
                           "- print NodeTransformed IR tree");
        System.err.println("\t-opt [level]     " +
                           "- print transformed and optimized IR tree");
        System.err.println();
        System.err.println("(note that nothing may be printed by the above if " +
                           "Context.printTrees is false.)");
    }
}
