/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* this is all vapor, don't take it to serious yet */

#ifndef debugger_h
#define debugger_h

#include "utilities.h"
#include "interpreter.h"

#include <stdio.h>

namespace JavaScript {
namespace Debugger {

    using namespace Interpreter;

    class Shell;
    
    typedef const Reader *ResolveFileCallback (const String &fileName);
    typedef bool DebuggerCommandCallback (Shell &debugger, const Lexer &lex);

    class Breakpoint {
    public:
        /* representation of a breakpoint */
        void set();
        void clear();
        bool getState();
        InstructionIterator getPC();
    };    

    struct DebuggerCommand
    {
        DebuggerCommand(String aName, String aParamDesc, String aShortHelp,
                        String aLongHelp = widenCString("No more help available."),
                        DebuggerCommandCallback *aCommandFunction = 0) 
            : mName(aName), mParamDesc(aParamDesc), mShortHelp(aShortHelp),
              mLongHelp(aLongHelp), mCommandFunction(aCommandFunction) {}

        String mName;
        String mParamDesc;
        String mShortHelp;
        String mLongHelp;
        DebuggerCommandCallback *mCommandFunction;
    };

    /* keep in sync with list in debugger.cpp */
    enum ShellCommand {
        ASSEMBLE,
        AMBIGUOUS,
        AMBIGUOUS2,
        CONTINUE,
        DISSASSEMBLE,
        EXIT,
        HELP,
        ISTEP,
        LET,
        PRINT,
        REGISTER,
        STEP,
        COMMAND_COUNT
    };
        
    class Shell : public Context::Listener {
    public:        
        Shell (World &aWorld, FILE *aIn, Formatter &aOut, Formatter &aErr,
               ResolveFileCallback *aCallback = 0) :
            mWorld(aWorld), mIn(aIn), mOut(aOut), mErr(aErr),
            mResolveFileCallback(aCallback), mStopMask(Context::EV_DEBUG),
            mTraceSource(false), mTraceICode(false), mLastSourcePos(0),
            mLastICodeID(NotABanana), mLastCommand(COMMAND_COUNT)
        {
        }

        ~Shell ()
        {
        }
        
        ResolveFileCallback
            *setResolveFileCallback (ResolveFileCallback  *aCallback)
        {
            ResolveFileCallback *rv = mResolveFileCallback;
            mResolveFileCallback = aCallback;
            return rv;
        }
        
        void listen(Context *context, Context::Event event);
        
        /**
         * install on a context
         */
        bool attachToContext (Context *aContext)
        {
            aContext->addListener (this);
            return true;
        }
        
        /**
         * detach an icdebugger from a context
         */
        bool detachFromContext (Context *aContext)
        {
            aContext->removeListener (this);
            return true;
        }

        FILE *getIStream() { return mIn; }
        Formatter &getOStream() { return mOut; }
        Formatter &getEStream() { return mErr; }

    private:
        bool doCommand (Context *cx, const String &aSource);
        void doSetVariable (Lexer &lex);
        void doPrint (Context *cx, Lexer &lex);

        void showOpAtPC(Context* cx, InstructionIterator pc);
        void showSourceAtPC(Context* cx, InstructionIterator pc);
        
        World &mWorld;
        FILE *mIn;
        Formatter &mOut, &mErr;
        ResolveFileCallback *mResolveFileCallback;
        uint32 mStopMask;
        bool mTraceSource, mTraceICode;
        uint32 mLastSourcePos, mLastICodeID;
        ShellCommand mLastCommand;
    };    

} /* namespace Debugger */
} /* namespace JavaScript */

#endif /* debugger_h */
