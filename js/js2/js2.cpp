// -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// The contents of this file are subject to the Netscape Public
// License Version 1.1 (the "License"); you may not use this file
// except in compliance with the License. You may obtain a copy of
// the License at http://www.mozilla.org/NPL/
//
// Software distributed under the License is distributed on an "AS
// IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
// implied. See the License for the specific language governing
// rights and limitations under the License.
//
// The Original Code is the JavaScript 2 Prototype.
//
// The Initial Developer of the Original Code is Netscape
// Communications Corporation.  Portions created by Netscape are
// Copyright (C) 1998 Netscape Communications Corporation. All
// Rights Reserved.


//
// JS2 shell.
//

#include <assert.h>

#include "world.h"
#include "interpreter.h"

namespace JS = JavaScript;
using namespace JavaScript;

#if defined(XP_MAC) && !defined(XP_MAC_MPW)
#include <SIOUX.h>
#include <MacTypes.h>

static char *mac_argv[] = {"js2", 0};

static void initConsole(StringPtr consoleName, const char* startupMessage, int &argc, char **&argv)
{
    SIOUXSettings.autocloseonquit = false;
    SIOUXSettings.asktosaveonclose = false;
    SIOUXSetTitle(consoleName);
    
    // Set up a buffer for stderr (otherwise it's a pig).
    static char buffer[BUFSIZ];
    setvbuf(stderr, buffer, _IOLBF, BUFSIZ);

    stdOut << startupMessage;

    argc = 1;
    argv = mac_argv;
}
#endif


// Interactively read a line from the input stream in and put it into s.
// Return false if reached the end of input before reading anything.
static bool promptLine(LineReader &inReader, string &s, const char *prompt)
{
	if (prompt) {
	    stdOut << prompt;
	  #ifdef XP_MAC_MPW
	    // Print a CR after the prompt because MPW grabs the entire line when entering an interactive command.
	    stdOut << '\n';
	  #endif
	}
	return inReader.readLine(s) != 0;
}


const bool showTokens = true;

static void readEvalPrint(FILE *in, World &world)
{
	String buffer;
	string line;
	String sourceLocation = widenCString("console");
	LineReader inReader(in);

	while (promptLine(inReader, line, buffer.empty() ? "js> " : 0)) {
		appendChars(buffer, line.data(), line.size());
		try {
			Arena a;
			Parser p(world, a, buffer, sourceLocation);

			if (showTokens) {
				Lexer &l = p.lexer;
				while (true) {
					const Token &t = l.get(true);
					if (t.hasKind(Token::end))
						break;
					stdOut << ' ';
					t.print(stdOut, true);
				}
			} else {
				ExprNode *parseTree = p.parsePostfixExpression();
			}
			clear(buffer);
			stdOut << '\n';
		} catch (Exception &e) {
			// If we got a syntax error on the end of input, then wait for a continuation
			// of input rather than printing the error message.
			if (!(e.hasKind(Exception::syntaxError) && e.lineNum && e.pos == buffer.size() &&
				  e.sourceFile == sourceLocation)) {
				stdOut << '\n' << e.fullMessage();
				clear(buffer);
			}
		}
	}
    stdOut << '\n';
#if 0	
    do {
        bufp = buffer;
        *bufp = '\0';

        /*
         * Accumulate lines until we get a 'compilable unit' - one that either
         * generates an error (before running out of source) or that compiles
         * cleanly.  This should be whenever we get a complete statement that
         * coincides with the end of a line.
         */
        startline = lineNum;
        do {
            if (!GetLine(cx, bufp, file, startline == lineNum ? "js> " : "")) {
                hitEOF = JS_TRUE;
                break;
            }
            bufp += strlen(bufp);
            lineNum++;
        } while (!JS_BufferIsCompilableUnit(cx, obj, buffer, strlen(buffer)));
    } while (!hitEOF);
    fprintf(stdout, "\n");
    return;
#endif
}


