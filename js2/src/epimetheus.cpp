// -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//
// JS2 shell.
//

#ifdef _WIN32
#include "msvc_pragma.h"
#endif

#define EXITCODE_RUNTIME_ERROR 3

#include <algorithm>
#include <assert.h>
#include <list>
#include <stack>
#include <map>

#include "world.h"
#include "strings.h"
#include "utilities.h"
#include "js2value.h"
#include "reader.h"
#include "parser.h"
#include "js2engine.h"
#include "regexp.h"
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

JavaScript::World *world = new World();
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

MetaData::JS2Metadata *metadata = NULL;


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
            Parser p(*world, a, flags, buffer, ConsoleName);
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
                    MetaData::BytecodeContainer *bCon = metadata->bCon;
                    metadata->ValidateStmtList(parsedStatements);
                    js2val rval = metadata->ExecuteStmtList(RunPhase, parsedStatements);
                    if (!JS2VAL_IS_VOID(rval))
                        stdOut << *metadata->toString(rval) << '\n';
                    delete bCon;
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

#ifdef TRACE_DEBUG
js2val trace(JS2Metadata *meta, const js2val /* thisValue */, js2val /* argv */ [], uint32 /* argc */)
{
    meta->engine->traceInstructions = !meta->engine->traceInstructions;
    return JS2VAL_UNDEFINED;
}
#endif

#ifdef DEBUG
js2val trees(JS2Metadata *meta, const js2val /* thisValue */, js2val /* argv */ [], uint32 /* argc */)
{
    meta->showTrees = !meta->showTrees;
    return JS2VAL_UNDEFINED;
}

void accessAccess(AccessSet access)
{
    if (access & ReadAccess)
        if (access & WriteAccess)
            stdOut << " [read/write] ";
        else
            stdOut << " [read-only] ";
    else
        stdOut << " [write-only] ";
}

void printLocalBindings(LocalBindingMap *lMap, ValueList *frameSlots)
{
    stdOut << " Local Bindings:\n";   

    for (LocalBindingIterator bi(*lMap); bi; ++bi) {
        LocalBindingEntry &lbe = *bi;
        for (LocalBindingEntry::NS_Iterator i = lbe.begin(), end = lbe.end(); (i != end); i++) {
            LocalBindingEntry::NamespaceBinding ns = *i;
            stdOut << "\t" << ns.first->name << "::" << lbe.name;
            LocalMember *m = checked_cast<LocalMember *>(ns.second->content);
            switch (m->memberKind) {
            case Member::ForbiddenMember:
                {
                    stdOut << " forbidden\n";
                }
                break;
            case Member::GetterMember:
                {
                    Getter *g = checked_cast<Getter *>(m);
                    stdOut << " get" << ":" << g->type->name << "\n";
                }
                break;
            case Member::SetterMember:
                {
                    Setter *s = checked_cast<Setter *>(m);
                    stdOut << " set" << ":" << s->type->name << "\n";
                }
                break;
            case Member::DynamicVariableMember:
                {
                    DynamicVariable *dv = checked_cast<DynamicVariable *>(m);
                    accessAccess(ns.second->accesses);
                    stdOut << ((dv->sealed) ? "sealed " : "sealed ");
                    stdOut << *metadata->toString(dv->value) << "\n";
                }
                break;
            case Member::FrameVariableMember:
                {
                    FrameVariable *fv = checked_cast<FrameVariable *>(m);
                    accessAccess(ns.second->accesses);
                    stdOut << ((fv->sealed) ? "sealed " : "sealed ");
                    stdOut << *metadata->toString((*frameSlots)[fv->frameSlot]) << "\n";
                }
                break;
            case Member::VariableMember:
                {
                    Variable *v = checked_cast<Variable *>(m);
                    stdOut << ":" << v->type->name;
                    accessAccess(ns.second->accesses);
                    stdOut << ((v->immutable) ? "immutable " : "non-immutable ");
                    stdOut << *metadata->toString(v->value) << "\n";
                }
                break;
            }
        }
    }
}

