// -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// The contents of this file are subject to the Netscape Public
// License Version 1.1 (the "License"); you may not use this file
// except in compliance with the License. You may obtain a copy of
// the License at http://www.mozilla.org/NPL/
//
// Software distributed under the License is distributed on an "AS
// IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
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

#ifdef _WIN32
#include "msvc_pragma.h"
#endif

#define EXITCODE_RUNTIME_ERROR 3

#include <algorithm>
#include <assert.h>

#include "world.h"
#include "utilities.h"
#include "js2value.h"

#include <map>
#include <algorithm>
#include <list>
#include <stack>

#include "reader.h"
#include "parser.h"
#include "regexp.h"
#include "js2engine.h"
#include "bytecodecontainer.h"
#include "js2metadata.h"

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

JavaScript::World world;
JavaScript::Arena a;

namespace JavaScript {
namespace Shell {

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


/* "filename" of the console */
const String ConsoleName = widenCString("<console>");
const bool showTokens = false;

#define INTERPRET_INPUT 1
//#define SHOW_ICODE 1

MetaData::JS2Metadata *metadata;


static int readEvalPrint(FILE *in)
{
    int result = 0;
    String buffer;
    string line;
    LineReader inReader(in);
    while (promptLine(inReader, line, buffer.empty() ? "ep> " : "> ")) {
        appendChars(buffer, line.data(), line.size());
        try {
            Pragma::Flags flags = Pragma::es4;
            Parser p(world, a, flags, buffer, ConsoleName);
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
                if (metadata->showTrees)
                {
                    PrettyPrinter f(stdOut, 80);
                    {
                        PrettyPrinter::Block b(f, 2);
                        f << "Program =";
                        f.linearBreak(1);
                        StmtNode::printStatements(f, parsedStatements);
                    }
                    f.end();
                    stdOut << '\n';
                }
                if (parsedStatements) {
                    MetaData::CompilationData *oldData = metadata->startCompilationUnit(NULL, buffer, ConsoleName);            
                    metadata->ValidateStmtList(parsedStatements);
                    js2val rval = metadata->ExecuteStmtList(RunPhase, parsedStatements);
                    if (!JS2VAL_IS_VOID(rval))
                        stdOut << *metadata->toString(rval) << '\n';
                    metadata->restoreCompilationUnit(oldData);
                }
            }
            clear(buffer);
        } catch (Exception &e) {
            // If we got a syntax error on the end of input, then wait for a continuation
            // of input rather than printing the error message.
            if (!(e.hasKind(Exception::syntaxError) && e.lineNum && e.pos == buffer.size() &&
                  e.sourceFile == ConsoleName)) {
                stdOut << '\n' << e.fullMessage();
                clear(buffer);
                result = EXITCODE_RUNTIME_ERROR;
            }
        }
    }
    stdOut << '\n';
    return result;
}

static bool processArgs(int argc, char **argv, int *result, bool *doTrees, bool *doTrace)
{
    bool doInteractive = true;
    *doTrees = false;
    *doTrace = false;
    for (int i = 0; i < argc; i++)  {    
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            default:
                stdOut << "unrecognized command line switch\n";
                i = argc;
                break;
            case 't':
                *doTrees = true;
                break;
            case 'x':
                *doTrace = true;
                break;
            case 'i':
                doInteractive = true;
                break;
            case 'f':
                {
                    try {
                        metadata->readEvalFile(JavaScript::widenCString(argv[++i]));
                    } catch (Exception &e) {
                        stdOut << '\n' << e.fullMessage();
                        *result = EXITCODE_RUNTIME_ERROR;
                        return false;
                    }
                    doInteractive = false;
                }
                break;
            }
        }
        else {
            if ((argv[i][0] == '/') && (argv[i][1] == '/')) {
                // skip rest of command line
                break;
            }
        }
    }
    return doInteractive;
}

using namespace MetaData;

