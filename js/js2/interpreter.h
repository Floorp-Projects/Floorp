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
// Copyright (C) 2000 Netscape Communications Corporation. All
// Rights Reserved.

#ifndef interpreter_h
#define interpreter_h

#include "utilities.h"
#include "jstypes.h"
#include "vmtypes.h"
#include "icodegenerator.h"
#include "gc_allocator.h"

namespace JavaScript {
namespace Interpreter {

    using namespace ICG;
    using namespace JSTypes;

    struct Activation;
    
    struct Linkage;

    class Context : public gc_base {
        void initContext();
    public:
        explicit Context(World& world, JSScope* aGlobal)
            : mWorld(world), mGlobal(aGlobal), mLinkage(0), mActivation(0), mHasOperatorsPackageLoaded(false) { initContext(); }

        World& getWorld()           { return mWorld; }
        JSScope* getGlobalObject()  { return mGlobal; }
        InstructionIterator getPC() { return mPC; }
        
        JSValues& getRegisters();
        ICodeModule* getICode();

        enum Event {
            EV_NONE     = 0x0000,
            EV_STEP     = 0x0001,
            EV_THROW    = 0x0002,
            EV_DEBUG    = 0x0004,
            EV_ALL      = 0xffff
        };
    
        class Listener {
        public:
            virtual void listen(Context *context, Event event) = 0;
        };
    
        void addListener(Listener* listener);
        void removeListener(Listener* listener);
    
        class Frame {
        public:
            virtual Frame* getNext() = 0;
            virtual void getState(InstructionIterator& pc, JSValues*& registers,
                                  ICodeModule*& iCode) = 0;
        };
    
        Frame* getFrames();

        JSValue interpret(ICodeModule* iCode, const JSValues& args);
        void doCall(JSFunction *target, Instruction *pc);

        ICodeModule* compileFunction(const String &source);
        ICodeModule* genCode(StmtNode *p, const String &fileName);
        JSValue readEvalFile(FILE* in, const String& fileName);

        void addBinaryOperator(BinaryOperator::BinaryOp op, BinaryOperator *fn) { mBinaryOperators[op].push_back(fn); }
        const JSValue findBinaryOverride(JSValue &operand1, JSValue &operand2, BinaryOperator::BinaryOp op);


        bool hasOperatorsPackageLoaded()    { return mHasOperatorsPackageLoaded; }

    private:
        void broadcast(Event event);
        void initOperatorsPackage();
        bool hasNamedArguments(ArgumentList &args);

    private:
        World& mWorld;
        JSScope* mGlobal;
        Linkage* mLinkage;
        typedef std::vector<Listener*, gc_allocator<Listener*> > ListenerList;
        typedef ListenerList::iterator ListenerIterator;
        ListenerList mListeners;
        Activation* mActivation;
        bool mHasOperatorsPackageLoaded;

        InstructionIterator mPC;
        
        BinaryOperatorList mBinaryOperators[BinaryOperator::BinaryOperatorCount];

    }; /* class Context */

} /* namespace Interpreter */
} /* namespace JavaScript */

#endif /* interpreter_h */