void printInstanceVariables(JS2Class *c, Slot *slots)
{
    if (c->super)
        printInstanceVariables(c->super, slots);

    for (InstanceBindingIterator rib(c->instanceBindings); rib; ++rib) {
        InstanceBindingEntry &ibe = *rib;
        for (InstanceBindingEntry::NS_Iterator i = ibe.begin(), end = ibe.end(); (i != end); i++) {
            InstanceBindingEntry::NamespaceBinding ns = *i;
            if (ns.second->content->memberKind == Member::InstanceVariableMember) {
                InstanceVariable *iv = checked_cast<InstanceVariable *>(ns.second->content);
                stdOut << "\t" << ns.first->name << "::" << ibe.name << ":" << iv->type->name;
                accessAccess(ns.second->accesses);
                stdOut << *metadata->toString(slots[iv->slotIndex].value) << "\n";
            }
        }
    }
}

js2val dump(JS2Metadata *meta, const js2val /* thisValue */, js2val argv[], uint32 argc)
{
    if (argc) {
        if (JS2VAL_IS_UNDEFINED(argv[0]))
            stdOut << "Undefined\n";
        else
        if (JS2VAL_IS_NULL(argv[0]))
            stdOut << "Null\n";
        else
        if (JS2VAL_IS_OBJECT(argv[0])) {
            JS2Object *fObj = JS2VAL_TO_OBJECT(argv[0]);
            switch (fObj->kind) {
            case SimpleInstanceKind:
                {                
                    SimpleInstance *s = checked_cast<SimpleInstance *>(fObj);
                    stdOut << "SimpleInstance\n";
                    if (JS2VAL_IS_OBJECT(s->super))
                        printFormat(stdOut, "super = 0x%08X\n", s->super);
                    else
                        stdOut << "super = " << *metadata->toString(s->super) << '\n';
                    stdOut << ((s->sealed) ? "sealed " : "not-sealed ") << '\n';
                    stdOut << "type = " << s->type->name << '\n';
                    printLocalBindings(&s->localBindings, NULL);
                    stdOut << " Instance Bindings:\n";   
                    printInstanceVariables(s->type, s->fixedSlots);
                    if (meta->objectType(argv[0]) == meta->functionClass) {
                        FunctionWrapper *fWrap;
                        fWrap = (checked_cast<FunctionInstance *>(fObj))->fWrap;
                        if (fWrap->code)
                            stdOut << "<native code>\n";
                        else
                            dumpBytecode(fWrap->bCon);
                    }
                }
                break;
            case ClassKind:
                {
                    JS2Class *c = checked_cast<JS2Class *>(fObj);
                    stdOut << "class " << c->name;
                    if (c->super)
                        stdOut << " extends " << c->super->name;
                    stdOut << "\n";
                    stdOut << ((c->dynamic) ? " dynamic, " : " non-dynamic, ") << ((c->final) ? "final" : "non-final") << "\n";
                    stdOut << " slotCount = " << c->slotCount << "\n";
                    printLocalBindings(&c->localBindings, c->frameSlots);
                    stdOut << " Instance Bindings:\n";                    
                    for (InstanceBindingIterator rib(c->instanceBindings); rib; ++rib) {
                        InstanceBindingEntry &ibe = *rib;
                        for (InstanceBindingEntry::NS_Iterator i = ibe.begin(), end = ibe.end(); (i != end); i++) {
                            InstanceBindingEntry::NamespaceBinding ns = *i;
                            switch (ns.second->content->memberKind) {
                            case Member::InstanceVariableMember:
                                {
                                    InstanceVariable *iv = checked_cast<InstanceVariable *>(ns.second->content);
                                    stdOut << "\tVariable " << ns.first->name << "::" << ibe.name << ":" << iv->type->name;
                                    accessAccess(ns.second->accesses);
                                    stdOut << ((iv->immutable) ? " immutable, " : " non-immutable, ") << ((iv->final) ? "final, " : "non-final, ") << ((iv->enumerable) ? "enumerable, " : "non-enumerable, ") ;
                                    stdOut << "slot:" << iv->slotIndex << ", defaultValue:" << *metadata->toString(iv->defaultValue) << "\n";
                                }
                                break;
                            case Member::InstanceMethodMember:
                                {
                                    InstanceMethod *im = checked_cast<InstanceMethod *>(ns.second->content);
                                    stdOut << "\tMethod " << ns.first->name << "::" << ibe.name; // XXX << *iv->type; the signature
                                    accessAccess(ns.second->accesses);
                                    stdOut << ((im->final) ? "final, " : "non-final, ") << ((im->enumerable) ? "enumerable, " : "non-enumerable, ") ;
                                    printFormat(stdOut, "function = 0x%08X\n", im->fInst);
                                }
                                break;
							case Member::InstanceGetterMember:
								{
									InstanceGetter *g = checked_cast<InstanceGetter *>(ns.second->content);
									stdOut << "\t" << ns.first->name << "::" << ibe.name;
									stdOut << " get" << ":" << g->type->name << "\n";
								}
								break;
							case Member::InstanceSetterMember:
								{
									InstanceSetter *s = checked_cast<InstanceSetter *>(ns.second->content);
									stdOut << "\t" << ns.first->name << "::" << ibe.name;
									stdOut << " set" << ":" << s->type->name << "\n";
								}
								break;
                            }
                        }
                    }
                }
                break;
            case PackageKind:
                {
                    Package *pkg = checked_cast<Package *>(fObj);
                    stdOut << "Package\n";
                    if (JS2VAL_IS_OBJECT(pkg->super))
                        printFormat(stdOut, "super = 0x%08X\n", pkg->super);
                    else
                        stdOut << "super = " << *metadata->toString(pkg->super) << '\n';
                    stdOut << ((pkg->sealed) ? "sealed " : "not-sealed ") << '\n';
                    printLocalBindings(&pkg->localBindings, pkg->frameSlots);
                }
                break;
            default:
                stdOut << "<<unimplemented kind " << (int32)(fObj->kind) << ">>\n";
                break;
            }
        }
        else
            stdOut << *metadata->toString(argv[0]);
    }
    return JS2VAL_UNDEFINED;
}