js2val print(JS2Metadata * /* meta */, const js2val /* thisValue */, js2val argv[], uint32 argc)
{
    for (uint32 i = 0; i < argc; i++) {
        stdOut << *metadata->toString(argv[i]) << '\n';
    }
    return JS2VAL_UNDEFINED;
}

#ifdef DEBUG
js2val trees(JS2Metadata *meta, const js2val /* thisValue */, js2val /* argv */ [], uint32 /* argc */)
{
    meta->showTrees = !meta->showTrees;
    return JS2VAL_UNDEFINED;
}

js2val trace(JS2Metadata *meta, const js2val /* thisValue */, js2val /* argv */ [], uint32 /* argc */)
{
    meta->engine->traceInstructions = !meta->engine->traceInstructions;
    return JS2VAL_UNDEFINED;
}

void printFrameBindings(Frame *f)
{
    stdOut << " Static Bindings:\n";                    
    for (StaticBindingIterator rsb = f->staticReadBindings.begin(), rsend = f->staticReadBindings.end(); (rsb != rsend); rsb++) {
        stdOut << "\t" << *rsb->second->qname.nameSpace->name << "::" << *rsb->second->qname.id;
        bool found = false;
        for (StaticBindingIterator wsb = f->staticWriteBindings.begin(), wsend = f->staticWriteBindings.end(); (wsb != wsend); wsb++) {
            if (rsb->second->qname == wsb->second->qname) {
                found = true;
                break;
            }
        }
        stdOut << ((found) ? " [read/write]" : " [read-only]") << "\n";
    }
    for (StaticBindingIterator wsb = f->staticWriteBindings.begin(), wsend = f->staticWriteBindings.end(); (wsb != wsend); wsb++) {
        bool found = false;
        for (StaticBindingIterator rsb = f->staticReadBindings.begin(), rsend = f->staticReadBindings.end(); (rsb != rsend); rsb++) {
            if (rsb->second->qname == wsb->second->qname) {
                found = true;
                break;
            }
        }
        if (!found)
            stdOut << "\t" << *wsb->second->qname.nameSpace->name << "::" << *wsb->second->qname.id << " [write-only]" << "\n";
    }

}

js2val dump(JS2Metadata *meta, const js2val /* thisValue */, js2val argv[], uint32 argc)
{
    if (argc) {
        if (JS2VAL_IS_OBJECT(argv[0])) {
            JS2Object *fObj = JS2VAL_TO_OBJECT(argv[0]);
            if (((fObj->kind == CallableInstanceKind)
                        && (meta->objectType(argv[0]) == meta->functionClass))) {
                FunctionWrapper *fWrap;
                fWrap = (checked_cast<CallableInstance *>(fObj))->fWrap;
                if (fWrap->code)
                    stdOut << "<native code>\n";
                else
                    dumpBytecode(fWrap->bCon);
            }
            else {
                if (fObj->kind == ClassKind) {
                    JS2Class *c = checked_cast<JS2Class *>(fObj);
                    stdOut << "class " << *c->getName();
                    if (c->super)
                        stdOut << " extends " << *c->super->getName();
                    stdOut << "\n";
                    stdOut << ((c->dynamic) ? " dynamic, " : " non-dynamic, ") << ((c->final) ? "final" : "non-final") << "\n";
                    stdOut << " slotCount = " << c->slotCount << "\n";

                    printFrameBindings(c);

                    stdOut << " Instance Bindings:\n";                    
                    for (InstanceBindingIterator rib = c->instanceReadBindings.begin(), riend = c->instanceReadBindings.end(); (rib != riend); rib++) {
                        stdOut << "\t" << *rib->second->qname.nameSpace->name << "::" << *rib->second->qname.id;
                        bool found = false;
                        for (InstanceBindingIterator wib = c->instanceWriteBindings.begin(), wiend = c->instanceWriteBindings.end(); (wib != wiend); wib++) {
                            if (rib->second->qname == wib->second->qname) {
                                found = true;
                                break;
                            }
                        }
                        stdOut << ((found) ? " [read/write]" : " [read-only]") << "\n";
                    }
                    for (InstanceBindingIterator wib = c->instanceWriteBindings.begin(), wiend = c->instanceWriteBindings.end(); (wib != wiend); wib++) {
                        bool found = false;
                        for (InstanceBindingIterator rib = c->instanceReadBindings.begin(), riend = c->instanceReadBindings.end(); (rib != riend); rib++) {
                            if (rib->second->qname == wib->second->qname) {
                                found = true;
                                break;
                            }
                        }
                        if (!found)
                            stdOut << "\t" << *wib->second->qname.nameSpace->name << "::" << *wib->second->qname.id << " [write-only]" << "\n";
                    }
                }
                else {
                    if (fObj->kind == PrototypeInstanceKind) {
                        PrototypeInstance *pInst = checked_cast<PrototypeInstance *>(fObj);
                        stdOut << "Prototype Instance\n";
                        if (pInst->parent)
                            printFormat(stdOut, "Parent @ 0x%08X\n", pInst->parent);
                        else
                            stdOut << " Null Parent\n";
                        if (pInst->type)
                            stdOut << " Type: " << *pInst->type->getName() << "\n";
                        else
                            stdOut << " Null Type\n";

                        stdOut << " Dynamic Properties:\n";                    
                        for (DynamicPropertyIterator dpi = pInst->dynamicProperties.begin(), dpend = pInst->dynamicProperties.end(); (dpi != dpend); dpi++) {
                            stdOut << "\t" << dpi->first << " = " << *meta->toString(dpi->second) << "\n";
                        }
                    }
                }
            }
        }
    }
    return JS2VAL_UNDEFINED;
}
#endif

