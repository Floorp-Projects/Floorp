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
    
    // Interactively read a line from the input stream in and put it into
    // s Return false if reached the end of input before reading anything.
    static bool promptLine(LineReader &inReader, string &s,
                           const char *prompt)
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
                    /*ExprNode *parseTree = */ p.parsePostfixExpression();
                }
                clear(buffer);
                stdOut << '\n';
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
        
#include "icodegenerator.h"
        
    static void testICG(World &world)
    {
        //
        // testing ICG 
        //
        uint32 pos = 0;
        ICodeGenerator icg;
            
        // var i,j;
        // i is bound to var #0, j to var #1
        Register r_i = icg.allocateVariable(world.identifiers[widenCString("i")]);
        Register r_j = icg.allocateVariable(world.identifiers[widenCString("j")]);

        //  i = j + 2;
        icg.beginStatement(pos);
        Register r1 = icg.loadImmediate(2.0);
        icg.move(r_i, icg.op(ADD, r_i, r_j));
            
        // j = a.b
        icg.beginStatement(pos);
        r1 = icg.loadName(world.identifiers[widenCString("a")]);
        r1 = icg.getProperty(r1, world.identifiers[widenCString("b")]);
        icg.move(r_j, r1);
    
        // while (i) i = i + j;
        icg.beginWhileStatement(pos);
        icg.endWhileExpression(r_i);        
        icg.move(r_i, icg.op(ADD, r_i, r_j));
        icg.endWhileStatement();

        // if (i) if (j) i = 3; else j = 4;
        icg.beginIfStatement(pos, r_i);
        icg.beginIfStatement(pos, r_j);
        icg.move(r_i, icg.loadImmediate(3));
        icg.beginElseStatement(true);
        icg.move(r_j, icg.loadImmediate(4));
        icg.endIfStatement();
        icg.beginElseStatement(false);
        icg.endIfStatement();
            

        // switch (i) { case 3: case 4: j = 4; break; case 5: j = 5; break; default : j = 6; }
        icg.beginSwitchStatement(pos, r_i);
        // case 3, note empty case statement (?necessary???)
        icg.endCaseCondition(icg.loadImmediate(3));
        icg.beginCaseStatement();
        icg.endCaseStatement();
        // case 4
        icg.endCaseCondition(icg.loadImmediate(4));
        icg.beginCaseStatement();
        icg.beginStatement(pos);
        icg.move(r_j, icg.loadImmediate(4));
        icg.breakStatement();
        icg.endCaseStatement();
        // case 5
        icg.endCaseCondition(icg.loadImmediate(5));
        icg.beginCaseStatement();
        icg.beginStatement(pos);
        icg.move(r_j, icg.loadImmediate(5));
        icg.breakStatement();
        icg.endCaseStatement();
        // default
        icg.beginDefaultStatement();
        icg.beginStatement(pos);
        icg.move(r_j, icg.loadImmediate(6));
        icg.endDefaultStatement();
        icg.endSwitchStatement();
            
        // for ( ; i; i = i + 1 ) j = 99;
        icg.beginForStatement(pos);
        icg.forCondition(r_i);
        icg.move(r_i, icg.op(ADD, r_i, icg.loadImmediate(1)));
        icg.forIncrement();
        icg.move(r_j, icg.loadImmediate(99));
        icg.endForStatement();
            
        ICodeModule *icm = icg.complete();
            
        stdOut << icg;
            
        delete icm;
    }

    static float64 testFunctionCall(World &world, float64 n)
    {
        JSObject glob;
        Context cx(world, glob);
        uint32 position = 0;
        //StringAtom& global = world.identifiers[widenCString("global")];
        StringAtom& sum = world.identifiers[widenCString("sum")];
            
        ICodeGenerator fun;
        // function sum(n) { if (n > 1) return 1 + sum(n - 1); else return 1; }
        // n is bound to var #0.
        Register r_n =
            fun.allocateVariable(world.identifiers[widenCString("n")]);
        fun.beginStatement(position);
        Register r1 = fun.op(COMPARE_GT, r_n, fun.loadImmediate(1.0));
        fun.beginIfStatement(position, r1);
        fun.beginStatement(position);
        r1 = fun.op(SUBTRACT, r_n, fun.loadImmediate(1.0));
        RegisterList args(1);
        args[0] = r1;
        r1 = fun.call(fun.loadName(sum), args);
        fun.returnStatement(fun.op(ADD, fun.loadImmediate(1.0), r1));
        fun.beginElseStatement(true);
        fun.beginStatement(position);
        fun.returnStatement(fun.loadImmediate(1.0));
        fun.endIfStatement();
            
        ICodeModule *funCode = fun.complete();
        stdOut << fun;
            
        // now a script : 
        // return sum(n);
        ICodeGenerator script;
        script.beginStatement(position);
        r1 = script.loadName(sum);
        RegisterList args_2(1);
        args_2[0] = script.loadImmediate(n);
        script.returnStatement(script.call(r1, args_2));
            
        stdOut << script;
            
        // preset the global property "sum" to contain the above function
        glob.defineFunction(sum, funCode);
            
        JSValue result = cx.interpret(script.complete(), JSValues());
        stdOut << "sum(" << n << ") = " << result.f64 << "\n";
            
        return result.f64;    
    }
        
    static float64 testFactorial(World &world, float64 n)
    {
        JSObject glob;
        Context cx(world, glob);
        // generate code for factorial, and interpret it.
        uint32 position = 0;
        ICodeGenerator icg;
            
        // fact(n) {
        // var result = 1;
        Register r_n = icg.allocateVariable(world.identifiers[widenCString("n")]);
        Register r_result = icg.allocateVariable(world.identifiers[widenCString("result")]);

        icg.beginStatement(position);
        icg.move(r_result, icg.loadImmediate(1.0));
            
        // while (n > 1) {
        //   result = result * n;
        //   n = n - 1;
        // }
        {
            icg.beginWhileStatement(position);
            Register r1 = icg.loadImmediate(1.0);
            Register r2 = icg.op(COMPARE_GT, r_n, r1);
            icg.endWhileExpression(r2);
            r2 = icg.op(MULTIPLY, r_result, r_n);
            icg.move(r_result, r2);
            icg.beginStatement(position);
            r1 = icg.loadImmediate(1.0); 
            r2 = icg.op(SUBTRACT, r_n, r1);
            icg.move(r_n, r2);
            icg.endWhileStatement();
        }
            
        // return result;
        icg.returnStatement(r_result);
        ICodeModule *icm = icg.complete();
        stdOut << icg;
            
        // preset the global property "fact" to contain the above function
        StringAtom& fact = world.identifiers[widenCString("fact")];
        glob.defineFunction(fact, icm);
            
        // now a script : 
        // return fact(n);
        ICodeGenerator script;
        script.beginStatement(position);
        RegisterList args(1);
        args[0] = script.loadImmediate(n);
        script.returnStatement(script.call(script.loadName(fact), args));
        stdOut << script;
            
        // test the iCode interpreter.
        JSValue result = cx.interpret(script.complete(), JSValues());
        stdOut << "fact(" << n << ") = " << result.f64 << "\n";
            
        delete icm;
            
        return result.f64;
    }
        
    static float64 testObjects(World &world, int32 n)
    {
        JSObject glob;
        Context cx(world, glob);
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
        initCG.returnStatement();
            
        ICodeModule* initCode = initCG.complete();
            
        stdOut << initCG;
        
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
            
        stdOut << incrCG;
            
        // run initialization code.
        JSValues args;
        cx.interpret(initCode, args);
            
        // call the increment function some number of times.
        JSValue result;
        while (n-- > 0)
            result = cx.interpret(incrCode, args);
            
        stdOut << "result = " << result.f64 << "\n";
            
        delete initCode;
        delete incrCode;
            
        return result.f64;
    }

} /* namespace Shell */
} /* namespace JavaScript */

    
int main(int /* argc */, char /* **argv */)
{
#if defined(XP_MAC) && !defined(XP_MAC_MPW)
    initConsole("\pJavaScript Shell", "Welcome to the js2 shell.\n", argc, argv);
#endif
	JavaScript::World world;
#if 1
    assert(JavaScript::Shell::testFactorial(world, 5) == 120);
    assert(JavaScript::Shell::testObjects(world, 5) == 5);
    //    testICG(world);
    assert(JavaScript::Shell::testFunctionCall(world, 5) == 5);
#endif
	JavaScript::Shell::readEvalPrint(stdin, world);
    return 0;
    // return ProcessArgs(argv + 1, argc - 1);
}