#if 0	
static int ProcessInputFile(JSContext *cx, JSObject *obj, char *filename)
{
    JSBool ok, hitEOF;
    JSScript *script;
    jsval result;
    JSString *str;
    char buffer[4096];
    char *bufp;
    int startline;
    FILE *file;

    if (filename && strcmp(filename, "-")) {
        file = fopen(filename, "r");
        if (!file) {
            fprintf(stderr, "Can't open \"%s\": %s", filename, strerror(errno));
            return 1;
        }
    } else {
        file = stdin;
    }

    if (!isatty(fileno(file))) {
        /*
         * It's not interactive - just execute it.
         *
         * Support the UNIX #! shell hack; gobble the first line if it starts with '#'.
         */
        int ch = fgetc(file);
        if (ch == '#') {
            while((ch = fgetc(file)) != EOF)
                if (ch == '\n' || ch == '\r')
                    break;
        } else
            ungetc(ch, file);
        script = JS_CompileFileHandle(cx, obj, filename, file);
        if (script)
            (void)JS_ExecuteScript(cx, obj, script, &result);
        return;
    }

    /* It's an interactive filehandle; drop into read-eval-print loop. */
    int32 lineNum = 1;
    hitEOF = JS_FALSE;
    do {
        bufp = buffer;
        *bufp = '\0';

        /*
         * Accumulate lines until we get a 'compilable unit' - one that either
         * generates an error (before running out of source) or that compiles
         * cleanly.  This should be whenever we get a complete statement that
         * coincides with the end of a line.
         */
        startline = lineNum;
        do {
            if (!GetLine(cx, bufp, file, startline == lineNum ? "js> " : "")) {
                hitEOF = JS_TRUE;
                break;
            }
            bufp += strlen(bufp);
            lineNum++;
        } while (!JS_BufferIsCompilableUnit(cx, obj, buffer, strlen(buffer)));
    } while (!hitEOF);
    fprintf(stdout, "\n");
    return;
}


static int
usage(void)
{
    stdErr << "usage: js [-s] [-w] [-v version] [-f scriptfile] [scriptfile] [scriptarg...]\n";
    return 2;
}


static int
ProcessArgs(char **argv, int argc)
{
    int i;
    char *filename = NULL;
    jsint length;
    jsval *vector;
    jsval *p;
    JSObject *argsObj;
    JSBool isInteractive = JS_TRUE;

    for (i=0; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
              case 'f':
                if (i+1 == argc) {
                    return usage();
                }
                filename = argv[i+1];
                /* "-f -" means read from stdin */
                if (filename[0] == '-' && filename[1] == '\0')
                    filename = NULL;
                ProcessInputFile(filename);
                filename = NULL;
                /* XXX: js -f foo.js should interpret foo.js and then
                 * drop into interactive mode, but that breaks test
                 * harness. Just execute foo.js for now.
                 */
                isInteractive = JS_FALSE;
                i++;
                break;

              default:
                return usage();
            }
        } else {
            filename = argv[i++];
            isInteractive = JS_FALSE;
            break;
        }
    }

    if (filename || isInteractive)
        ProcessInputFile(filename);
    return gExitCode;
}
#endif

#include "icodegenerator.h"

static void testICG(World &world)
{
    //
    // testing ICG 
    //
    uint32 pos = 0;
    ICodeGenerator icg;

    // var i,j; i = j + 2;
    // i is bound to var #0, j to var #1

    icg.beginStatement(pos);
    Register r1 = icg.loadVariable(1);
    Register r2 = icg.loadImmediate(2.0);
    icg.saveVariable(0, icg.op(ADD, r1, r2));

    // j = a.b
    icg.beginStatement(pos);
    Register r4 = icg.loadName(world.identifiers[widenCString("a")]);
    Register r5 = icg.getProperty(r4, world.identifiers[widenCString("b")]);
    icg.saveVariable(1, r5);
    
    // while (i) i = i + j;
    icg.beginWhileStatement(pos);
    r1 = icg.loadVariable(0);
    icg.endWhileExpression(r1);        
    icg.saveVariable(0, icg.op(ADD, icg.loadVariable(0), icg.loadVariable(1)));
    icg.endWhileStatement();

    // if (i) if (j) i = 3; else j = 4;
    r1 = icg.loadVariable(0);   
    icg.beginIfStatement(pos, r1);
        r2 = icg.loadVariable(1);
        icg.beginIfStatement(pos, r2);
        icg.saveVariable(0, icg.loadImmediate(3));
        icg.beginElseStatement(true);
        icg.saveVariable(1, icg.loadImmediate(4));
        icg.endIfStatement();
    icg.beginElseStatement(false);
    icg.endIfStatement();


    // switch (i) { case 3: case 4: j = 4; break; case 5: j = 5; break; default : j = 6; }
    r1 = icg.loadVariable(0);   
    icg.beginSwitchStatement(pos, r1);
        // case 3, note empty case statement (?necessary???)
        icg.endCaseCondition(icg.loadImmediate(3));
            icg.beginCaseStatement();
            icg.endCaseStatement();
        // case 4
        icg.endCaseCondition(icg.loadImmediate(4));
            icg.beginCaseStatement();
            icg.beginStatement(pos);
            icg.saveVariable(1, icg.loadImmediate(4));
            icg.breakStatement();
            icg.endCaseStatement();
        // case 5
        icg.endCaseCondition(icg.loadImmediate(5));
            icg.beginCaseStatement();
            icg.beginStatement(pos);
            icg.saveVariable(1, icg.loadImmediate(5));
            icg.breakStatement();
            icg.endCaseStatement();
        // default
            icg.beginDefaultStatement();
            icg.beginStatement(pos);
            icg.saveVariable(1, icg.loadImmediate(6));
            icg.endDefaultStatement();
    icg.endSwitchStatement();

    // for ( ; i; i + 1 ) j = 99;
    icg.beginForStatement(pos);
        r1 = icg.loadVariable(0);
        icg.forCondition(r1);
        icg.saveVariable(0, icg.op(ADD, icg.loadVariable(0), icg.loadImmediate(1)));
        icg.forIncrement();
        icg.saveVariable(0, icg.loadImmediate(99));
    icg.endForStatement();

    ICodeModule *icm = icg.complete();

    std::cout << icg;

    delete icm;
}