js2val load(JS2Metadata *meta, const js2val /* thisValue */, js2val argv[], uint32 argc)
{
    if (argc) {
        // Save off the current top frame and root it.
        Environment *curEnv = meta->env;
        JS2Object::RootIterator ri = JS2Object::addRoot(&curEnv);
        // Set the environment to global object and system frame so that the
        // load happens into the top frame.
        meta->env = new Environment(curEnv->getSystemFrame(), curEnv->getPackageOrGlobalFrame());
        
        js2val result = JS2VAL_UNDEFINED;
        try {
            result = meta->readEvalFile(*meta->toString(argv[0]));
        }
        catch (Exception x) {
            meta->env = curEnv;
            JS2Object::removeRoot(ri);
            throw x;
        }
        meta->env = curEnv;
        JS2Object::removeRoot(ri);
        return result;
    }
    else
        return JS2VAL_UNDEFINED;
}

} /* namespace Shell */
} /* namespace JavaScript */

int main(int argc, char **argv)
{
    using namespace JavaScript;
    using namespace Shell;
    
#if defined(XP_MAC) && !defined(XP_MAC_MPW)
    initConsole("\pJavaScript Shell", "Welcome to Epimetheus.\n", argc, argv);
#else
    stdOut << "Welcome to Epimetheus.\n";
#endif

    metadata = new MetaData::JS2Metadata(world);
    metadata->addGlobalObjectFunction("print", print);
    metadata->addGlobalObjectFunction("load", load);
#ifdef DEBUG
    metadata->addGlobalObjectFunction("dump", dump);
    metadata->addGlobalObjectFunction("trees", trees);
    metadata->addGlobalObjectFunction("trace", trace);
#endif

    try {
        bool doInteractive = true;
        int result = 0;
        if (argc > 1) {
            doInteractive = processArgs(argc - 1, argv + 1, &result, &metadata->showTrees, &metadata->engine->traceInstructions);
        }
        if (doInteractive)
            result = readEvalPrint(stdin);

        return result;
    }
    catch (Exception &e) {
        stdOut << '\n' << e.fullMessage();
        return EXITCODE_RUNTIME_ERROR;
    }
}