js2val dumpAt(JS2Metadata *meta, const js2val /* thisValue */, js2val argv[], uint32 argc)
{
    if (JS2VAL_IS_INT(argv[0])) {
        argv[0] = OBJECT_TO_JS2VAL(JS2VAL_TO_INT(argv[0]));
        return dump(meta, JS2VAL_NULL, argv, argc);
    }
    return JS2VAL_VOID;
}

js2val forceGC(JS2Metadata *meta, const js2val /* thisValue */, js2val /* argv */ [], uint32 /* argc */)
{
    meta->gc();
    return JS2VAL_VOID;
}

#endif

js2val load(JS2Metadata *meta, const js2val /* thisValue */, js2val argv[], uint32 argc)
{
    if (argc) {
        // Save off the current top frame and root it.
        Environment *curEnv = meta->env;
        DEFINE_ROOTKEEPER(meta, rk, curEnv);
        // Set the environment to global object and system frame so that the
        // load happens into the top frame.
        meta->env = new Environment(curEnv->getSystemFrame(), curEnv->getPackageFrame());
        
        js2val result = JS2VAL_UNDEFINED;
        try {
            result = meta->readEvalFile(*meta->toString(argv[0]));
        }
        catch (Exception x) {
            meta->env = curEnv;
            throw x;
        }
        meta->env = curEnv;
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

    metadata = new MetaData::JS2Metadata(*world);
        delete metadata;
        world->identifiers.clear();
        delete world;

    world = new World();
    metadata = new MetaData::JS2Metadata(*world);

    metadata->addGlobalObjectFunction("print", print, 1);
    metadata->addGlobalObjectFunction("load", load, 1);
#ifdef DEBUG
    metadata->addGlobalObjectFunction("dump", dump, 1);
    metadata->addGlobalObjectFunction("dumpAt", dumpAt, 1);
    metadata->addGlobalObjectFunction("trees", trees, 0);
    metadata->addGlobalObjectFunction("gc", forceGC, 0);
#endif
#ifdef TRACE_DEBUG
    metadata->addGlobalObjectFunction("trace", trace, 0);
#endif
    try {
        bool doInteractive = true;
        int result = 0;
        if (argc > 1) {
            doInteractive = processArgs(argc - 1, argv + 1, &result, &metadata->showTrees, &metadata->engine->traceInstructions);
        }
        if (doInteractive)
            result = readEvalPrint(stdin);
        delete metadata;
        world->identifiers.clear();
        delete world;
        return result;
    }
    catch (Exception &e) {
        stdOut << '\n' << e.fullMessage();
        return EXITCODE_RUNTIME_ERROR;
    }
}