static float64 testFunctionCall(float64 n)
{
    uint32 position = 0;
    ICodeGenerator icg;

    // function sum(n) { if (n > 1) return 1 + sum(n - 1); else return 1; }
    // n is bound to var #0.
    icg.beginStatement(position);
    icg.loadVariable(0);

    return 0.0;    
}

static float64 testFactorial(float64 n)
{
	// generate code for factorial, and interpret it.
    uint32 position = 0;
    ICodeGenerator icg;

	// fact(n) {
	// n is bound to var #0.
	// var result = 1;
	// result is bound to var #1.
	icg.beginStatement(position);
	icg.saveVariable(1, icg.loadImmediate(1.0));

	// while (n > 1) {
	//   result = result * n;
	//   n = n - 1;
	// }
	{
	    icg.beginWhileStatement(position);
		Register r0 = icg.loadVariable(0);
		Register r1 = icg.loadImmediate(1.0);
		Register r2 = icg.op(COMPARE_GT, r0, r1);
	    icg.endWhileExpression(r2);
	    r0 = icg.loadVariable(0);
	    r1 = icg.loadVariable(1);
	    r2 = icg.op(MULTIPLY, r1, r0);
	    icg.saveVariable(1, r2);
	    icg.beginStatement(position);
	    r0 = icg.loadVariable(0);
	    r1 = icg.loadImmediate(1.0); 
	    r2 = icg.op(SUBTRACT, r0, r1);
	    icg.saveVariable(0, r2);
	    icg.endWhileStatement();
	}
	
	// return result;
    icg.returnStatement(icg.loadVariable(1));
    ICodeModule *icm = icg.complete();
    std::cout << icg;

    // test the iCode interpreter.
    JSValues args(1);
    args[0] = JSValue(n);
    JSValue result = interpret(icm, args);
    std::cout << "fact(" << n << ") = " << result.f64 << std::endl;
    
    delete icm;

    return result.f64;
}

static float64 testObjects(World &world, int32 n)
{
    // create some objects, put some properties, and retrieve them.
    uint32 position = 0;
    ICodeGenerator initCG;
    
    // var global = new Object();
    StringAtom& global = world.identifiers[widenCString("global")];
    initCG.beginStatement(position);
    initCG.saveName(global, initCG.newObject());
    
    // global.counter = 0;
    StringAtom& counter = world.identifiers[widenCString("counter")];
    initCG.beginStatement(position);
    initCG.setProperty(initCG.loadName(global), counter, initCG.loadImmediate(0.0));

    // var array = new Array();
    StringAtom& array = world.identifiers[widenCString("array")];
    initCG.beginStatement(position);
    initCG.saveName(array, initCG.newArray());

    ICodeModule* initCode = initCG.complete();
    
    std::cout << initCG;

    // function increment()
    // {
    //   var i = global.counter;
    //   array[i] = i;
    //   return ++global.counter;
    // }
    ICodeGenerator incrCG;
    
    incrCG.beginStatement(position);
    Register robject = incrCG.loadName(global);
    Register roldvalue = incrCG.getProperty(robject, counter);
    Register rarray = incrCG.loadName(array);
    incrCG.setElement(rarray, roldvalue, roldvalue);
    Register rvalue = incrCG.op(ADD, roldvalue, incrCG.loadImmediate(1.0));
    incrCG.setProperty(robject, counter, rvalue);
    incrCG.returnStatement(rvalue);

    ICodeModule* incrCode = incrCG.complete();

    std::cout << incrCG;

    // run initialization code.
    JSValues args;
    interpret(initCode, args);

    // call the increment function some number of times.
    JSValue result;
    while (n-- > 0)
        result = interpret(incrCode, args);

    std::cout << "result = " << result.f64 << std::endl;

    delete initCode;
    delete incrCode;

    return result.f64;
}

int main(int argc, char **argv)
{
  #if defined(XP_MAC) && !defined(XP_MAC_MPW)
    initConsole("\pJavaScript Shell", "Welcome to the js2 shell.\n", argc, argv);
  #endif
	World world;
  #if 1
    assert(testFactorial(5) == 120);
    assert(testObjects(world, 5) == 5);
    testICG(world);
  #endif
	readEvalPrint(stdin, world);
    return 0;
    // return ProcessArgs(argv + 1, argc - 1);
}
