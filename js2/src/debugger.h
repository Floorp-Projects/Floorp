/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
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

namespace JavaScript {
namespace Debugger {

    typedef void (debuggerCallback) (Context *aContext, ICodeDebugger *aICD);
    
    class Breakpoint {
        /* representation of a breakpoint */
        void set();
        void clear();
        bool getState();
        InstructionIterator getPC();
    };
    
    class ICodeDebugger {
    public:
        /**
         * set a debugger callback, returning the old one
         */
        static debuggerCallback * setCallback (debuggerCallback *aCallback);
        
        /**
         * install an icdebugger on a context
         */
        static bool attachToContext (Context *aContext);

        /**
         * detach an icdebugger from a context
         */
        static bool detachFromContext (Context *aContext);

        enum DebuggerAction {
            /* step to next instruction */
            STEP, 
            /* step until current block returns or execution completes */
            STEP_OUT,
            /* execute next instruction, treat CALLs as a single instruction */
            NEXT,
            /* resume normal execution */
            CONT,
            /* restart the program from the top */
            RUN,
            /* clear current execution, prepare to step from top */
            KILL
        };

        void setNextAction (DebuggerAction aAction);

        /**
         * evaluate an expression within the current execution state
         */
        JSValue evaluate (ICodeModule *aICode);

        /**
         * create a break at a desired pc
         */
        Breakpoint *createBreakpoint (InstructionIterator aPC);
        /**
         * destroy a breakpoint
         */
        void destroyBreakpoint (Breakpoint *aBreak);        

        /**
         * get the current pc
         */
        InstructionIterator getPC() const;
        /**
         * set next statement to a desired pc, return the last pc
         */
        InstructionIterator setNextStatement(InstructionIterator aPC);
        
    }; /* class ICodeDebugger */

} /* namespace Debugger */
} /* namespace JavaScript */

