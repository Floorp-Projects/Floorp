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
#include "icodegenerator.h"

#include "debugger.h"
        
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


JavaScript::World world;
JavaScript::Debugger::Shell jsd(world, stdin, JavaScript::stdOut,
                                JavaScript::stdOut);

const bool showTokens = false;


static JSValue print(const JSValues &argv)
{
    size_t n = argv.size();
    if (n > 0) {
        stdOut << argv[0];
        for (size_t i = 1; i < n; ++i)
            stdOut << ' ' << argv[i];
    }
    stdOut << "\n";
    return kUndefinedValue;
}


static void genCode(World &world, Context &cx, StmtNode *p)
{
    ICodeGenerator icg(&world);
    Register ret = NotARegister;
    while (p) {
        ret = icg.genStmt(p);
        p = p->next;
    }
    icg.returnStmt(ret);
    stdOut << '\n';
    stdOut << icg;
    JSValue result = cx.interpret(icg.complete(), JSValues());
    stdOut << "result = " << result << "\n";
}

static void readEvalPrint(FILE *in, World &world)
{
    JSScope glob;
    Context cx(world, &glob);
    StringAtom& printName = world.identifiers[widenCString("print")];
    glob.defineNativeFunction(printName, print);

    String buffer;
    string line;
    String sourceLocation = widenCString("console");
    LineReader inReader(in);
        
    while (promptLine(inReader, line, buffer.empty() ? "js> " : "> ")) {
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
	            stdOut << '\n';
            } else {
                StmtNode *parsedStatements = p.parseProgram();
				ASSERT(p.lexer.peek(true).hasKind(Token::end));
                {
                	PrettyPrinter f(stdOut, 20);
                	{
                		PrettyPrinter::Block b(f, 2);
	                	f << "Program =";
	                	f.linearBreak(1);
	                	StmtNode::printStatements(f, parsedStatements);
                	}
                	f.end();
                }
        	    stdOut << '\n';
#if 0
				// Generate code for parsedStatements, which is a linked list of zero or more statements
                genCode(world, cx, parsedStatements);
#endif
            }
            clear(buffer);
        } catch (Exception &e) {
            /* If we got a syntax error on the end of input,
             * then wait for a continuation
             * of input rather than printing the error message. */
            if (!(e.hasKind(Exception::syntaxError) &&
                  e.lineNum && e.pos == buffer.size() &&
                  e.sourceFile == sourceLocation)) {
                stdOut << '\n' << e.fullMessage();
                clear(buffer);
            }
        }
    }
    stdOut << '\n';
}


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


static float64 testFactorial(World &world, float64 n)
{
    JSScope glob;
    Context cx(world, &glob);
    // generate code for factorial, and interpret it.
    uint32 pos = 0;
    ICodeGenerator icg(&world);
        
    // fact(n) {
    // var result = 1;

    StringAtom &n_name = world.identifiers[widenCString("n")];
    StringAtom &result_name = world.identifiers[widenCString("result")];

    Register r_n = icg.allocateParameter(n_name);
    Register r_result = icg.allocateVariable(result_name);

    Arena a;

    ExprStmtNode *e = new(a) ExprStmtNode(pos, StmtNode::expression, new(a) BinaryExprNode(pos, ExprNode::assignment, 
                                                                    new(a) IdentifierExprNode(pos, ExprNode::identifier, result_name),
                                                                    new(a) NumberExprNode(pos, 1.0) ) );
    icg.genStmt(e);

    // while (n > 1) {
    //   result = result * n;
    //   n = n - 1;
    // }
    {
        BinaryExprNode *c = new(a) BinaryExprNode(pos, ExprNode::greaterThan, 
                                                new(a) IdentifierExprNode(pos, ExprNode::identifier, n_name),
                                                new(a) NumberExprNode(pos, 1.0) ) ;
        ExprStmtNode *e1 = new(a) ExprStmtNode(pos, StmtNode::expression, new(a) BinaryExprNode(pos, ExprNode::assignment, 
                                                                        new(a) IdentifierExprNode(pos, ExprNode::identifier, result_name),
                                                                        new(a) BinaryExprNode(pos, ExprNode::multiply, 
                                                                                new(a) IdentifierExprNode(pos, ExprNode::identifier, result_name),
                                                                                new(a) IdentifierExprNode(pos, ExprNode::identifier, n_name) ) ) );
        ExprStmtNode *e2 = new(a) ExprStmtNode(pos, StmtNode::expression, new(a) BinaryExprNode(pos, ExprNode::assignment, 
                                                                        new(a) IdentifierExprNode(pos, ExprNode::identifier, n_name),
                                                                        new(a) BinaryExprNode(pos, ExprNode::subtract, 
                                                                                new(a) IdentifierExprNode(pos, ExprNode::identifier, n_name),
                                                                                new(a) NumberExprNode(pos, 1.0) ) ) );
        e1->next = e2;
        BlockStmtNode *b = new(a) BlockStmtNode(pos, StmtNode::block, NULL, e1);

        UnaryStmtNode *w = new(a) UnaryStmtNode(pos, StmtNode::While, c, b);

        icg.genStmt(w);

    }
        
    // return result;
    icg.returnStmt(r_result);
    ICodeModule *icm = icg.complete();
    stdOut << icg;
        
    // preset the global property "fact" to contain the above function
    StringAtom& fact = world.identifiers[widenCString("fact")];
    glob.defineFunction(fact, icm);
        
    // now a script : 
    // return fact(n);
    ICodeGenerator script(&world);
    RegisterList args(1);
    args[0] = script.loadImmediate(n);
    script.returnStmt(script.call(script.loadName(fact), args));
    stdOut << script;
    
    // install a listener so we can trace execution of factorial.
    Tracer t;
    cx.addListener(&t);
    
    // test the iCode interpreter.
    JSValue result = cx.interpret(script.complete(), JSValues());
    stdOut << "fact(" << n << ") = " << result.f64 << "\n";
    
    delete icm;
        
    return result.f64;
}
    


} /* namespace Shell */
} /* namespace JavaScript */


int main(int argc, char **argv)
{
  #if defined(XP_MAC) && !defined(XP_MAC_MPW)
    initConsole("\pJavaScript Shell", "Welcome to the js2 shell.\n", argc, argv);
  #endif
    using namespace JavaScript;
    using namespace Shell;
  #if 0
    assert(testFactorial(world, 5) == 120);
  #endif
    readEvalPrint(stdin, world);
    return 0;
    // return ProcessArgs(argv + 1, argc - 1);
}
