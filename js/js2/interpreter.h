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
    
    enum InterpretStage {
        STEP,
        CATCH,
        TRAP
    };
    
    struct Linkage;
    
    class Context : public gc_base {
    public:
        explicit Context(World& world, JSScope* aGlobal)
            : mWorld(world), mGlobal(aGlobal), mLinkage(0), mActivation(0) {}

        JSScope* getGlobalObject() { return mGlobal; }

        JSScope* setGlobalObject(JSScope* aGlobal)
        {
            JSScope* oldGlobal = mGlobal;
            mGlobal = aGlobal;
            return oldGlobal;
        }

        InstructionIterator getPC() 
        {
            return mPc;
        }
        
        JSValues &getRegisters();
        
        ICodeModule *getICode()
        {
            return mICode;
        }

        class Listener {
        public:
            virtual void listen(Context *context, InterpretStage Stage) = 0;
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

    private:
        void broadcast(InterpretStage Stage);

    private:
        World& mWorld;
        JSScope* mGlobal;
        Linkage* mLinkage;
        std::vector<Listener*> mListeners;
        Activation* mActivation;
        ICodeModule *mICode;
        InstructionIterator mPc;
        
    }; /* class Context */

} /* namespace Interpreter */
} /* namespace JavaScript */

#endif /* interpreter_h */
