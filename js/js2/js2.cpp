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


JavaScript::World world;
JavaScript::Debugger::Shell jsd(world, JavaScript::stdOut, JavaScript::stdOut);
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


#if 0
class Tracer : public Context::Listener {
    void listen(Context* context, InterpretStage stage)
    {
        static String lastLine (widenCString("\n"));
        String line;
        ICodeModule *iCode = context->getICode();
        InstructionIterator pc = context->getPC();

        stdOut << "jsd [pc:";
        printFormat (stdOut, "%04X", (pc - iCode->its_iCode->begin()));
        stdOut << ", reason:" << (uint)stage << "]> ";
        
        std::getline(cin, line);
        if (line.size() == 0)
            line = lastLine;
        else
        {
            line.append(widenCString("\n"));
            lastLine = line;
        }
        
        jsd.doCommand(context, line);
    }
};

#else
/**
 * Poor man's instruction tracing facility.
 */
class Tracer : public Context::Listener {
    typedef InstructionStream::difference_type InstructionOffset;
    void listen(Context* context, InterpretStage /*stage*/)
    {
        ICodeModule *iCode = context->getICode();
        JSValues &registers = context->getRegisters();
        InstructionIterator pc = context->getPC();
        
        
        InstructionOffset offset = (pc - iCode->its_iCode->begin());
        printFormat(stdOut, "%04X: ", offset);
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
};
#endif

static void testICG(World &world)
{
    //
    // testing ICG 
    //
    uint32 pos = 0;
    ICodeGenerator icg(&world, true, 1);
    
    // var i,j;
    // i is bound to var #0, j to var #1
    Register r_i = icg.allocateVariable(world.identifiers[widenCString("i")]);
    Register r_j = icg.allocateVariable(world.identifiers[widenCString("j")]);
    Register r_x = icg.allocateVariable(world.identifiers[widenCString("x")]);

    //  i = j + 2;
    icg.beginStatement(pos);
    Register r1 = icg.loadImmediate(2.0);
    icg.move(r_i, icg.op(ADD, r1, r_j));
    
    // j = a.b
    icg.beginStatement(pos);
    r1 = icg.loadName(world.identifiers[widenCString("a")]);
    r1 = icg.getProperty(r1, world.identifiers[widenCString("b")]);
    icg.move(r_j, r1);

    // label1 : while (i) { while (i) { i = i + j; break label1; } }
    icg.beginLabelStatement(pos, world.identifiers[widenCString("label1")]);            
    icg.beginWhileStatement(pos);
    icg.endWhileExpression(r_i);        

        icg.beginWhileStatement(pos);
        icg.endWhileExpression(r_i);        
        icg.move(r_i, icg.op(ADD, r_i, r_j));
        icg.breakStatement(pos, world.identifiers[widenCString("label1")]);
        icg.endWhileStatement();

    icg.endWhileStatement();
    icg.endLabelStatement();

    // if (i) if (j) i = 3; else j = 4;
    icg.beginIfStatement(pos, r_i);
    icg.beginIfStatement(pos, r_j);
    icg.move(r_i, icg.loadImmediate(3));
    icg.beginElseStatement(true);
    icg.move(r_j, icg.loadImmediate(4));
    icg.endIfStatement();
    icg.beginElseStatement(false);
    icg.endIfStatement();
    
    // try {
    //   if (i) if (j) i = 3; else j = 4;
    //   throw j;
    // } 
    // catch (x) {
    //  j = x;
    // }
    // finally {
    //  i = 5;
    // }
    icg.beginTryStatement(pos, true, true);    // hasCatch, hasFinally
        icg.beginIfStatement(pos, r_i);
        icg.beginIfStatement(pos, r_j);
        icg.move(r_i, icg.loadImmediate(3));
        icg.beginElseStatement(true);
        icg.beginStatement(pos);
        icg.move(r_j, icg.loadImmediate(4));
        icg.endIfStatement();
        icg.beginElseStatement(false);
        icg.endIfStatement();
        icg.throwStatement(pos, r_j);
    icg.endTryBlock();
        icg.beginCatchStatement(pos);
        icg.endCatchExpression(r_x);
        icg.beginStatement(pos);
        icg.move(r_j, r_x);
        icg.endCatchStatement();
        icg.beginFinallyStatement(pos);
        icg.beginStatement(pos);
        icg.move(r_i, icg.loadImmediate(5));
        icg.endFinallyStatement();
    icg.endTryStatement();
    

    // switch (i) { case 3: case 4: j = 4; break; case 5: j = 5; break; default : j = 6; }
    icg.beginSwitchStatement(pos, r_i);
    // case 3, note empty case statement (?necessary???)
    icg.endCaseCondition(icg.loadImmediate(3));
    icg.beginCaseStatement(pos);
    icg.endCaseStatement();
    // case 4
    icg.endCaseCondition(icg.loadImmediate(4));
    icg.beginCaseStatement(pos);
    icg.beginStatement(pos);
    icg.move(r_j, icg.loadImmediate(4));
    icg.breakStatement(pos);
    icg.endCaseStatement();
    // case 5
    icg.endCaseCondition(icg.loadImmediate(5));
    icg.beginCaseStatement(pos);
    icg.beginStatement(pos);
    icg.move(r_j, icg.loadImmediate(5));
    icg.breakStatement(pos);
    icg.endCaseStatement();
    // default
    icg.beginDefaultStatement(pos);
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
    JSScope glob;
    Context cx(world, &glob);
    Tracer t;
    cx.addListener(&t);
    
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
    JSScope glob;
    Context cx(world, &glob);
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
    
    // install a listener so we can trace execution of factorial.
    Tracer t;
    cx.addListener(&t);
    
    // test the iCode interpreter.
    JSValue result = cx.interpret(script.complete(), JSValues());
    stdOut << "fact(" << n << ") = " << result.f64 << "\n";
    
    delete icm;
        
    return result.f64;
}
    
static float64 testObjects(World &world, int32 n)
{
    JSScope glob;
    Context cx(world, &glob);
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

static float64 testProto(World &world, int32 n)
{
    JSScope glob;
    Context cx(world, &glob);

    Tracer t;
    cx.addListener(&t);
    
    // create some objects, put some properties, and retrieve them.
    uint32 position = 0;
    ICodeGenerator initCG;
    
    // var proto = new Object();
    StringAtom& proto = world.identifiers[widenCString("proto")];
    initCG.beginStatement(position);
    initCG.saveName(proto, initCG.newObject());
    
    // function increment()
    // {
    //   this.counter = this.counter + 1;
    // }
    ICodeGenerator incrCG;
    StringAtom& counter = world.identifiers[widenCString("counter")];
    
    incrCG.beginStatement(position);
    Register rthis = incrCG.allocateVariable(world.identifiers[widenCString("counter")]);
    Register rcounter = incrCG.getProperty(rthis, counter);
    incrCG.setProperty(rthis, counter, incrCG.op(ADD, rcounter, incrCG.loadImmediate(1.0)));
    incrCG.returnStatement();
    
    StringAtom& increment = world.identifiers[widenCString("increment")];
    ICodeModule* incrCode = incrCG.complete();
    glob.defineFunction(increment, incrCode);
    
    // proto.increment = increment;
    initCG.beginStatement(position);
    initCG.setProperty(initCG.loadName(proto), increment, initCG.loadName(increment));

    // var global = new Object();
    StringAtom& global = world.identifiers[widenCString("global")];
    initCG.beginStatement(position);
    initCG.saveName(global, initCG.newObject());
        
    // global.counter = 0;
    initCG.beginStatement(position);
    initCG.setProperty(initCG.loadName(global), counter, initCG.loadImmediate(0.0));
    
    // global.proto = proto;
    // initCG.beginStatement(position);
    // initCG.setProperty(initCG.loadName(global), proto, initCG.loadName(proto));
    initCG.returnStatement();
    
    ICodeModule* initCode = initCG.complete();
        
    stdOut << initCG;
            
    // run initialization code.
    JSValues args;
    cx.interpret(initCode, args);
    
    // objects now exist, do real prototype chain manipulation.
    JSObject* globalObject = glob.getVariable(global).object;
    globalObject->setPrototype(glob.getVariable(proto).object);
    
    // generate call to global.increment()
    ICodeGenerator callCG;
    callCG.beginStatement(position);
    RegisterList argList(1);
    Register rglobal = argList[0] = callCG.loadName(global);
    callCG.call(callCG.getProperty(rglobal, increment), argList);
    callCG.returnStatement();
    
    ICodeModule* callCode = callCG.complete();
    
    // call the increment method some number of times.
    while (n-- > 0)
        (void) cx.interpret(callCode, args);
    
    JSValue result = glob.getVariable(global).object->getProperty(counter);
    
    stdOut << "result = " << result.f64 << "\n";
        
    delete initCode;
    delete incrCode;
    delete callCode;
    
    return result.f64;
}

} /* namespace Shell */
} /* namespace JavaScript */


int main(int argc, char **argv)
{
#if defined(XP_MAC) && !defined(XP_MAC_MPW)
    initConsole("\pJavaScript Shell", "Welcome to the js2 shell.\n", argc, argv);
#endif

#if 1
    using namespace JavaScript;
    using namespace Shell;
    
    assert(testFactorial(world, 5) == 120);
    assert(testObjects(world, 5) == 5);
    assert(testProto(world, 5) == 5);
//    JavaScript::Shell::testICG(world);
     assert(testFunctionCall(world, 5) == 5);
#endif
    readEvalPrint(stdin, world);
    return 0;
    // return ProcessArgs(argv + 1, argc - 1);
}
