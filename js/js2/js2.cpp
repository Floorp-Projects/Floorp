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

#if 1
#define DEBUGGER_FOO
#define INTERPRET_INPUT
#else
#undef DEBUGGER_FOO
#undef INTERPRET_INPUT
#endif

#include <assert.h>

#include "world.h"
#include "interpreter.h"
#include "icodegenerator.h"

#ifdef DEBUGGER_FOO
#include "debugger.h"
#endif
        
#if defined(XP_MAC) && !defined(XP_MAC_MPW)
#include <SIOUX.h>
#include <MacTypes.h>

static char *mac_argv[] = {"js2", 0};

static void initConsole(StringPtr consoleName,
                        const char* startupMessage,
                        int &argc, char **&argv)
{
    SIOUXSettings.autocloseonquit = false;
    SIOUXSettings.asktosaveonclose = false;
    SIOUXSetTitle(consoleName);

    // Set up a buffer for stderr (otherwise it's a pig).
    static char buffer[BUFSIZ];
    setvbuf(stderr, buffer, _IOLBF, BUFSIZ);

    JavaScript::stdOut << startupMessage;

    argc = 1;
    argv = mac_argv;
}

#endif

namespace JavaScript {
namespace Shell {
    
using namespace ICG;
using namespace JSTypes;
using namespace Interpreter;

// Interactively read a line from the input stream in and put it into
// s. Return false if reached the end of input before reading anything.
static bool promptLine(LineReader &inReader, string &s, const char *prompt)
{
    if (prompt) {
        stdOut << prompt;
	  #ifdef XP_MAC_MPW
        // Print a CR after the prompt because MPW grabs the entire
        // line when entering an interactive command.
        stdOut << '\n';
	  #endif
    }
    return inReader.readLine(s) != 0;
}

World world;

/* "filename" of the console */
const String ConsoleName = widenCString("<console>");
const bool showTokens = false;

#ifdef DEBUGGER_FOO    
Reader *sourceReader; /* Reader for console file */

static
const Reader *ResolveFile (const String& fileName)
{
    if (fileName == ConsoleName)
        return sourceReader;
    else
    {
        stdErr << "Could not locate source for file '" << fileName << "'\n";
        return 0;
    }
    
}

JavaScript::Debugger::Shell jsd(world, stdin, JavaScript::stdOut,
                                JavaScript::stdOut, &ResolveFile);
#endif

static JSValue print(Context *, const JSValues &argv)
{
    size_t n = argv.size();
    if (n > 1) {                // the 'this' parameter is un-interesting
        stdOut << argv[1];
        for (size_t i = 2; i < n; ++i)
            stdOut << ' ' << argv[i];
    }
    stdOut << "\n";
    return kUndefinedValue;
}

static JSValue dump(Context *, const JSValues &argv)
{
    size_t n = argv.size();
    if (n > 1) {                // the 'this' parameter is un-interesting
        if (argv[1].isFunction()) {
            JSFunction *f = static_cast<JSFunction *>(argv[1].function);
            if (f->isNative())
                stdOut << "Native function";
            else
                stdOut << *f->getICode();
        }
        else
            stdOut << "Not a function";
    }
    stdOut << "\n";
    return kUndefinedValue;
}


inline char narrow(char16 ch) { return char(ch); }

static JSValue load(Context *cx, const JSValues &argv)
{

    JSValue result;
    size_t n = argv.size();
    if (n > 1) {
        for (size_t i = 1; i < n; ++i) {
            JSValue val = argv[i].toString();
            if (val.isString()) {
                String fileName(*val.string);
                std::string str(fileName.length(), char());
                std::transform(fileName.begin(), fileName.end(), str.begin(), narrow);
                FILE* f = fopen(str.c_str(), "r");
                if (f) {
                    result = cx->readEvalFile(f, fileName);
                    fclose(f);
                }
            }
        }
    }
    return result;
}

#if 0       // need a XP version of this, rip off from Monkey?
#include <sys/timeb.h>
static JSValue time(Context *cx, const JSValues &argv)
{
    struct _timeb timebuffer;
    _ftime(&timebuffer);

    return JSValue((double)timebuffer.time * 1000 + timebuffer.millitm);
}
#endif

/**
 * Poor man's instruction tracing facility.
 */
class Tracer : public Context::Listener {
    typedef InstructionStream::difference_type InstructionOffset;
    void listen(Context* context, Context::Event event)
    {
        if (event & Context::EV_STEP) {
            ICodeModule *iCode = context->getICode();
            JSValues &registers = context->getRegisters();
            InstructionIterator pc = context->getPC();
            
            
            InstructionOffset offset = (pc - iCode->its_iCode->begin());
            printFormat(stdOut, "trace [%02u:%04u]: ",
                        iCode->mID, offset);

            Instruction* i = *pc;
            stdOut << *i;
            if (i->op() != BRANCH && i->count() > 0) {
                stdOut << " [";
                i->printOperands(stdOut, registers);
                stdOut << "]\n";
            } else {
                stdOut << '\n';
            }
        }
    }
};


static void readEvalPrint(FILE *in, World &world)
{
    JSScope global;
    Context cx(world, &global);
#ifdef DEBUGGER_FOO
    jsd.attachToContext (&cx);
#endif
    global.defineNativeFunction(world.identifiers["print"], print);
    global.defineNativeFunction(world.identifiers["dump"], dump);
    global.defineNativeFunction(world.identifiers["load"], load);
//   global.defineNativeFunction(world.identifiers["time"], time);

    String buffer;
    string line;
    LineReader inReader(in);

#ifdef HAVE_GEORGE_TRACE_IT
    Tracer *george = new Tracer();
    cx.addListener(george);
#endif
    
    while (promptLine(inReader, line, buffer.empty() ? "js> " : "> ")) {
        appendChars(buffer, line.data(), line.size());
        try {
            Arena a;
            Parser p(world, a, buffer, ConsoleName);
                
            if (showTokens) {
                Lexer &l = p.lexer;
                while (true) {
                    const Token &t = l.get(true);
                    if (t.hasKind(Token::end))
                        break;
                    stdOut << ' ';
                    t.print(stdOut, true);
                }
	            stdOut << '\n';
            } else {
                StmtNode *parsedStatements = p.parseProgram();
				ASSERT(p.lexer.peek(true).hasKind(Token::end));
                {
                	PrettyPrinter f(stdOut, 30);
                	{
                		PrettyPrinter::Block b(f, 2);
	                	f << "Program =";
	                	f.linearBreak(1);
	                	StmtNode::printStatements(f, parsedStatements);
                	}
                	f.end();
                }
        	    stdOut << '\n';
#ifdef INTERPRET_INPUT
#ifdef DEBUGGER_FOO
                sourceReader = &(p.lexer.reader);
#endif
				// Generate code for parsedStatements, which is a linked 
                // list of zero or more statements
                ICodeModule* icm = cx.genCode(parsedStatements, ConsoleName);
                if (icm) {
#ifdef SHOW_ICODE
                    stdOut << *icm;
#endif
                    JSValue result = cx.interpret(icm, JSValues());
                    stdOut << "result = " << result << "\n";
                    delete icm;
                }
#endif
            }
            clear(buffer);
        } catch (Exception &e) {
            /* If we got a syntax error on the end of input,
             * then wait for a continuation
             * of input rather than printing the error message. */
            if (!(e.hasKind(Exception::syntaxError) &&
                  e.lineNum && e.pos == buffer.size() &&
                  e.sourceFile == ConsoleName)) {
                stdOut << '\n' << e.fullMessage();
                clear(buffer);
            }
        }
    }
    stdOut << '\n';
}



char * tests[] = {
    "function fact(n) { if (n > 1) return n * fact(n-1); else return 1; } print(fact(6), \" should be 720\"); return;" ,
    "a = { f1: 1, f2: 2}; print(a.f2++, \" should be 2\"); print(a.f2 <<= 1, \" should be 6\"); return;" ,
    "class A { static var b = 3; static function s() { return b++; }function x() { return \"Ax\"; } function y() { return \"Ay\"; } }  var a:A = new A; print(a.s(), \" should be 3\"); print(A.b, \" should be 4\"); return;",
    "class B extends A { function x() { return \"Bx\"; }  }  var b:B = new B; print(b.x(), \" should be Bx\"); print(b.y(), \" should be Ay\"); return;"
};

static void testCompile()
{
    JSScope glob;
    Context cx(world, &glob);
    glob.defineNativeFunction(world.identifiers["print"], print);
    for (uint i = 0; i < sizeof(tests) / sizeof(char *); i++) {
        String testScript = widenCString(tests[i]);
        Arena a;
        Parser p(world, a, testScript, widenCString("testCompile"));
        StmtNode *parsedStatements = p.parseProgram();
        ICodeGenerator icg(&world, &glob);
        StmtNode *s = parsedStatements;
        while (s) {
            icg.genStmt(s);
            s = s->next;
        }
        cx.interpret(icg.complete(&Void_Type), JSValues());
    }
}


} /* namespace Shell */
} /* namespace JavaScript */


#if defined(XP_MAC) && !defined(XP_MAC_MPW)
int main(int argc, char **argv)
{
    initConsole("\pJavaScript Shell", "Welcome to the js2 shell.\n", argc, argv);
#else
int main(int , char **)
{
#endif

    using namespace JavaScript;
    using namespace Shell;
#if 1
    testCompile();
#endif
    readEvalPrint(stdin, world);
    return 0;
    // return ProcessArgs(argv + 1, argc - 1);
}
