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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mountain View Compiler
 * Company.  Portions created by Mountain View Compiler Company are
 * Copyright (C) 1998-2000 Mountain View Compiler Company. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Jeff Dyer <jeff@compilercompany.com>
 */

import com.compilercompany.ecmascript.*;
import sun.tools.util.CommandLine;
import java.io.*;

/*
 * The main driver.
 */

public class Main {

    String[] classes;

    /**
     * Entry point.
     */
    public static void main(String[] args) {
	/* Preprocess @file arguments */
	try {
	    args = CommandLine.parse(args);
	} catch (FileNotFoundException e) {
	    Util.error("at.args.cant.read", e.getMessage());
	} catch (IOException e) {
	    Util.error("at.args.io.exception", e.getMessage());
	}
	
	new Main(args).run();
	System.exit(0);
    }

    /**
     * Parse options.
     */

	private static boolean traceInput  = false;
    private static boolean traceLexer  = false;
	private static boolean traceParser = false;
	private static boolean debug       = false;


    public Main(String[] args) {
	if (args.length == 0) {
	    Util.usage(1);
	}
	
	/* Default values for options, overridden by user options. */

	int i = 0;
	for (; i < args.length; i++) {
	    if (args[i].equals("-i")||args[i].equals("-input")) {
		    traceInput = true;
	    } else if (args[i].equals("-l")||args[i].equals("-lexer")) {
		    traceLexer = true;
	    } else if (args[i].equals("-p")||args[i].equals("-parser")) {
		    traceParser = true;
	    } else if (args[i].equals("-d")||args[i].equals("-debug")) {
		    debug = true;
	    } else if (args[i].charAt(0) == '-') {
            Util.error("unknown.option", args[i], null, true);
	    } else {
		    break; /* The rest must be classes. */
	    }
	}

	/*
	 * Arrange for output destination.
	 */

/*
	if (odir != null && ofile != null)
	    Util.error("dir.file.mixed");
	if (odir != null)
	    ; //setOutDir(odir);
	if (ofile != null)
	    Debugger.setOutFile(ofile);
*/

	/* 
	 * Grab the rest of argv[] ... this must be the classes.
	 */
	classes = new String[args.length - i];
	System.arraycopy(args, i, classes, 0, args.length - i);
	
	if (classes.length == 0) {
	    Util.error("no.classes.specified");
	}

    }
    
    public void run() {
	try {
	    compile("",classes);
	} catch (Exception x) {
        x.printStackTrace();
    }
    }

    static final void compile(String name, String[] input) throws Exception {

        Debugger.trace( "begin testEvaluator: " + name );

        String result;
        Node node;
        Value type;
        Evaluator evaluator;
		Value value;

        Class pc = Parser.class;
        Class[] args = new Class[0];

        Parser parser;
        long t=0;

        ObjectValue global = null;

        for( int i = 0; i < input.length; i++ ) {

            try {

                if( traceInput ) {
                    InputBuffer.setOut(input[i]+".inp");
                } if( traceLexer ) {
                    Scanner.setOut(input[i]+".lex");
                } if( debug ) {
                    Debugger.setDbgFile( input[i]+".dbg" );
                } 

                Debugger.setErrFile( input[i]+".err" );

                FileInputStream srcfile = new FileInputStream( input[i] );
                InputStreamReader reader = new InputStreamReader( srcfile );
                
                Context context = new Context();
                global = new ObjectValue("__systemGlobal", new GlobalObject());
                context.pushScope(global);

                parser    = new Parser(reader);
                Evaluator cevaluator = new ConstantEvaluator();

                System.gc();
	            t = System.currentTimeMillis();
                node = parser.parseProgram();
                if( traceParser ) {
                    Debugger.trace("setting parser output to " + input[i]);
                    JSILGenerator.setOut( input[i]+".par" );
                    node.evaluate(context,new JSILGenerator());
                }

                //Evaluator evaluator;


                context.setEvaluator(new BlockEvaluator());
                node.evaluate(context, context.getEvaluator());

                JSILGenerator.setOut( input[i]+".blocks" );
                context.setEvaluator(new JSILGenerator());
                node.evaluate(context, context.getEvaluator());

                context.setEvaluator(new ConstantEvaluator());
                value = node.evaluate(context, context.getEvaluator());

                context.setEvaluator(new JSILGenerator());
                JSILGenerator.setOut( input[i]+".jsil" );
                node.evaluate(context, context.getEvaluator());

	            t = System.currentTimeMillis() - t;
                //Debugger.trace(""+global);
                System.out.println(input[i] + ": "+context.errorCount()+" errors [" + Long.toString(t) + " msec] --> " + value.getValue(context) );

            } catch( Exception x ) {
                x.printStackTrace();
	            t = System.currentTimeMillis() - t;
                System.out.println(input[i] + ": internal error" );
            }
            //Debugger.trace( "  " + i + " passed in " + Long.toString(t) + 
            //                    " msec: [" + input[i] + "] = " + result );

        }
    }
}

/*
 * The end.
 */
