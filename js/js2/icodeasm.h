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


#ifdef _WIN32	// Microsoft Visual C++ 6.0 whines about name lengths over 255 getting truncated in the browser database
#pragma warning( disable : 4786) 
#endif


#ifndef icodeasm_h___
#define icodeasm_h___

#include <string>
#include <iterator>

#include "vmtypes.h"
#include "jstypes.h"
#include "interpreter.h"

namespace JavaScript {
namespace ICodeASM {

    enum OperandType {
        otNone = 0,
        otArgumentList,
        otBinaryOp,
        otBool,
        otDouble,
        otICodeModule,
        otJSClass,
        otJSString,
        otJSFunction,
        otJSType,
        otLabel,
        otUInt32,
        otRegister,
        otStringAtom
    };

    struct AnyOperand {
        OperandType type;
        int64 data;
        /*void *data;*/
    };
    
    struct StatementNode {
        uint icodeID;
        AnyOperand operand[4];
    };
    
    class ICodeParser 
    {
    private:
        ICodeParser(const ICodeParser&); /* No copy constructor */
        
        Interpreter::Context *mCx;
        uint32 mInstructionCount;
        VM::LabelList mLabels; /* contains both named *and* unnamed labels */
        typedef std::map<const char *, VM::Label*> LabelMap;
        LabelMap mNamedLabels;
        
    public:
        uint32 mMaxRegister;
        VM::InstructionStream *mInstructions;
        
    public:
        ICodeParser (Interpreter::Context *aCx) : mCx(aCx), mInstructions(0) {}
        void parseSourceFromString (const string8 &source);

        /* operand parse functions;  These functions take care of finding
         * the start of the token with |SeekTokenStart|, and checking the
         * "estimation" (explicit checking takes care of |begin| == |end|,
         * aka EOF, because EOF is a token estimate.)  Once the start of the
         * token is found, and it is of the expected type, the actual parsing is
         * carried out by one of the general purpose parse functions.
         */
        string8_citer
        parseArgumentListOperand (string8_citer begin, string8_citer end,
                                  VM::ArgumentList **rval);
        string8_citer
        parseBinaryOpOperand (string8_citer begin, string8_citer end,
                              VM::BinaryOperator::BinaryOp *rval);
        string8_citer
        parseBoolOperand (string8_citer begin, string8_citer end,
                          bool *rval);
        string8_citer
        parseDoubleOperand (string8_citer begin, string8_citer end,
                            double *rval);
        string8_citer
        parseICodeModuleOperand (string8_citer begin, string8_citer end,
                                 string8 **rval);
        string8_citer
        parseJSClassOperand (string8_citer begin, string8_citer end,
                             string8 **rval);
        string8_citer
        parseJSStringOperand (string8_citer begin, string8_citer end,
                              JSTypes::JSString **rval);
        string8_citer
        parseJSFunctionOperand (string8_citer begin, string8_citer end,
                                string8 **rval);
        string8_citer
        parseJSTypeOperand (string8_citer begin, string8_citer end,
                            JSTypes::JSType **rval);
        string8_citer
        parseLabelOperand (string8_citer begin, string8_citer end,
                           VM::Label **rval);
        string8_citer
        parseUInt32Operand (string8_citer begin, string8_citer end,
                            uint32 *rval);
        string8_citer
        parseRegisterOperand (string8_citer begin, string8_citer end,
                              JSTypes::Register *rval);
        string8_citer
        parseStringAtomOperand (string8_citer begin, string8_citer end,
                                StringAtom **rval);

        /* "high level" parse functions */
        string8_citer
        parseInstruction (uint icodeID, string8_citer start,
                          string8_citer end);
        string8_citer
        parseNextStatement (string8_citer begin, string8_citer end);

    };
    
}
}

#endif /* #ifndef icodeasm_h___ */
