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
            : mWorld(world), mGlobal(aGlobal), mLinkage(0), mActivation(0), mHasOperatorsPackageLoaded(false), mCurrentClosure(0) { initContext(); }

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

        void loadClass(const char *fileName);

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
        ICodeModule* mICode;
        bool mHasOperatorsPackageLoaded;
        JSClosure* mCurrentClosure;

        InstructionIterator mPC;
        
        BinaryOperatorList mBinaryOperators[BinaryOperator::BinaryOperatorCount];

    }; /* class Context */

    /**
    * 
    */
    struct Handler: public gc_base {
        Handler(Label *catchLabel, Label *finallyLabel)
            : catchTarget(catchLabel), finallyTarget(finallyLabel) {}
        Label *catchTarget;
        Label *finallyTarget;
    };
    typedef std::vector<Handler *> CatchStack;


    /**
     * Represents the current function's invocation state.
     */
    struct Activation : public JSObject {
        JSValues mRegisters;
        CatchStack catchStack;
        
        Activation(uint32 highRegister, const JSValues& args)
            : mRegisters(highRegister + 1)
        {
            // copy arg list to initial registers.
            JSValues::iterator dest = mRegisters.begin();
            for (JSValues::const_iterator src = args.begin(), 
                     end = args.end(); src != end; ++src, ++dest) {
                *dest = *src;
            }
        }

        Activation(uint32 highRegister, Activation* caller, const JSValue thisArg,
                     const ArgumentList* list)
            : mRegisters(highRegister + 1)
        {
            // copy caller's parameter list to initial registers.
            JSValues::iterator dest = mRegisters.begin();
            *dest++ = thisArg;
            const JSValues& params = caller->mRegisters;
            for (ArgumentList::const_iterator src = list->begin(), 
                     end = list->end(); src != end; ++src, ++dest) {
                Register r = (*src).first.first;
                if (r != NotARegister)
                    *dest = params[r];
                else
                    *dest = JSValue(JSValue::uninitialized_tag);
            }
        }

        // calling a binary operator, no 'this'
        Activation(uint32 highRegister, const JSValue thisArg, const JSValue arg1, const JSValue arg2)
            : mRegisters(highRegister + 1)
        {
            mRegisters[0] = thisArg;
            mRegisters[1] = arg1;
            mRegisters[2] = arg2;
        }

        // calling a getter function, no arguments
        Activation(uint32 highRegister, const JSValue thisArg)
            : mRegisters(highRegister + 1)
        {
            mRegisters[0] = thisArg;
        }

        // calling a setter function, 1 argument
        Activation(uint32 highRegister, const JSValue thisArg, const JSValue arg)
            : mRegisters(highRegister + 1)
        {
            mRegisters[0] = thisArg;
            mRegisters[1] = arg;
        }

    };  /* struct Activation */

} /* namespace Interpreter */
} /* namespace JavaScript */

#endif /* interpreter_h */
